#pragma once
#include <vector>
#include "DisplayManager.h"
#include "OpenSkyClient.h"
#include "AircraftBlip.h"
#include "UiRect.h"

// Mapa equirectangular de la provincia de Buenos Aires. Proyecta cada avión
// por su lat/lon sobre un contorno simplificado de la provincia. Mismo
// hit-test táctil que el radar: tocar un avión abre su ficha de detalle.
class MapScreen {
  public:
    explicit MapScreen(DisplayManager& display) : _display(display) {}

    void render(std::vector<AircraftState>& aircraft);

    // Busca un avión en las coordenadas tocadas. Devuelve nullptr si no hay.
    const AircraftState* hitTest(uint16_t x, uint16_t y) const;

  private:
    DisplayManager& _display;
    UiRect _mapArea = {0, 0, 0, 0};       // rectángulo del mapa en pantalla (aspecto real)
    std::vector<AircraftBlip> _blips;     // se rellena en cada render()

    void computeMapArea(TFT_eSPI& tft);
    int  mapX(double lon) const;
    int  mapY(double lat) const;
    bool inBounds(double lat, double lon) const;
    void drawOutline(TFT_eSPI& tft);
    void drawCities(TFT_eSPI& tft);
    void drawPlane(TFT_eSPI& tft, int x, int y, double trackDeg, uint16_t color);
};
