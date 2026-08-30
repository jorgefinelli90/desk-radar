#pragma once
#include <vector>
#include "DisplayManager.h"
#include "OpenSkyClient.h"
#include "AircraftBlip.h"
#include "MapTiles.h"
#include "MapAssets.h"
#include "UiRect.h"

// Mapa realista del área de casa. El fondo es una imagen raster pre-renderizada
// (ver tools/build-map.mjs) que vive en LittleFS; encima se dibujan los aviones
// como siluetas rotadas por su rumbo y coloreadas por altitud.
//
// Dos zooms: 80 km y 40 km, alternables con el botón de abajo. Los dos salen
// del mismo fetch de OpenSky, así que cambiar de zoom no pide datos nuevos.
class MapScreen {
  public:
    explicit MapScreen(DisplayManager& display) : _display(display), _tiles(display) {}

    // Monta LittleFS. Si falla, la pantalla sigue andando sobre fondo negro.
    bool begin() { return _tiles.begin(); }

    // Llamar al entrar desde otra pantalla: fuerza el redibujo completo.
    void onEnter();

    void render(std::vector<AircraftState>& aircraft);

    // Busca un avión en las coordenadas tocadas. Devuelve nullptr si no hay.
    const AircraftState* hitTest(uint16_t x, uint16_t y) const;

    // Botón que alterna 80 km <-> 40 km (mismo patrón que AirportScreen)
    const UiRect& zoomButtonRect() const { return _zoomBtn; }
    void toggleZoom();
    int  currentWidthKm() const { return MAP_ASSETS[_assetIdx].widthKm; }

  private:
    // El mapa arranca justo debajo de la barra de estado. Abajo quedan la
    // leyenda de altitud y el botón de zoom.
    static const int MAP_TOP    = DisplayManager::STATUS_BAR_HEIGHT;
    static const int LEGEND_Y   = MAP_TOP + MAP_VIEW_H + 2;
    static const int LEGEND_H   = 16;
    static const int ZOOM_BTN_H = 24;

    static const size_t MAX_PLANES = 60; // tope de seguridad para render/hit-test

    DisplayManager& _display;
    MapTiles _tiles;

    int  _assetIdx = 0;          // 0 = 80 km, 1 = 40 km
    bool _needsFullRedraw = true;
    UiRect _zoomBtn = { 0, 0, 0, 0 };

    std::vector<AircraftBlip> _blips; // se rellena en cada render()
    std::vector<UiRect> _dirty;       // zonas a restaurar en el próximo render

    // Color según altitud, rampa tipo FlightRadar24
    static uint16_t altitudeColor(double meters);

    void drawPlane(TFT_eSPI& tft, int x, int y, double trackDeg, uint16_t color);
    void drawHome(TFT_eSPI& tft);
    void drawLegend(TFT_eSPI& tft);
    void drawZoomButton(TFT_eSPI& tft);
    void drawNoMapNotice(TFT_eSPI& tft);
};
