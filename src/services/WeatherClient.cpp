#include "services/WeatherClient.h"
#include "config.h"
#include "core/DataLock.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Tabla WMO de Open-Meteo agrupada en las categorías que sabemos dibujar.
// Los textos van sin acentos a propósito: las fuentes de TFT_eSPI son ASCII.
void WeatherClient::describe(int code, WeatherIcon& icon, String& text) {
  switch (code) {
    case 0:  icon = WeatherIcon::Sun;          text = "Despejado";            break;
    case 1:  icon = WeatherIcon::Sun;          text = "Mayormente despejado"; break;
    case 2:  icon = WeatherIcon::PartlyCloudy; text = "Parcialmente nublado"; break;
    case 3:  icon = WeatherIcon::Cloud;        text = "Nublado";              break;

    case 45:
    case 48: icon = WeatherIcon::Fog;          text = "Niebla";               break;

    case 51:
    case 53:
    case 55: icon = WeatherIcon::Drizzle;      text = "Llovizna";             break;
    case 56:
    case 57: icon = WeatherIcon::Drizzle;      text = "Llovizna helada";      break;

    case 61: icon = WeatherIcon::Rain;         text = "Lluvia leve";          break;
    case 63: icon = WeatherIcon::Rain;         text = "Lluvia";               break;
    case 65: icon = WeatherIcon::Rain;         text = "Lluvia fuerte";        break;
    case 66:
    case 67: icon = WeatherIcon::Rain;         text = "Lluvia helada";        break;

    case 71:
    case 73:
    case 75: icon = WeatherIcon::Snow;         text = "Nieve";                break;
    case 77: icon = WeatherIcon::Snow;         text = "Granos de nieve";      break;

    case 80:
    case 81: icon = WeatherIcon::Rain;         text = "Chaparrones";          break;
    case 82: icon = WeatherIcon::Rain;         text = "Chaparrones fuertes";  break;
    case 85:
    case 86: icon = WeatherIcon::Snow;         text = "Nevadas";              break;

    case 95: icon = WeatherIcon::Storm;        text = "Tormenta";             break;
    case 96:
    case 99: icon = WeatherIcon::Storm;        text = "Tormenta con granizo"; break;

    default: icon = WeatherIcon::Unknown;      text = "Condicion desconocida"; break;
  }
}

bool WeatherClient::shouldRefresh() const {
  if (!_everTried) return true;
  if (millis() - _lastTryMs < API_RETRY_MS) return false;
  if (!_ok) return true;
  return millis() - _lastOkMs >= WEATHER_CACHE_MS;
}

bool WeatherClient::refresh() {
  _everTried = true;
  _lastTryMs = millis();

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  char url[320];
  snprintf(url, sizeof(url),
    "%s?latitude=%.4f&longitude=%.4f"
    "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m"
    "&timezone=auto",
    OPENMETEO_URL, HOME_LAT, HOME_LON);

  if (!http.begin(client, url)) {
    _lastError = "Sin conexion";
    Serial.println("[Clima] No se pudo iniciar la conexion");
    return false;
  }

  http.setTimeout(15000);

  int code = http.GET();
  if (code != 200) {
    _lastError = "HTTP " + String(code);
    Serial.printf("[Clima] Error pidiendo el clima, HTTP %d\n", code);
    http.end();
    return false;
  }

  // getString() y no getStream(): Open-Meteo tambien responde chunked
  // (ver el comentario largo en NewsClient::refresh).
  String payload = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);

  if (err) {
    _lastError = String("JSON: ") + err.c_str();
    Serial.print("[Clima] Error parseando: ");
    Serial.println(err.c_str());
    Serial.print("[Clima] Payload recibido: ");
    Serial.println(payload.substring(0, 120));
    return false;
  }

  JsonObject cur = doc["current"].as<JsonObject>();
  if (cur.isNull()) {
    _lastError = "Sin datos";
    Serial.println("[Clima] La respuesta no trajo current");
    return false;
  }

  WeatherNow w;
  w.tempC       = cur["temperature_2m"]       | 0.0;
  w.feelsLikeC  = cur["apparent_temperature"] | 0.0;
  w.humidityPct = cur["relative_humidity_2m"] | 0;
  w.windKmh     = cur["wind_speed_10m"]       | 0.0; // Open-Meteo ya lo da en km/h
  w.wmoCode     = cur["weather_code"]         | -1;
  describe(w.wmoCode, w.icon, w.description);

  // Con timezone=auto la hora ya viene local: "2026-08-30T14:30"
  String t = cur["time"].as<String>();
  w.observedAt = (t.length() >= 16) ? t.substring(11, 16) : String("");

  // Publicacion bajo el candado: esto corre en la tarea de red y el loop puede
  // estar dibujando la pantalla del clima justo ahora (ver NewsClient).
  {
    DataLock::Guard g;
    _now = w;
    _ok = true;
    _lastError = "";
  }
  _lastOkMs = millis();
  Serial.printf("[Clima] %.1f C, codigo WMO %d\n", w.tempC, w.wmoCode);
  return true;
}
