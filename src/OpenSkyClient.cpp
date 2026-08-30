#include "OpenSkyClient.h"
#include "config.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

OpenSkyClient::OpenSkyClient(const char* clientId, const char* clientSecret)
  : _clientId(clientId), _clientSecret(clientSecret) {}

bool OpenSkyClient::ensureToken() {
  // Renueva con 60s de margen antes de que expire
  if (_accessToken.length() > 0 && millis() < _tokenExpiresAt) {
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

  String body = "grant_type=client_credentials";
  body += "&client_id=" + String(_clientId);
  body += "&client_secret=" + String(_clientSecret);

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
  _tokenExpiresAt = millis() + (uint32_t)((expiresIn - 60) * 1000UL); // margen de 60s

  Serial.println("[OpenSky] Token renovado OK");
  return _accessToken.length() > 0;
}

bool OpenSkyClient::fetchStates(const GeoUtils::BBox& box, std::vector<AircraftState>& out) {
  out.clear();

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

  http.addHeader("Authorization", "Bearer " + _accessToken);

  int code = http.GET();
  if (code != 200) {
    Serial.printf("[OpenSky] Error pidiendo states, HTTP %d\n", code);
    http.end();
    return false;
  }

  // Nota: en versiones anteriores acá había un filtro de ArduinoJson para
  // ahorrar RAM (pensado para el ESP32-C3 original). Se sacó a propósito:
  // el filtro por índice numérico reindexa el array resultante desde 0,
  // así que s[5]/s[6]/etc dejaban de corresponder a lon/lat reales. Con el
  // ESP32 WROOM/WROVER actual sobra RAM para parsear el JSON completo.
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();

  if (err) {
    Serial.print("[OpenSky] Error parseando states: ");
    Serial.println(err.c_str());
    return false;
  }

  JsonArray states = doc["states"].as<JsonArray>();
  if (states.isNull()) {
    return true; // sin tráfico en la zona, no es un error
  }

  for (JsonArray s : states) {
    AircraftState a;
    a.icao24   = s[0].as<String>();
    a.callsign = s[1].is<const char*>() ? String(s[1].as<const char*>()) : String("");
    a.callsign.trim();
    a.lon           = s[5] | 0.0;
    a.lat           = s[6] | 0.0;
    a.baroAltitudeM = s[7] | 0.0;
    a.onGround      = s[8] | false;
    a.velocityMs    = s[9] | 0.0;

    if (a.lat == 0.0 && a.lon == 0.0) continue; // sin posición válida

    out.push_back(a);
  }

  return true;
}
