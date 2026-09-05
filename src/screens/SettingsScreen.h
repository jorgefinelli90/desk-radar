#pragma once
#include "core/DisplayManager.h"
#include "models/UiRect.h"

// Qué pidió el usuario en la pantalla de ajustes.
enum class SettingsAction {
  None,
  ResetWifi,    // confirmado con dos toques
  Recalibrate,  // un solo toque: no es destructivo, se puede repetir
};

// Pantalla de ajustes: qué red está usando, cómo entrar por el navegador, un
// botón para recalibrar el touch y otro para borrar la config de WiFi.
//
// El borrado pide dos toques: un toque accidental no puede dejar el
// dispositivo sin red. La recalibración no: lo peor que pasa es que la repitas.
class SettingsScreen {
  public:
    explicit SettingsScreen(DisplayManager& display) : _display(display) {}

    // Llamar al entrar: descarta una confirmación a medias que haya quedado.
    void onEnter();

    void render(const String& ssid, const String& ip, const String& hostname);

    // Devuelve qué acción disparó el toque. Para el borrado de WiFi, el primer
    // toque solo cambia el botón a "¿Seguro?" y devuelve None.
    SettingsAction handleTap(uint16_t x, uint16_t y);

    // Deja de esperar la confirmación si pasó demasiado tiempo. Devuelve true
    // si algo cambió y hay que redibujar.
    bool tick();

  private:
    DisplayManager& _display;
    UiRect _calBtn = {0, 0, 0, 0};
    UiRect _resetBtn = {0, 0, 0, 0};
    bool _confirming = false;
    uint32_t _confirmStartMs = 0;

    // Si el usuario toca una vez y se va, no queremos que el botón quede
    // armado esperando un segundo toque para siempre.
    static const uint32_t CONFIRM_TIMEOUT_MS = 6000;

    void drawCalButton();
    void drawResetButton();
};
