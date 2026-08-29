#include "DisplayManager.h"

void DisplayManager::begin() {
  _tft.init();
  _tft.setRotation(0); // 240x320 vertical. Poner 1 o 3 si preferís horizontal.
  _tft.fillScreen(TFT_BLACK);
}

void DisplayManager::showMessage(const String& msg) {
  _tft.fillScreen(TFT_BLACK);
  _tft.setTextColor(TFT_WHITE, TFT_BLACK);
  _tft.setTextDatum(MC_DATUM);
  _tft.setTextSize(1);
  _tft.drawString(msg, _tft.width() / 2, _tft.height() / 2, 2);
}

void DisplayManager::showStatusBar(const String& left, const String& right, bool fastMode) {
  int barH = STATUS_BAR_HEIGHT;
  _tft.fillRect(0, 0, _tft.width(), barH, TFT_NAVY);
  _tft.setTextColor(TFT_WHITE, TFT_NAVY);
  _tft.setTextDatum(ML_DATUM);
  _tft.drawString(left, 4, barH / 2, 1);

  _tft.setTextDatum(MR_DATUM);
  uint16_t color = fastMode ? TFT_GREEN : TFT_SILVER;
  _tft.setTextColor(color, TFT_NAVY);
  _tft.drawString(right, _tft.width() - 4, barH / 2, 1);
}
