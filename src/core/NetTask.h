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
enum class NetJob : uint8_t { Aircraft, News, Weather };

class NetTask {
  public:
    NetTask(OpenSkyClient& os, NewsClient& news, WeatherClient& weather)
      : _os(os), _news(news), _weather(weather) {}

    // Crea la tarea. Llamar después de DataLock::begin().
    void begin();

    // Encola un pedido. Devuelve false si la tarea ya está ocupada: no se
    // acumulan pedidos a propósito, porque el que viene atrás siempre pide lo
    // mismo y con datos más nuevos.
    bool request(NetJob job, const GeoUtils::BBox& box = GeoUtils::BBox{0, 0, 0, 0});

    bool busy() const { return _busy; }

    // Si la tarea dejó aviones nuevos, los pasa a `out` y devuelve true. El
    // traspaso es un swap bajo el candado: no copia nada.
    bool takeAircraft(std::vector<AircraftState>& out);

    // true una sola vez, cuando terminó un refresh de noticias o de clima y hay
    // que volver a dibujar la pantalla.
    bool takeRefreshed();

  private:
    OpenSkyClient& _os;
    NewsClient&    _news;
    WeatherClient& _weather;

    TaskHandle_t      _handle  = nullptr;
    SemaphoreHandle_t _wakeup  = nullptr;  // "hay trabajo"

    // volatile: los escribe una tarea y los lee la otra. Son de 32 bits o menos,
    // que en el ESP32 se leen y escriben de una sola vez.
    volatile bool _busy      = false;
    volatile bool _hasData   = false;
    volatile bool _refreshed = false;

    NetJob          _job = NetJob::Aircraft;
    GeoUtils::BBox  _box{0, 0, 0, 0};

    // Buzón de aviones ya parseados, esperando a que el loop los recoja.
    std::vector<AircraftState> _inbox;

    static void trampoline(void* self);
    void run();
};
