#pragma once
#include <vector>
#include "config.h"
#include "DisplayManager.h"
#include "OpenSkyClient.h"
#include "AircraftBlip.h"

class RadarScreen {
  public:
    explicit RadarScreen(DisplayManager& display)
      : _display(display), _disc(&display.tft()) {}

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
    // --- Geometría (todo en píxeles) ---
    // El disco vive en un sprite propio de DISC_SIZE x DISC_SIZE, pegado en
    // (DISC_X, DISC_Y) de la pantalla. Las coordenadas locales del sprite
    // tienen el centro en (DISC_R_MAX + margen), ver CENTER.
    static const int DISC_X    = 20;
    static const int DISC_Y    = 18;
    static const int DISC_SIZE = 200;   // sprite 200x200 -> 20 KB a 4 bpp
    static const int CENTER    = DISC_SIZE / 2;
    static const int RING_MAX  = 88;    // radio del anillo exterior
    static const int CARD_R    = 95;    // radio donde van las letras N/E/S/O

    // Índices de la paleta de 16 colores del sprite (a 4 bpp el "color" que
    // reciben las primitivas ES el índice, no un RGB565).
    enum : uint8_t {
      C_BG = 0, C_RING, C_CROSS,
      C_TRAIL0,                      // 3..9: 7 bandas del trail, de tenue a viva
      C_TRAIL_TOP = C_TRAIL0 + 6,
      C_EDGE,                        // 10: borde de ataque del barrido
      C_BLIP,                        // 11: avión normal
      C_ALERT,                       // 12: avión dentro de RADAR_NEAR_KM
      C_PING,                        // 13: destello de detección
      C_HOME,                        // 14: casa en el centro
      C_LABEL                        // 15: etiquetas de anillos y cardinales
    };

    // Un blip del radar: el AircraftBlip compartido (zona tocable + copia del
    // avión, que es lo que consume hitTest) más lo que necesita la animación.
    // Va anidado acá para no meter campos de radar en AircraftBlip, que
    // también usa MapScreen.
    struct Blip {
      AircraftBlip base;
      int16_t  sx = 0, sy = 0;  // centro en coordenadas locales del sprite
      float    bearing = 0;     // grados, 0 = Norte, horario
      uint32_t pingMs = 0;      // cuándo lo cruzó el barrido por última vez
      bool     near = false;
    };

    DisplayManager& _display;
    TFT_eSprite     _disc;
    bool _discReady = false;   // el sprite se pudo asignar
    bool _triedInit = false;
    bool _chromeValid = false; // el marco fijo (barra, leyenda, panel) está dibujado

    std::vector<Blip> _blips; // se rellena en cada render()

    float    _sweepDeg = 0;      // ángulo actual del barrido (0 = Norte, horario)
    float    _prevSweepDeg = 0;  // ángulo del frame anterior (para detectar cruces)
    uint32_t _lastFrameMs = 0;

#if RADAR_DEBUG_TIMING
    uint32_t _statsMs = 0;
    uint32_t _frameCount = 0;
    uint32_t _frameCostSum = 0;
#endif

    void ensureDisc();
    void drawFrame();            // compone y vuelca un frame del disco
    void drawDiscSprite();       // camino con sprite (sin flicker)
    void drawDiscDirect();       // fallback sin sprite (solo la porción que cambia)
    void updatePings();          // marca los blips que el barrido acaba de cruzar
    void drawStaticDiscDirect(); // disco fijo del fallback, sobre el TFT
    void drawChrome();
    void drawPanel(const AircraftState* closest, int shown);

    // Punto sobre el círculo: 0 grados = Norte, sentido horario.
    static void polar(float deg, int r, int& x, int& y, int cx, int cy);
};
