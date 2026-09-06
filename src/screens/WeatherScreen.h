#pragma once
#include "core/DisplayManager.h"
#include "services/WeatherClient.h"

class WeatherScreen {
  public:
    explicit WeatherScreen(DisplayManager& display) : _display(display) {}

    void render(const WeatherClient& weather);

    // Color segun temperatura (frio->cyan, calor->rojo). Publico porque
    // WeatherForecastScreen lo reusa para colorear los maximos del pronostico:
    // es la paleta de "esta pantalla", no una utilidad generica de proyecto.
    static uint16_t tempColor(double c);

  private:
    DisplayManager& _display;

    // Íconos dibujados a mano con primitivas de TFT_eSPI (nada de bitmaps).
    static void drawIcon(TFT_eSPI& tft, WeatherIcon icon, int cx, int cy);
    static void drawSun(TFT_eSPI& tft, int cx, int cy, int r, uint16_t color);
    static void drawCloud(TFT_eSPI& tft, int cx, int cy, int w, uint16_t color);
    static void drawDrops(TFT_eSPI& tft, int cx, int cy, int count, int len, uint16_t color);
    static void drawFlakes(TFT_eSPI& tft, int cx, int cy, uint16_t color);
    static void drawBolt(TFT_eSPI& tft, int cx, int cy, uint16_t color);
};
