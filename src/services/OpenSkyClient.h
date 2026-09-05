#pragma once
#include <Arduino.h>
#include <vector>
#include "utils/GeoUtils.h"

struct AircraftState {
  // char fijo y no String: cada fetch trae hasta 60 aviones, y con dos String
  // por avion eran unos 120 malloc/free cada 5 a 30 segundos, mas los que
  // agregaban las copias de AircraftBlip y el std::sort. En una pantalla
  // pensada para quedar encendida semanas eso fragmenta el heap, y justo las
  // dos cosas que mas lo necesitan piden bloques grandes y contiguos: el sprite
  // del radar (20 KB) y el stack de la tarea de red (10 KB).
  //
  // Los dos largos son fijos por especificacion ADS-B: el ICAO24 son 6 digitos
  // hexadecimales y el callsign, 8 caracteres. Van inicializados para que un
  // AircraftState recien declarado sea seguro de leer.
  char    icao24[7]   = {0};
  char    callsign[9] = {0};
  double  lat;
  double  lon;
  double  baroAltitudeM;
  double  velocityMs;
  double  trackDeg;      // true_track: rumbo real del avión (0-360, 0 = Norte)
  bool    onGround;

  // Calculados respecto a un punto de referencia (se llenan después de parsear)
  double  distanceKm = 0;
  double  bearingDeg = 0;

  // Etiqueta para mostrar en pantalla: el callsign si vino, y si no el ICAO24,
  // que siempre esta. Cuatro pantallas hacian esta misma cuenta a mano.
  const char* label() const { return callsign[0] ? callsign : icao24; }
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
