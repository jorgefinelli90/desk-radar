#include "core/DisplayManager.h"
#include "config.h"

void DisplayManager::begin() {
  _tft.init();
  _tft.setRotation(SCREEN_ROTATION); // ver config.h (0/2 = vertical, 1/3 = horizontal)
  _tft.fillScreen(TFT_BLACK);
}

void DisplayManager::showMessage(const String& msg) {
  _tft.fillScreen(TFT_BLACK);
  _tft.setTextColor(TFT_WHITE, TFT_BLACK);
  _tft.setTextDatum(MC_DATUM);
  _tft.setTextSize(1);
  _tft.drawString(msg, _tft.width() / 2, _tft.height() / 2, 2);
}

void DisplayManager::showStatusBar(const String& left, const String& right,
                                   uint16_t rightColor) {
  _statusLeft  = left;
  _statusRight = right;
  _statusColor = rightColor;

  int barH = STATUS_BAR_HEIGHT;
  _tft.fillRect(0, 0, _tft.width(), barH, TFT_NAVY);
  _tft.setTextColor(TFT_WHITE, TFT_NAVY);
  _tft.setTextDatum(ML_DATUM);
  _tft.drawString(left, 4, barH / 2, 1);

  int libre = drawStatusClock();

  _tft.setTextDatum(MR_DATUM);
  _tft.setTextColor(rightColor, TFT_NAVY);
  _tft.drawString(right, libre, barH / 2, 1);
}

// El reloj va pegado al borde derecho. Mientras NTP no haya sincronizado no se
// dibuja nada y el texto de la derecha se queda con todo el ancho: mostrar
// "00:00" sería peor que no mostrar la hora.
int DisplayManager::drawStatusClock() {
  const int borde = _tft.width() - 4;
  _clockRight = borde;
  _clockDrawn = _clock ? _clock->hhmm() : String("");

  if (_clockDrawn.length() == 0) return borde;

  _tft.setTextDatum(MR_DATUM);
  _tft.setTextColor(TFT_CYAN, TFT_NAVY);
  _tft.drawString(_clockDrawn, borde, STATUS_BAR_HEIGHT / 2, 1);

  return borde - _tft.textWidth(_clockDrawn, 1) - 6;
}

void DisplayManager::tickStatusClock() {
  if (!_clock) return;

  String ahora = _clock->hhmm();
  if (ahora.length() == 0 || ahora == _clockDrawn) return;

  // Primera hora después de sincronizar: hasta recién el texto de la derecha
  // venía usando el lugar del reloj, así que hay que rehacer la barra entera
  // una vez en vez de dibujar la hora encima.
  if (_clockDrawn.length() == 0) {
    showStatusBar(_statusLeft, _statusRight, _statusColor);
    return;
  }

  // Cambió el minuto: solo la esquina del reloj. Repintar la barra completa
  // haría parpadear el texto de la izquierda una vez por minuto.
  int w = _tft.textWidth(ahora, 1);
  _tft.fillRect(_clockRight - w - 2, 0, w + 6, STATUS_BAR_HEIGHT, TFT_NAVY);

  _tft.setTextDatum(MR_DATUM);
  _tft.setTextColor(TFT_CYAN, TFT_NAVY);
  _tft.drawString(ahora, _clockRight, STATUS_BAR_HEIGHT / 2, 1);
  _clockDrawn = ahora;
}
