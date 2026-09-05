#include "services/OpenSkyClient.h"
#include "config.h"
#include "core/DeviceConfig.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "utils/StrUtils.h"

bool OpenSkyClient::ensureToken() {
  // Renueva con 60s de margen antes de que expire.
  //
  // Se resta y se mira el signo en vez de comparar directo con "<": millis()
  // da la vuelta a los 49,7 dias y vuelve a cero, y ahi "millis() <
  // _tokenExpiresAt" diria que un token ya vencido sigue vigente. Como no se
  // renovaria nunca mas, el radar dejaria de traer datos hasta que alguien
  // reinicie la placa. La resta en aritmetica sin signo sigue dando bien
  // despues del wrap; es el mismo patron que usan los caches de NewsClient y
  // WeatherClient.
  if (_accessToken.length() > 0 && (int32_t)(millis() - _tokenExpiresAt) < 0) {
    return true;
  }
  return requestNewToken();
}

bool OpenSkyClient::requestNewToken() {
  WiFiClientSecure client;
  client.setInsecure(); // MVP: sin validar certificado. Ver README para pinning/CA.

  HTTPClient http;
  if (!http.begin(client, OPENSKY_TOKEN_URL)) {
    Serial.println("[OpenSky] No se pudo iniciar conexion de token");
    return false;
  }

  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // Se leen de NVS en cada intento: si el usuario las corrige desde el
  // navegador, el proximo token ya sale con las nuevas sin reiniciar.
  String id     = deviceConfig.get("osid");
  String secret = deviceConfig.get("ossec");
  if (id.length() == 0 || secret.length() == 0) {
    Serial.println("[OpenSky] Faltan las credenciales. Cargalas en http://desk-radar.local/config");
    http.end();
    return false;
  }

  String body = "grant_type=client_credentials";
  body += "&client_id=" + id;
  body += "&client_secret=" + secret;

  int code = http.POST(body);
  if (code != 200) {
    Serial.printf("[OpenSky] Error pidiendo token, HTTP %d\n", code);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.println("[OpenSky] Error parseando respuesta de token");
    return false;
  }

  _accessToken = doc["access_token"].as<String>();
  long expiresIn = doc["expires_in"] | 1800; // segundos, default 30 min

  // Se renueva 60s antes de que venza, pero cuidando que la resta no quede
  // negativa: si el servidor devolviera un expires_in corto, (expiresIn - 60)
  // negativo multiplicado por un unsigned da un plazo enorme y el token no se
  // renovaria nunca (el mismo sintoma que el wrap de millis(), por otra puerta).
  long validFor = expiresIn - 60;
  if (validFor < 30) validFor = expiresIn / 2; // expires_in raro o muy corto
  if (validFor < 1)  validFor = 1;
  _tokenExpiresAt = millis() + (uint32_t)validFor * 1000UL;

  Serial.println("[OpenSky] Token renovado OK");
  return _accessToken.length() > 0;
}

// OJO con el manejo de errores de aca: `out` NO se toca hasta que la respuesta
// entera esta parseada y es buena. Antes esta funcion arrancaba con un
// out.clear() y recien despues pedia el token y hacia el GET, asi que cualquier
// 429, 503 o corte de TLS -que con OpenSky pasa seguido- devolvia false con la
// lista ya vaciada: el radar se quedaba pelado 30 segundos por un error de red
// que habia durado un instante. Ahora se llena un vector aparte y se cambia por
// el del llamador de una sola vez, al final del todo.
bool OpenSkyClient::fetchStates(const GeoUtils::BBox& box, std::vector<AircraftState>& out) {
  if (!ensureToken()) {
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  char url[256];
  snprintf(url, sizeof(url),
    "%s?lamin=%.4f&lamax=%.4f&lomin=%.4f&lomax=%.4f",
    OPENSKY_STATES_URL, box.lamin, box.lamax, box.lomin, box.lomax);

  if (!http.begin(client, url)) {
    Serial.println("[OpenSky] No se pudo iniciar conexion de states");
    return false;
  }

  http.setTimeout(15000); // OpenSky a veces tarda varios segundos

  http.addHeader("Authorization", "Bearer " + _accessToken);

  int code = http.GET();
  if (code != 200) {
    Serial.printf("[OpenSky] Error pidiendo states, HTTP %d\n", code);
    http.end();
    return false;
  }

  // OpenSky responde con Transfer-Encoding: chunked, y HTTPClient::getStream()
  // entrega el socket TCP crudo: los headers de chunk ("16c5\r\n...") viajan
  // dentro del stream y ArduinoJson muere en el primer byte. El que desarma el
  // chunked es writeToStream(), que es justamente lo que usa getString().
  // Por eso el token funcionaba (usa getString) y los aviones no aparecian.
  //
  // Sin filtro a proposito: filtrar un array de arrays reindexa las columnas y
  // rompia lat/lon/altitud. Estos bounding boxes devuelven unos pocos KB.
  String payload = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);

  if (err) {
    Serial.print("[OpenSky] Error parseando states: ");
    Serial.println(err.c_str());
    Serial.print("[OpenSky] Payload recibido: ");
    Serial.println(payload.substring(0, 120));
    return false;
  }

  // A partir de aca la respuesta ya es buena, asi que lo que salga reemplaza a
  // lo que habia. states nulo significa que la zona esta vacia de verdad, y eso
  // tambien es un dato: el swap de abajo deja `out` vacio, que es lo correcto.
  // Lo que no puede pasar es vaciarla por un error de red.
  std::vector<AircraftState> frescos;

  JsonArray states = doc["states"].as<JsonArray>();
  if (states.isNull()) {
    out.swap(frescos);
    return true; // sin tráfico en la zona, no es un error
  }

  frescos.reserve(states.size());

  for (JsonArray s : states) {
    AircraftState a;
    StrUtils::copyTrimmed(s[0].is<const char*>() ? s[0].as<const char*>() : nullptr,
                a.icao24, sizeof(a.icao24));
    StrUtils::copyTrimmed(s[1].is<const char*>() ? s[1].as<const char*>() : nullptr,
                a.callsign, sizeof(a.callsign));
    a.lon           = s[5] | 0.0;
    a.lat           = s[6] | 0.0;
    a.baroAltitudeM = s[7] | 0.0;
    a.onGround      = s[8] | false;
    a.velocityMs    = s[9] | 0.0;
    a.trackDeg      = s[10] | 0.0;

    if (a.lat == 0.0 && a.lon == 0.0) continue; // sin posición válida

    frescos.push_back(a);
  }

  out.swap(frescos);
  return true;
}
