#pragma once
#include <Arduino.h>
#include <vector>
#include "utils/GeoUtils.h"

struct AircraftState {
  String  icao24;
  String  callsign;
  double  lat;
  double  lon;
  double  baroAltitudeM;
  double  velocityMs;
  double  trackDeg;      // true_track: rumbo real del avión (0-360, 0 = Norte)
  bool    onGround;

  // Calculados respecto a un punto de referencia (se llenan después de parsear)
  double  distanceKm = 0;
  double  bearingDeg = 0;
};

class OpenSkyClient {
  public:
    // Las credenciales ya no vienen del compilador: se leen de NVS en runtime
    // (ver DeviceConfig), asi que se pueden cambiar desde el navegador sin
    // recompilar. Se releen en cada pedido de token.
    OpenSkyClient() = default;

    // Pide un token nuevo si no hay uno vigente. Devuelve false si falla.
    bool ensureToken();

    // Trae aviones dentro de un bounding box. Devuelve false si falla la request.
    bool fetchStates(const GeoUtils::BBox& box, std::vector<AircraftState>& out);

  private:
    String  _accessToken;
    uint32_t _tokenExpiresAt = 0; // millis() en el que expira (con margen)

    bool requestNewToken();
};
