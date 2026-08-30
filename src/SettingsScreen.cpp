#include "SettingsScreen.h"
#include "config.h"

void SettingsScreen::onEnter() {
  _confirming = false;
}

void SettingsScreen::render(const String& ssid, const String& ip,
                            const String& hostname) {
  TFT_eSPI& tft = _display.tft();
  tft.fillScreen(TFT_BLACK);

  _display.showStatusBar("< HOME  AJUSTES", "", false);

  int y = 30;
  auto row = [&](const char* label, const String& value, uint16_t color) {
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString(label, 12, y, 1);
    tft.setTextColor(color, TFT_BLACK);
    tft.drawString(value.length() ? value : String("-"), 12, y + 12, 2);
    y += 40;
  };

  row("RED WIFI", ssid, TFT_WHITE);
  row("IP LOCAL", ip, TFT_WHITE);
  row("DESDE EL NAVEGADOR", "http://" + hostname + ".local/", TFT_CYAN);

  tft.drawFastHLine(12, y - 6, tft.width() - 24, TFT_DARKGREEN);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Tambien entra por la IP si el .local", 12, y + 2, 1);
  tft.drawString("no te resuelve (Android viejo).", 12, y + 14, 1);

  // Botón de reset abajo de todo, lejos del resto, para que no se toque solo
  const int btnH = 46;
  _resetBtn = { 12, (int)tft.height() - btnH - 12, (int)tft.width() - 24, btnH };
  drawResetButton();
}

void SettingsScreen::drawResetButton() {
  TFT_eSPI& tft = _display.tft();
  const UiRect& r = _resetBtn;

  uint16_t fill   = _confirming ? TFT_RED : TFT_MAROON;
  uint16_t border = _confirming ? TFT_ORANGE : TFT_RED;

  tft.fillRoundRect(r.x, r.y, r.w, r.h, 10, fill);
  tft.drawRoundRect(r.x, r.y, r.w, r.h, 10, border);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, fill);

  if (_confirming) {
    tft.drawString("Tocar otra vez para borrar", r.x + r.w / 2, r.y + r.h / 2 - 8, 2);
    tft.setTextColor(TFT_YELLOW, fill);
    tft.drawString("se reinicia en modo AP", r.x + r.w / 2, r.y + r.h / 2 + 12, 1);
  } else {
    tft.drawString("Reiniciar config WiFi", r.x + r.w / 2, r.y + r.h / 2 - 7, 2);
    tft.setTextColor(TFT_PINK, fill);
    tft.drawString("pide confirmacion", r.x + r.w / 2, r.y + r.h / 2 + 12, 1);
  }
}

bool SettingsScreen::handleTap(uint16_t x, uint16_t y) {
  if (!_resetBtn.contains(x, y)) {
    // Tocar fuera del botón cancela la confirmación pendiente
    if (_confirming) {
      _confirming = false;
      drawResetButton();
    }
    return false;
  }

  if (!_confirming) {
    _confirming = true;
    _confirmStartMs = millis();
    drawResetButton();
    return false; // primer toque: solo arma la confirmación
  }

  return true; // segundo toque sobre el botón ya armado
}

bool SettingsScreen::tick() {
  if (!_confirming) return false;
  if (millis() - _confirmStartMs < CONFIRM_TIMEOUT_MS) return false;

  _confirming = false;
  drawResetButton();
  return true;
}
