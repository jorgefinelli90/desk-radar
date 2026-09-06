#pragma once
#include <Arduino.h>
#include "config.h"

// "daylight" = el sol le pega, se ve iluminada si mirás en el momento y
// dirección correctos; "eclipsed" = está en la sombra de la Tierra, invisible
// a simple vista aunque esté pasando justo arriba.
enum class ISSVisibility { Daylight, Eclipsed, Unknown };

struct ISSPosition {
  double lat         = 0;
  double lon         = 0;
  double altitudeKm  = 0;
  double velocityKmh = 0;
  ISSVisibility visibility = ISSVisibility::Unknown;

  // Calculados respecto a HOME_LAT/HOME_LON (ver GeoUtils). A diferencia de
  // AircraftState, esto lo calcula el propio cliente y no un enriquecido
  // aparte en main.cpp: acá solo hay un punto y siempre es contra casa, igual
  // que WeatherClient ya trabaja directo con HOME_LAT/HOME_LON.
  double distanceKm = 0;
  double bearingDeg = 0;
};

// Posición en vivo de la Estación Espacial Internacional, via wheretheiss.at.
// No necesita API key. Cache corto (ISS_CACHE_MS en config.h): a la velocidad
// que se mueve, un dato de hace 15 minutos ya no sirve para nada.
class ISSClient {
  public:
    bool shouldRefresh() const;
    bool refresh();

    const ISSPosition& now() const { return _now; }
    bool hasData() const { return _ok; }
    const String& lastError() const { return _lastError; }

  private:
    ISSPosition _now;

    bool     _ok = false;
    bool     _everTried = false;
    uint32_t _lastOkMs = 0;
    uint32_t _lastTryMs = 0;
    String   _lastError;

};
