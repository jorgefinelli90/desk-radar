#include "screens/SettingsScreen.h"
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

  // Los dos botones abajo de todo, lejos del resto, para que no se toquen solos.
  // El de borrar WiFi va último (el más peligroso, el más lejos del contenido).
  const int resetH = 46;
  const int calH   = 38;
  _resetBtn = { 12, (int)tft.height() - resetH - 12, (int)tft.width() - 24, resetH };
  _calBtn   = { 12, _resetBtn.y - calH - 8,          (int)tft.width() - 24, calH };
  drawCalButton();
  drawResetButton();
}

void SettingsScreen::drawCalButton() {
  TFT_eSPI& tft = _display.tft();
  const UiRect& r = _calBtn;

  tft.fillRoundRect(r.x, r.y, r.w, r.h, 10, TFT_NAVY);
  tft.drawRoundRect(r.x, r.y, r.w, r.h, 10, TFT_BLUE);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString("Recalibrar touch", r.x + r.w / 2, r.y + r.h / 2 - 6, 2);
  tft.setTextColor(TFT_CYAN, TFT_NAVY);
  tft.drawString("si tocas y responde al lado", r.x + r.w / 2, r.y + r.h / 2 + 11, 1);
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

SettingsAction SettingsScreen::handleTap(uint16_t x, uint16_t y) {
  // Cualquier toque que no sea sobre el botón de borrado cancela la
  // confirmación pendiente, incluido el de recalibrar.
  if (!_resetBtn.contains(x, y) && _confirming) {
    _confirming = false;
    drawResetButton();
  }

  if (_calBtn.contains(x, y)) {
    return SettingsAction::Recalibrate;
  }

  if (!_resetBtn.contains(x, y)) {
    return SettingsAction::None;
  }

  if (!_confirming) {
    _confirming = true;
    _confirmStartMs = millis();
    drawResetButton();
    return SettingsAction::None; // primer toque: solo arma la confirmación
  }

  return SettingsAction::ResetWifi; // segundo toque sobre el botón ya armado
}

bool SettingsScreen::tick() {
  if (!_confirming) return false;
  if (millis() - _confirmStartMs < CONFIRM_TIMEOUT_MS) return false;

  _confirming = false;
  drawResetButton();
  return true;
}
