#pragma once
#include <TFT_eSPI.h>

class TouchManager {
  public:
    explicit TouchManager(TFT_eSPI& tft) : _tft(tft) {}

    // Carga calibración guardada en NVS, o corre el wizard interactivo
    // (tocar 3 cruces en pantalla) si es la primera vez.
    void begin();

    // Devuelve true una sola vez por toque (flanco de bajada->presión),
    // con anti-rebote. Llena x,y en coordenadas de pantalla.
    bool getTap(uint16_t& x, uint16_t& y);

    // Rehace la calibración a pedido (botón en Ajustes) y la deja aplicada,
    // sin tener que reflashear con FORCE_TOUCH_CALIBRATION.
    void recalibrate();

  private:
    TFT_eSPI& _tft;
    uint16_t _calData[5] = {0, 0, 0, 0, 0};
    bool _wasTouched = false;
    uint32_t _lastTapMs = 0; // ventana anti-rebote: TOUCH_DEBOUNCE_MS en config.h

    bool loadCalibration();
    void saveCalibration();
    void runCalibrationWizard();
};
