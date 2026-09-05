#include "core/Clock.h"
#include "config.h"
#include <time.h>

// Sin sincronizar, el reloj del ESP32 arranca en enero de 1970. Cualquier fecha
// posterior a noviembre de 2023 alcanza como prueba de que SNTP ya contestó: no
// hace falta preguntarle nada al cliente NTP.
static const time_t HORA_CREIBLE = 1700000000;

void Clock::begin() {
  if (_started) return;
  _started = true;

  // El segundo argumento es el offset de horario de verano, que en Argentina no
  // existe. configTime() vuelve enseguida: deja el cliente SNTP corriendo en
  // segundo plano y la hora llega sola.
  configTime(TZ_OFFSET_H * 3600, 0, NTP_SERVER_1, NTP_SERVER_2);
  Serial.printf("[Reloj] Sincronizando con %s (UTC%+d)\n", NTP_SERVER_1, TZ_OFFSET_H);
}

bool Clock::hasTime() const {
  if (_synced) return true;
  _synced = (time(nullptr) > HORA_CREIBLE);
  if (_synced) {
    Serial.println("[Reloj] Hora sincronizada");
  }
  return _synced;
}

String Clock::hhmm() const {
  if (!hasTime()) return String("");

  time_t ahora = time(nullptr);
  struct tm t;
  localtime_r(&ahora, &t);

  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
  return String(buf);
}
