#include "services/ISSClient.h"
#include "config.h"
#include "core/DataLock.h"
#include "utils/GeoUtils.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

bool ISSClient::shouldRefresh() const {
  if (!_everTried) return true;
  if (millis() - _lastTryMs < API_RETRY_MS) return false;
  if (!_ok) return true;
  return millis() - _lastOkMs >= ISS_CACHE_MS;
}

bool ISSClient::refresh() {
  _everTried = true;
  _lastTryMs = millis();

  // Valida contra el root de Let's Encrypt (TLS_ROOT_CA_PEM en config.h):
  // wheretheiss.at termina en el mismo root que OpenSky/GNews/Open-Meteo.
  WiFiClientSecure client;
  client.setCACert(TLS_ROOT_CA_PEM);

  HTTPClient http;
  if (!http.begin(client, ISS_URL)) {
    _lastError = "Sin conexion";
    Serial.println("[ISS] No se pudo iniciar la conexion");
    return false;
  }

  http.setTimeout(15000);

  int code = http.GET();
  if (code != 200) {
    _lastError = "HTTP " + String(code);
    Serial.printf("[ISS] Error pidiendo la posicion, HTTP %d\n", code);
    http.end();
    return false;
  }

  // getString() y no getStream(): esta API tambien responde chunked (ver el
  // comentario largo en NewsClient::refresh).
  String payload = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);

  if (err) {
    _lastError = String("JSON: ") + err.c_str();
    Serial.print("[ISS] Error parseando: ");
    Serial.println(err.c_str());
    Serial.print("[ISS] Payload recibido: ");
    Serial.println(payload.substring(0, 120));
    return false;
  }

  if (doc["latitude"].isNull() || doc["longitude"].isNull()) {
    _lastError = "Sin datos";
    Serial.println("[ISS] La respuesta no trajo latitude/longitude");
    return false;
  }

  ISSPosition p;
  p.lat         = doc["latitude"]  | 0.0;
  p.lon         = doc["longitude"] | 0.0;
  p.altitudeKm  = doc["altitude"]  | 0.0;
  p.velocityKmh = doc["velocity"]  | 0.0;

  String vis = doc["visibility"].as<String>();
  if (vis == "daylight")     p.visibility = ISSVisibility::Daylight;
  else if (vis == "eclipsed") p.visibility = ISSVisibility::Eclipsed;
  else                        p.visibility = ISSVisibility::Unknown;

  p.distanceKm = GeoUtils::distanceKm(HOME_LAT, HOME_LON, p.lat, p.lon);
  p.bearingDeg = GeoUtils::bearingDeg(HOME_LAT, HOME_LON, p.lat, p.lon);

  // Publicacion bajo el candado: esto corre en la tarea de red y el loop puede
  // estar dibujando la pantalla de la ISS justo ahora (ver NewsClient).
  {
    DataLock::Guard g;
    _now = p;
    _ok = true;
    _lastError = "";
  }
  _lastOkMs = millis();
  Serial.printf("[ISS] %.0f km de altura, %.0f km/h, a %.0f km de casa\n",
                p.altitudeKm, p.velocityKmh, p.distanceKm);

  return true;
}
