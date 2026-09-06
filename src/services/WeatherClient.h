#pragma once
#include <Arduino.h>
#include "config.h"

// Categoría de dibujo del ícono. Varios códigos WMO caen en la misma:
// no vale la pena dibujar 28 íconos distintos en 240x320.
enum class WeatherIcon { Sun, PartlyCloudy, Cloud, Fog, Drizzle, Rain, Snow, Storm, Unknown };

struct WeatherNow {
  double      tempC        = 0;
  double      feelsLikeC   = 0;
  double      windKmh      = 0;
  int         humidityPct  = 0;
  int         wmoCode      = -1;
  WeatherIcon icon         = WeatherIcon::Unknown;
  String      description;   // en español sin acentos (las fuentes son ASCII)
  String      observedAt;    // "HH:MM" hora local

  // Pronóstico extendido: viene en la MISMA request que lo de arriba (bloque
  // "daily" de Open-Meteo), así que mostrarlo no cuesta una llamada más a la
  // API. Array de tamaño fijo y no vector: son 5 días como mucho
  // (WEATHER_FORECAST_DAYS), no vale la pena la asignación dinámica.
  struct Day {
    String      label;        // "Hoy", "Lun", "Mar", ...
    WeatherIcon icon         = WeatherIcon::Unknown;
    String      description; // igual que arriba, en español sin acentos
    double      tempMaxC     = 0;
    double      tempMinC     = 0;
  };
  Day daily[WEATHER_FORECAST_DAYS];
  int dailyCount = 0; // cuantos de daily[] son validos (Open-Meteo podria dar menos)
};

// Clima actual de HOME_LAT/HOME_LON usando Open-Meteo.
// No necesita API key: la API es abierta para uso no comercial.
class WeatherClient {
  public:
    bool shouldRefresh() const;
    bool refresh();

    const WeatherNow& now() const { return _now; }
    bool hasData() const { return _ok; }
    const String& lastError() const { return _lastError; }

  private:
    WeatherNow _now;

    bool     _ok = false;
    bool     _everTried = false;
    uint32_t _lastOkMs = 0;
    uint32_t _lastTryMs = 0;
    String   _lastError;

    // Traduce el código WMO a ícono + texto legible
    static void describe(int code, WeatherIcon& icon, String& text);
};
