#include "core/NetTask.h"
#include "core/DataLock.h"
#include <string.h>

// 14 KB de stack. El handshake TLS de WiFiClientSecure es lo que más pide
// (mbedTLS arma los buffers de sesión ahí adentro); con los 4 KB del default la
// tarea se moría de stack overflow en el primer fetch a OpenSky.
//
// Estaba en 10 KB hasta que el pronóstico extendido del clima (payload más
// grande, con el bloque "daily" de Open-Meteo) causó un crash con backtrace
// corrupto en el primer arranque -la firma típica de un stack overflow-, que
// no se pudo reproducir en 3 reinicios limpios midiendo con
// uxTaskGetStackHighWaterMark() (~5,3 KB libres de margen en cada uno). No
// hay certeza de que haya sido esto -pudo ser un hiccup puntual de red/NTP en
// ese primer arranque-, pero subir el margen sale gratis frente a los 320 KB
// de RAM totales, así que se sube igual en vez de dejarlo al límite medido.
static const uint32_t NET_STACK_BYTES = 14336;

// Core 0. El loop de Arduino corre en el core 1, así que la red queda del lado
// donde ya vive el stack de WiFi y deja al core 1 entero para dibujar.
static const BaseType_t NET_CORE = 0;

// Prioridad 1: por debajo del loop (que corre en 1 también pero en otro núcleo)
// y por encima del idle. No necesita más: se pasa casi todo el tiempo bloqueada
// esperando la red.
static const UBaseType_t NET_PRIORITY = 1;

void NetTask::begin() {
  if (_handle) return;

  _wakeup = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(trampoline, "net", NET_STACK_BYTES, this,
                          NET_PRIORITY, &_handle, NET_CORE);

  Serial.printf("[Red] Tarea de red en el core %d, %u bytes de stack\n",
                (int)NET_CORE, (unsigned)NET_STACK_BYTES);
}

void NetTask::trampoline(void* self) {
  static_cast<NetTask*>(self)->run();
}

bool NetTask::request(NetJob job, const GeoUtils::BBox& box) {
  // Un pedido por vez. No se hace cola a propósito: si la tarea está trayendo
  // aviones, el pedido que viene atrás quiere exactamente lo mismo, y el que
  // está en curso va a traer datos más nuevos que los que traería el encolado.
  if (_busy) return false;

  _job = job;
  _box = box;
  _busy = true;
  xSemaphoreGive(_wakeup);
  return true;
}

bool NetTask::requestRoute(const char* icao24) {
  if (_busy) return false;

  strncpy(_routeIcao24, icao24, sizeof(_routeIcao24) - 1);
  _routeIcao24[sizeof(_routeIcao24) - 1] = '\0';

  _job = NetJob::Route;
  _busy = true;
  xSemaphoreGive(_wakeup);
  return true;
}

void NetTask::run() {
  for (;;) {
    xSemaphoreTake(_wakeup, portMAX_DELAY);

    // Copias locales: el loop no puede cambiarlas mientras trabajamos porque no
    // manda otro pedido hasta que _busy vuelva a false.
    const NetJob job = _job;
    const GeoUtils::BBox box = _box;

    if (job == NetJob::Aircraft) {
      // El fetch entero ocurre FUERA del candado, que es de lo que se trata
      // todo esto. Solo la publicación lo toma.
      std::vector<AircraftState> frescos;
      if (_os.fetchStates(box, frescos)) {
        DataLock::Guard g;
        _inbox.swap(frescos);
        _hasData = true;
      }
    } else if (job == NetJob::News) {
      // refresh() publica su resultado bajo el mismo candado (ver NewsClient).
      if (_news.refresh()) _refreshed = true;
    } else if (job == NetJob::Weather) {
      if (_weather.refresh()) _refreshed = true;
    } else if (job == NetJob::ISS) {
      if (_iss.refresh()) _refreshed = true;
    } else if (job == NetJob::Route) {
      RouteInfo route;
      if (_os.fetchRoute(_routeIcao24, route)) {
        DataLock::Guard g;
        _routeResult = route;
        _hasRoute = true;
      }
    }

    _busy = false;
  }
}

bool NetTask::takeAircraft(std::vector<AircraftState>& out) {
  if (!_hasData) return false;

  DataLock::Guard g;
  out.swap(_inbox);
  _inbox.clear();
  _hasData = false;
  return true;
}

bool NetTask::takeRefreshed() {
  if (!_refreshed) return false;
  _refreshed = false;
  return true;
}

bool NetTask::takeRoute(RouteInfo& out) {
  if (!_hasRoute) return false;

  DataLock::Guard g;
  out = _routeResult;
  _hasRoute = false;
  return true;
}
