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

// Origen/destino estimados de un vuelo, via /flights/aircraft. "Estimados"
// porque asi los llama la propia OpenSky: los infiere de la trayectoria, no
// los lee de un plan de vuelo. icao24 identifica de que avion es esta ruta,
// para poder descartarla si para cuando llega el usuario ya paso a mirar otro.
struct RouteInfo {
  char icao24[7]  = {0};
  char depIcao[5] = {0}; // codigo ICAO del aeropuerto de origen, o "" si no vino
  char arrIcao[5] = {0}; // idem destino
  bool valid      = false; // true si vino al menos uno de los dos
};

class OpenSkyClient {
  public:
    // Las credenciales ya no vienen del compilador: se leen de NVS en runtime
    // (ver DeviceConfig), asi que se pueden cambiar desde el navegador sin
    // recompilar. Se releen en cada pedido de token.
    OpenSkyClient() = default;

    // Pide un token nuevo si no hay uno vigente. Devuelve false si falla.
    bool ensureToken();

    // Trae aviones dentro de un bounding box. Devuelve false si falla la request,
    // incluido el caso en que ni siquiera se intenta por estar en backoff (ver
    // isBackingOff()).
    bool fetchStates(const GeoUtils::BBox& box, std::vector<AircraftState>& out);

    // Busca el vuelo mas reciente de un avion en las ultimas ROUTE_LOOKBACK_S
    // horas, para sacarle el origen/destino estimado. Se llama una sola vez al
    // abrir la ficha de un avion (no en cada refresco), asi que el gasto de
    // cupo es chico frente al de fetchStates(). Comparte el mismo contador y
    // backoff: un 429 en esta ruta significa lo mismo que en /states/all.
    //
    // Devuelve true si la request salio bien, aunque no haya encontrado ningun
    // vuelo (out.valid queda en false); false solo si la request en si fallo.
    bool fetchRoute(const char* icao24, RouteInfo& out);

    // --- Metricas para el dashboard --------------------------------------
    // volatile y no bajo DataLock: son enteros de 32 bits alineados, y en el
    // ESP32 esa lectura/escritura ya es atomica a nivel de bus. No hace falta
    // el candado que si necesita la lista de aviones (un vector no se puede
    // leer a mitad de un swap sin corromperse; un uint32 si). El peor caso es
    // leer un valor de un instante antes, que para un contador informativo no
    // importa. Mismo patron que _busy/_hasData en NetTask.

    // Cuantas veces se golpeo /states/all hoy. Se resetea a las 00:00 hora
    // local (necesita NTP: sin hora, sigue sumando sin reiniciar solo).
    uint32_t requestsToday() const { return _requestsToday; }

    // true mientras estamos en cuarentena por un 429 reciente.
    bool isBackingOff() const {
      return _backoffUntilMs != 0 && (int32_t)(millis() - _backoffUntilMs) < 0;
    }

    // Segundos que faltan para el proximo intento. 0 si no estamos en backoff.
    uint32_t backoffRemainingS() const {
      if (!isBackingOff()) return 0;
      return (uint32_t)(_backoffUntilMs - millis()) / 1000;
    }

    // Doblado con techo: 0 -> START, despues x2 hasta MAX. Publico y estatico
    // para poder testearlo sin necesitar red ni reloj.
    static uint32_t nextBackoffMs(uint32_t currentMs);

  private:
    String  _accessToken;
    uint32_t _tokenExpiresAt = 0; // millis() en el que expira (con margen)

    volatile uint32_t _requestsToday  = 0;
    uint32_t          _dayNumber      = 0; // dia local en curso, 0 = sin hora todavia
    volatile uint32_t _backoffMs      = 0; // duracion del backoff actual, 0 = ninguno
    volatile uint32_t _backoffUntilMs = 0;

    bool requestNewToken();
};
