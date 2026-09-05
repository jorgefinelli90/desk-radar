#include "core/DataLock.h"

namespace DataLock {

static SemaphoreHandle_t _mutex = nullptr;

void begin() {
  if (_mutex) return;
  _mutex = xSemaphoreCreateMutex();
}

void lock() {
  if (!_mutex) return; // todavía no hay tarea de red: nadie con quién competir
  xSemaphoreTake(_mutex, portMAX_DELAY);
}

void unlock() {
  if (!_mutex) return;
  xSemaphoreGive(_mutex);
}

} // namespace DataLock
