#pragma once
#include <Arduino.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "utils/GeoUtils.h"
#include "services/OpenSkyClient.h"
#include "services/NewsClient.h"
#include "services/WeatherClient.h"
#include "services/ISSClient.h"

// Toda la red del firmware, corriendo en el core 0.
//
// Antes los tres clientes HTTP se llamaban desde el loop, que vive en el core 1
// junto con el touch, el barrido del radar y el servidor web. Un fetch de
// OpenSky son varios segundos entre el handshake TLS y la respuesta, y durante
// ese rato no corría nada más: el barrido se congelaba, el touch no respondía y
// el dashboard no atendía. El cartel "Buscando aviones..." existía para tapar
// eso, y ya no hace falta.
//
// El ESP32 tiene dos núcleos y el firmware usaba uno solo. Ahora el loop pide
// trabajo y sigue dibujando a 22 fps mientras la tarea del core 0 espera a la
// API.
enum class NetJob : uint8_t { Aircraft, News, Weather, Route, ISS };

class NetTask {
  public:
    NetTask(OpenSkyClient& os, NewsClient& news, WeatherClient& weather, ISSClient& iss)
      : _os(os), _news(news), _weather(weather), _iss(iss) {}

    // Crea la tarea. Llamar después de DataLock::begin().
    void begin();

    // Encola un pedido. Devuelve false si la tarea ya está ocupada: no se
    // acumulan pedidos a propósito, porque el que viene atrás siempre pide lo
    // mismo y con datos más nuevos.
    bool request(NetJob job, const GeoUtils::BBox& box = GeoUtils::BBox{0, 0, 0, 0});

    // Pedido de ruta (origen/destino) para un avion puntual, identificado por
    // icao24. Aparte de request() porque necesita un string en vez de un bbox;
    // comparte el mismo "un pedido por vez" (devuelve false si la tarea esta
    // ocupada).
    bool requestRoute(const char* icao24);

    bool busy() const { return _busy; }

    // Si la tarea dejó aviones nuevos, los pasa a `out` y devuelve true. El
    // traspaso es un swap bajo el candado: no copia nada.
    bool takeAircraft(std::vector<AircraftState>& out);

    // true una sola vez, cuando terminó un refresh de noticias o de clima y hay
    // que volver a dibujar la pantalla.
    bool takeRefreshed();

    // Si la tarea dejó una ruta lista, la pasa a `out` (copia: es un struct
    // chico, no un vector) y devuelve true. El llamador tiene que comparar
    // out.icao24 contra el avión que está mirando: para cuando la respuesta
    // llega, el usuario puede haber pasado a ver otro.
    bool takeRoute(RouteInfo& out);

  private:
    OpenSkyClient& _os;
    NewsClient&    _news;
    WeatherClient& _weather;
    ISSClient&     _iss;

    TaskHandle_t      _handle  = nullptr;
    SemaphoreHandle_t _wakeup  = nullptr;  // "hay trabajo"

    // volatile: los escribe una tarea y los lee la otra. Son de 32 bits o menos,
    // que en el ESP32 se leen y escriben de una sola vez.
    volatile bool _busy      = false;
    volatile bool _hasData   = false;
    volatile bool _refreshed = false;

    NetJob          _job = NetJob::Aircraft;
    GeoUtils::BBox  _box{0, 0, 0, 0};
    char            _routeIcao24[7] = {0}; // para NetJob::Route

    // Buzón de aviones ya parseados, esperando a que el loop los recoja.
    std::vector<AircraftState> _inbox;

    volatile bool _hasRoute = false;
    RouteInfo     _routeResult;

    static void trampoline(void* self);
    void run();
};
