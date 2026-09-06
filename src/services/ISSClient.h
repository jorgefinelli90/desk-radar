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

// Un punto de la traza en el suelo: solo lat/lon, no hace falta mas para
// dibujar la curva sobre el mapa.
struct ISSTrackPoint {
  double lat = 0;
  double lon = 0;
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

    // Traza de la orbita: ISS_TRACK_HALF puntos hacia atras (mas viejo primero)
    // y otros tantos hacia adelante (mas cercano primero) desde "ahora",
    // separados ISS_TRACK_STEP_S segundos. Vacio (count 0) hasta el primer
    // fetch bueno de la traza, que puede tardar mas que el de la posicion.
    const ISSTrackPoint* trackPast()   const { return _trackPast; }
    const ISSTrackPoint* trackFuture() const { return _trackFuture; }
    int trackPastCount()   const { return _trackPastCount; }
    int trackFutureCount() const { return _trackFutureCount; }

  private:
    ISSPosition _now;

    bool     _ok = false;
    bool     _everTried = false;
    uint32_t _lastOkMs = 0;
    uint32_t _lastTryMs = 0;
    String   _lastError;

    ISSTrackPoint _trackPast[ISS_TRACK_HALF];
    ISSTrackPoint _trackFuture[ISS_TRACK_HALF];
    int      _trackPastCount   = 0;
    int      _trackFutureCount = 0;
    uint32_t _lastTrackOkMs    = 0;

    // Best-effort: si falla no tira abajo el refresh de la posicion, que es lo
    // que importa. Se pide aparte y mucho menos seguido (ISS_TRACK_CACHE_MS).
    void refreshTrackIfDue();
};
