#include "TouchManager.h"
#include "config.h"
#include <Preferences.h>

static const char* PREFS_NAMESPACE = "touchcal";
static const char* PREFS_KEY = "data";
// La calibración solo vale para la rotación con la que se tomó (los datos que
// devuelve calibrateTouch mapean raw->pantalla ya orientada). Guardamos al lado
// con qué rotación se calibró: si cambia SCREEN_ROTATION, el wizard se vuelve a
// correr solo en vez de dejar el touch espejado.
static const char* PREFS_KEY_ROT = "rot";

bool TouchManager::loadCalibration() {
  Preferences prefs;
  prefs.begin(PREFS_NAMESPACE, true); // read-only
  bool ok = false;
  if (prefs.isKey(PREFS_KEY) && prefs.getBytesLength(PREFS_KEY) == sizeof(_calData) &&
      prefs.getUChar(PREFS_KEY_ROT, 0xFF) == SCREEN_ROTATION) {
    prefs.getBytes(PREFS_KEY, _calData, sizeof(_calData));
    ok = true;
  }
  prefs.end();
  return ok;
}

void TouchManager::saveCalibration() {
  Preferences prefs;
  prefs.begin(PREFS_NAMESPACE, false);
  prefs.putBytes(PREFS_KEY, _calData, sizeof(_calData));
  prefs.putUChar(PREFS_KEY_ROT, SCREEN_ROTATION);
  prefs.end();
}

void TouchManager::runCalibrationWizard() {
  _tft.fillScreen(TFT_BLACK);
  _tft.setTextColor(TFT_WHITE, TFT_BLACK);
  _tft.setTextDatum(MC_DATUM);
  _tft.drawString("Toca cada cruz", _tft.width() / 2, _tft.height() / 2 - 10, 2);
  _tft.drawString("para calibrar el touch", _tft.width() / 2, _tft.height() / 2 + 10, 2);
  _tft.drawString("Apunta al centro, sin mover el dedo", _tft.width() / 2, _tft.height() / 2 + 30, 1);
  delay(1800);

  // Bloquea hasta que el usuario toque las 3 cruces. Guarda resultado en _calData.
  _tft.calibrateTouch(_calData, TFT_MAGENTA, TFT_BLACK, 15);
}

void TouchManager::begin() {
  if (FORCE_TOUCH_CALIBRATION) {
    Serial.println("[Touch] FORCE_TOUCH_CALIBRATION=1: recalibro a proposito");
    runCalibrationWizard();
    saveCalibration();
  } else if (!loadCalibration()) {
    // Sin este log no hay forma de saber si el wizard salta una vez (esperado
    // al cambiar la rotacion) o en cada arranque (algo anda mal con la NVS).
    Serial.printf("[Touch] Sin calibracion valida para rotacion %u: corro el wizard\n",
                  SCREEN_ROTATION);
    runCalibrationWizard();
    saveCalibration();
  } else {
    Serial.printf("[Touch] Calibracion cargada de NVS (rotacion %u)\n", SCREEN_ROTATION);
  }
  _tft.setTouch(_calData);
}

void TouchManager::recalibrate() {
  runCalibrationWizard();
  saveCalibration();
  _tft.setTouch(_calData);

  // El dedo todavía puede estar apoyado sobre la última cruz: sin esto, al
  // levantarlo el flanco siguiente entra como un tap y activa lo que haya
  // debajo en la pantalla que se repinta atrás.
  _wasTouched = true;
  _lastTapMs = millis();
}

bool TouchManager::getTap(uint16_t& x, uint16_t& y) {
  uint16_t rawX = 0, rawY = 0;
  // Sin el tercer argumento, TFT_eSPI usa un umbral de presion de 600 y hay
  // que apretar bastante mas fuerte de lo que pidio el wizard. Ver config.h.
  bool touched = _tft.getTouch(&rawX, &rawY, TOUCH_PRESSURE);

  bool isNewTap = false;
  if (touched && !_wasTouched) {
    uint32_t now = millis();
    if (now - _lastTapMs > TOUCH_DEBOUNCE_MS) {
      x = rawX;
      y = rawY;
      isNewTap = true;
      _lastTapMs = now;
    }
  }
  _wasTouched = touched;
  return isNewTap;
}
