#pragma once
#include <vector>
#include "config.h"
#include "core/DisplayManager.h"
#include "services/OpenSkyClient.h"
#include "models/AircraftBlip.h"
#include "services/MapTiles.h"
#include "MapAssets.h"

class RadarScreen {
  public:
    RadarScreen(DisplayManager& display, MapTiles& tiles)
      : _display(display), _tiles(tiles), _disc(&display.tft()) {}

    // Llamar al entrar a la pantalla (desde Home o al volver del Detail).
    // Reinicia el barrido y marca el marco fijo como "hay que redibujarlo".
    void onEnter();

    // Dibuja el radar completo con la lista de aviones ya calculada
    // (distanceKm y bearingDeg deben estar llenos).
    void render(std::vector<AircraftState>& aircraft, bool fastMode);

    // Avanza la animación del barrido. Se llama en cada vuelta del loop y
    // devuelve enseguida si todavía no toca dibujar un frame nuevo, así que
    // no bloquea el polling del touch.
    void tick();

    // true si algún avión está dentro de RADAR_NEAR_KM
    static bool hasNearbyTraffic(const std::vector<AircraftState>& aircraft);

    // Busca un avión en las coordenadas tocadas. Devuelve nullptr si no hay.
    const AircraftState* hitTest(uint16_t x, uint16_t y) const;

  private:
    // --- Geometría ---
    // DISC_SIZE y RING_MAX vienen de MapAssets.h porque el mapa de fondo se
    // genera con esos mismos valores: si no coincidieran, el mapa y los anillos
    // quedarían a distinta escala.
    static const int DISC_SIZE = RADAR_DISC_SIZE;
    static const int RING_MAX  = RADAR_RING_MAX;
    static const int DISC_X    = 20;
    static const int DISC_Y    = 18;
    static const int CENTER    = DISC_SIZE / 2;
    static const int CARD_R    = 95;    // radio donde van las letras N/E/S/O

    // Índices de la paleta de 16 colores del sprite (a 4 bpp el "color" que
    // reciben las primitivas ES el índice, no un RGB565).
    //
    // El reparto es apretado a propósito: los índices 1..5 se los lleva el mapa
    // de fondo (5 grises que calcula build-map.mjs del histograma real), así que
    // al radar le quedan 10. Por eso la estela tiene 3 bandas y no 7.
    enum : uint8_t {
      C_BG = 0,          // negro, fuera del círculo
      C_MAP0 = 1,        // 1..5: grises del mapa
      C_RING = 6,        // anillos y cruz
      C_TRAIL0 = 7,      // 7,8,9: estela de tenue a viva
      C_TRAIL_TOP = 9,
      C_EDGE = 10,       // borde de ataque del barrido
      C_BLIP = 11,
      C_ALERT = 12,      // avión dentro de RADAR_NEAR_KM
      C_PING = 13,       // destello de detección / marcador del aeropuerto
      C_HOME = 14,
      C_LABEL = 15       // etiquetas de anillos, cardinales y El Palomar
    };

    // Un blip del radar: el AircraftBlip compartido (zona tocable + copia del
    // avión, que es lo que consume hitTest) más lo que necesita la animación.
    struct Blip {
      AircraftBlip base;
      int16_t  sx = 0, sy = 0;  // centro en coordenadas locales del sprite
      float    bearing = 0;     // grados, 0 = Norte, horario
      uint32_t pingMs = 0;      // cuándo lo cruzó el barrido por última vez
      bool     near = false;
    };

    DisplayManager& _display;
    MapTiles&       _tiles;
    TFT_eSprite     _disc;

    bool _discReady = false;   // el sprite se pudo asignar
    bool _mapReady = false;    // el mapa de fondo se pudo cargar
    bool _triedInit = false;
    bool _chromeValid = false; // el marco fijo (barra, leyenda, panel) está dibujado

    uint16_t _palette[16];
    uint8_t* _mapRam = nullptr; // copia del mapa 4 bpp, RADAR_MAP_BYTES

    std::vector<Blip> _blips; // se rellena en cada render()

    float    _sweepDeg = 0;      // ángulo actual del barrido (0 = Norte, horario)
    float    _prevSweepDeg = 0;  // ángulo del frame anterior (para detectar cruces)
    uint32_t _lastFrameMs = 0;

    // Posición de El Palomar en el disco, calculada una sola vez
    bool  _palomarInView = false;
    int   _palomarX = 0, _palomarY = 0;

#if RADAR_DEBUG_TIMING
    uint32_t _statsMs = 0;
    uint32_t _frameCount = 0;
    uint32_t _frameCostSum = 0;
#endif

    void ensureDisc();
    void buildPalette();
    void computePalomar();
    void drawFrame();            // compone y vuelca un frame del disco
    void drawDiscSprite();       // camino con sprite (sin flicker)
    void drawDiscDirect();       // fallback sin sprite (solo la porción que cambia)
    void drawStaticDiscDirect(); // disco fijo del fallback, sobre el TFT
    void updatePings();          // marca los blips que el barrido acaba de cruzar
    void drawChrome();
    void drawPanel(const AircraftState* closest, int shown);

    // Punto sobre el círculo: 0 grados = Norte, sentido horario.
    static void polar(float deg, int r, int& x, int& y, int cx, int cy);
};
