#pragma once
#include <Arduino.h>
#include <FS.h>
#include "DisplayManager.h"
#include "MapAssets.h"
#include "UiRect.h"

// Lector del mapa raster pre-renderizado.
//
// Los .bin son RGB565 crudo ya guardado en big-endian por tools/build-map.mjs,
// justo como los quiere el ILI9341. Por eso se empujan con setSwapBytes(false)
// y el ESP32 no toca un solo byte: solo lee de flash y manda por SPI.
//
// Toda la E/S del mapa pasa por acá a propósito: el módulo TFT tiene ranura de
// microSD, así que el día que le pongas una tarjeta se cambia esta clase sola
// (LittleFS -> SD) sin tocar MapScreen.
class MapTiles {
  public:
    explicit MapTiles(DisplayManager& display) : _display(display) {}

    // Monta el sistema de archivos. Devuelve false si no se pudo: MapScreen
    // sigue funcionando, solo que sobre fondo negro.
    bool begin();
    bool ready() const { return _mounted; }

    // Vuelca la imagen completa del mapa en (0, screenTop). Va por bandas de
    // BAND_ROWS filas para no necesitar los 125 KB de una.
    bool pushFull(int assetIdx, int screenTop);

    // Restaura un rectángulo del mapa. Se usa para borrar los aviones del frame
    // anterior sin releer la imagen entera.
    // r está en coordenadas de la IMAGEN (0,0 = esquina sup. izq. del mapa).
    bool pushRect(int assetIdx, const UiRect& r, int screenTop);

  private:
    // 16 filas x 240 px x 2 bytes = 7.680 bytes. Con WiFi y TLS arriba no hay
    // lugar para mucho más, y a esta altura el overhead por banda es marginal.
    static const int BAND_ROWS = 16;

    DisplayManager& _display;
    bool _mounted = false;

    uint16_t _band[MAP_VIEW_W * BAND_ROWS];

    // fs::File y no File a secas: TFT_eSPI define FS_NO_GLOBALS, que deja el
    // alias global de FS.h fuera de juego.
    bool openAsset(int assetIdx, fs::File& f);
};
