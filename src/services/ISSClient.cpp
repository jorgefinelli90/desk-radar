#include "services/ISSClient.h"
#include "config.h"
#include "core/DataLock.h"
#include "utils/GeoUtils.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string.h>

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

  refreshTrackIfDue();
  return true;
}

void ISSClient::refreshTrackIfDue() {
  if (_lastTrackOkMs != 0 && millis() - _lastTrackOkMs < ISS_TRACK_CACHE_MS) return;

  // time(nullptr) da hora LOCAL (configTime se llama con el offset de
  // Argentina, ver Clock.cpp), no UTC. wheretheiss.at espera timestamps UNIX
  // reales, asi que hay que deshacer el offset antes de pedir. Sin NTP
  // sincronizado todavia esto daria una fecha de 1970 y la API devolveria
  // basura: se reintenta solo en el proximo refresh, que ya viene cada
  // ISS_CACHE_MS de por si.
  time_t localNow = time(nullptr);
  static const time_t HORA_CREIBLE = 1700000000; // mismo piso que usa Clock.cpp
  if (localNow < HORA_CREIBLE) return;
  time_t utcNow = localNow - (time_t)TZ_OFFSET_H * 3600;

  // Timestamps en orden: todo el pasado (mas viejo -> mas cercano a ahora),
  // despues todo el futuro (mas cercano -> mas lejano). "ahora" (i=0) no se
  // pide: ya lo trae el fetch de posicion de arriba.
  String ts;
  for (int i = -ISS_TRACK_HALF; i <= ISS_TRACK_HALF; i++) {
    if (i == 0) continue;
    if (ts.length()) ts += ",";
    ts += String((long)(utcNow + (time_t)i * ISS_TRACK_STEP_S));
  }

  String url = String(ISS_URL) + "/positions?timestamps=" + ts;

  WiFiClientSecure client;
  client.setCACert(TLS_ROOT_CA_PEM);

  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.println("[ISS] No se pudo iniciar la conexion para la traza");
    return;
  }
  http.setTimeout(15000);

  int code = http.GET();
  if (code != 200) {
    Serial.printf("[ISS] Error pidiendo la traza, HTTP %d\n", code);
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err || !doc.is<JsonArray>()) {
    Serial.print("[ISS] Error parseando la traza: ");
    Serial.println(err.c_str());
    return;
  }

  ISSTrackPoint past[ISS_TRACK_HALF];
  ISSTrackPoint future[ISS_TRACK_HALF];
  int pastN = 0, futureN = 0, idx = 0;

  for (JsonObject o : doc.as<JsonArray>()) {
    ISSTrackPoint pt;
    pt.lat = o["latitude"]  | 0.0;
    pt.lon = o["longitude"] | 0.0;
    if (idx < ISS_TRACK_HALF)          past[pastN++]     = pt;
    else if (futureN < ISS_TRACK_HALF) future[futureN++] = pt;
    idx++;
  }

  {
    DataLock::Guard g;
    memcpy(_trackPast, past, sizeof(past));
    memcpy(_trackFuture, future, sizeof(future));
    _trackPastCount = pastN;
    _trackFutureCount = futureN;
  }
  _lastTrackOkMs = millis();
  Serial.printf("[ISS] Traza actualizada: %d puntos atras, %d adelante\n", pastN, futureN);
}
