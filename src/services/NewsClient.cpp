#include "services/NewsClient.h"
#include "config.h"
#include "core/DataLock.h"
#include "utils/TextUtils.h"
#include "core/DeviceConfig.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

String NewsClient::localTimeFromIso(const String& iso) {
  // Formato fijo ISO-8601 en UTC: 0123456789012345
  //                               2026-08-30T14:23:00Z
  if (iso.length() < 16 || iso[10] != 'T') return String("");

  int hh = iso.substring(11, 13).toInt();
  int mm = iso.substring(14, 16).toInt();

  hh += TZ_OFFSET_H;
  if (hh < 0)   hh += 24;
  if (hh >= 24) hh -= 24;

  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", hh, mm);
  return String(buf);
}

bool NewsClient::shouldRefresh() const {
  if (!_everTried) return true;
  // No martillar la API si el último intento fue recién
  if (millis() - _lastTryMs < API_RETRY_MS) return false;
  if (!_ok) return true; // falló: reintentar
  return millis() - _lastOkMs >= NEWS_CACHE_MS;
}

bool NewsClient::refresh() {
  _everTried = true;
  _lastTryMs = millis();

  String apiKey = deviceConfig.get("gnews");
  if (apiKey.length() == 0) {
    _lastError = "Falta la API key";
    Serial.println("[News] Sin API key. Cargala en http://desk-radar.local/config");
    return false;
  }

  // Valida contra el root de Let's Encrypt (TLS_ROOT_CA_PEM en config.h). Por
  // aca pasa la API key de GNews.
  WiFiClientSecure client;
  client.setCACert(TLS_ROOT_CA_PEM);

  HTTPClient http;
  char url[320];
  snprintf(url, sizeof(url),
    "%s?category=%s&lang=%s&country=%s&max=%d&apikey=%s",
    GNEWS_URL, NEWS_CATEGORY, NEWS_LANG, NEWS_COUNTRY, NEWS_MAX_ITEMS, apiKey.c_str());

  if (!http.begin(client, url)) {
    _lastError = "Sin conexion";
    Serial.println("[News] No se pudo iniciar la conexion");
    return false;
  }

  http.setTimeout(15000);

  int code = http.GET();
  if (code != 200) {
    // 401/403 = API key mal puesta o vencida; 429 = se acabo el cupo diario
    if (code == 401 || code == 403)      _lastError = "API key invalida";
    else if (code == 429)                _lastError = "Cupo diario agotado";
    else                                 _lastError = "HTTP " + String(code);
    Serial.printf("[News] Error pidiendo titulares, HTTP %d\n", code);
    http.end();
    return false;
  }

  // Ojo: hay que leer con getString(), NO con getStream(). GNews responde con
  // Transfer-Encoding: chunked y getStream() devuelve el socket crudo, con los
  // headers de chunk mezclados en el cuerpo; ArduinoJson ve "16c5\r\n{..." y
  // devuelve InvalidInput. getString() pasa por writeToStream(), que es el
  // unico que interpreta el chunked.
  //
  // Son 5 artículos ya recortados por el free tier: ~6 KB, entra cómodo.
  String payload = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);

  if (err) {
    // Mostramos el motivo concreto en pantalla: distinguir InvalidInput de
    // IncompleteInput o NoMemory ahorra horas de debug a ciegas.
    _lastError = String("JSON: ") + err.c_str();
    Serial.print("[News] Error parseando: ");
    Serial.println(err.c_str());
    Serial.print("[News] Payload recibido: ");
    Serial.println(payload.substring(0, 120));
    return false;
  }

  JsonArray articles = doc["articles"].as<JsonArray>();
  if (articles.isNull()) {
    _lastError = "Sin titulares";
    Serial.println("[News] La respuesta no trajo articles");
    return false;
  }

  // Se arma en un vector local y se publica de una sola vez bajo el candado.
  // Esto corre en la tarea de red (core 0) mientras el loop puede estar
  // dibujando la pantalla de noticias en el otro nucleo: un clear() seguido de
  // push_back() en vivo le reasignaria el vector abajo de los pies.
  std::vector<NewsItem> frescos;
  for (JsonObject a : articles) {
    NewsItem item;
    item.title  = TextUtils::toAscii(a["title"].as<String>());
    item.source = TextUtils::toAscii(a["source"]["name"].as<String>());
    item.time   = localTimeFromIso(a["publishedAt"].as<String>());
    // GNews manda un resumen de una o dos frases en "description". Es lo que
    // se lee al tocar el titular; el campo "content" del plan gratuito viene
    // cortado a la mitad con un "... [1234 chars]" pegado, asi que no sirve.
    item.summary = TextUtils::toAscii(a["description"].as<String>());

    if (item.title.length() == 0) continue;

    frescos.push_back(item);
    if ((int)frescos.size() >= NEWS_MAX_ITEMS) break;
  }

  const bool hay = !frescos.empty();
  const int  cuantos = (int)frescos.size();

  {
    DataLock::Guard g;
    // Si no vino nada se conservan los titulares anteriores, por el mismo
    // motivo que en OpenSkyClient: una respuesta pobre no tiene por que dejar
    // la pantalla en blanco.
    if (hay) _items.swap(frescos);
    _ok = hay;
    _lastError = hay ? String("") : String("Sin titulares");
  }

  if (hay) {
    _lastOkMs = millis();
    Serial.printf("[News] %d titulares actualizados\n", cuantos);
  }

  return _ok;
}
