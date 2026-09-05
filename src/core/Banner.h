#pragma once
#include <Arduino.h>
#include "core/DisplayManager.h"

// Banner de mensaje superpuesto a la pantalla que esté activa.
//
// Entra deslizándose desde arriba, se queda unos segundos y se retira. No sabe
// nada de la pantalla de abajo: cuando termina avisa con needsRepaint() y quien
// lo usa vuelve a renderizar lo que corresponda. Es lo más simple que garantiza
// que no queden artefactos, sin gastar RAM en guardar el fondo (un buffer de
// 240x64 en 16 bits serían 30 KB).
class Banner {
  public:
    explicit Banner(DisplayManager& display) : _display(display) {}

    void show(const String& text);

    bool active() const { return _phase != Phase::Idle; }

    // Avanza la animación. No bloquea: vuelve enseguida si no toca frame.
    void tick();

    // true una sola vez, cuando el banner terminó y hay que repintar el fondo.
    bool takeRepaintRequest();

  private:
    enum class Phase { Idle, SlideIn, Hold, SlideOut };

    static const int HEIGHT = 66;

    DisplayManager& _display;
    String   _text;
    Phase    _phase = Phase::Idle;
    int      _y = -HEIGHT;      // borde superior del banner, en pantalla
    int      _lastY = -HEIGHT;
    uint32_t _phaseStart = 0;
    uint32_t _lastFrameMs = 0;
    bool     _repaintPending = false;

    void draw();
};
