#pragma once
#include "core/DisplayManager.h"
#include "services/WeatherClient.h"

// Pronóstico extendido: hasta WEATHER_FORECAST_DAYS días, en una lista
// vertical simple. Se llega tocando la pantalla de Clima (WeatherScreen) y
// cualquier toque acá vuelve, el mismo patrón que NewsDetailScreen.
//
// No hace su propio fetch: los datos ya vinieron en la misma request que el
// clima actual (ver WeatherClient::refresh()), así que solo dibuja lo que ya
// está en WeatherNow.
class WeatherForecastScreen {
  public:
    explicit WeatherForecastScreen(DisplayManager& display) : _display(display) {}

    void render(const WeatherNow& now);

  private:
    DisplayManager& _display;
};
