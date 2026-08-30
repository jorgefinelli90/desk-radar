#pragma once
#include "DisplayManager.h"
#include "UiRect.h"

// Pantalla de ajustes: qué red está usando, cómo entrar por el navegador y un
// botón para borrar la config de WiFi y volver al modo AP.
//
// El borrado pide dos toques: un toque accidental no puede dejar el
// dispositivo sin red.
class SettingsScreen {
  public:
    explicit SettingsScreen(DisplayManager& display) : _display(display) {}

    // Llamar al entrar: descarta una confirmación a medias que haya quedado.
    void onEnter();

    void render(const String& ssid, const String& ip, const String& hostname);

    // Devuelve true cuando el usuario confirmó el borrado (segundo toque).
    // Si es el primer toque, cambia el botón a "¿Seguro?" y devuelve false.
    bool handleTap(uint16_t x, uint16_t y);

    // Deja de esperar la confirmación si pasó demasiado tiempo. Devuelve true
    // si algo cambió y hay que redibujar.
    bool tick();

  private:
    DisplayManager& _display;
    UiRect _resetBtn = {0, 0, 0, 0};
    bool _confirming = false;
    uint32_t _confirmStartMs = 0;

    // Si el usuario toca una vez y se va, no queremos que el botón quede
    // armado esperando un segundo toque para siempre.
    static const uint32_t CONFIRM_TIMEOUT_MS = 6000;

    void drawResetButton();
};
