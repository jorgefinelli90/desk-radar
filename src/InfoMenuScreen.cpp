#include "InfoMenuScreen.h"

void InfoMenuScreen::render() {
  TFT_eSPI& tft = _display.tft();
  tft.fillScreen(TFT_BLACK);

  _display.showStatusBar("< HOME  MAS INFO", "", false);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Toca una opcion", tft.width() / 2, 34, 2);

  int margin = 20;
  int btnW = tft.width() - margin * 2;
  int btnH = 92;
  int gap = 18;
  int top = 56;

  _newsBtn    = { margin, top,                 btnW, btnH };
  _weatherBtn = { margin, top + btnH + gap,    btnW, btnH };

  // Mismo dibujo de botón que HomeScreen, para que las dos pantallas de menú
  // se sientan iguales al tacto y a la vista.
  auto drawBtn = [&](const UiRect& r, uint16_t fill, uint16_t border,
                     const char* title, uint16_t subColor, const char* subtitle) {
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 12, fill);
    tft.drawRoundRect(r.x, r.y, r.w, r.h, 12, border);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, fill);
    tft.drawString(title, r.x + r.w / 2, r.y + r.h / 2 - 12, 4);
    tft.setTextColor(subColor, fill);
    tft.drawString(subtitle, r.x + r.w / 2, r.y + r.h / 2 + 18, 1);
  };

  drawBtn(_newsBtn,    TFT_DARKCYAN, TFT_CYAN,   "NOTICIAS", TFT_WHITE, "Titulares de Argentina");
  drawBtn(_weatherBtn, TFT_OLIVE,    TFT_YELLOW, "CLIMA",    TFT_WHITE, "Ahora mismo en tu zona");

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Toca la barra de arriba para volver", tft.width() / 2, tft.height() - 6, 1);
}

InfoChoice InfoMenuScreen::hitTest(uint16_t x, uint16_t y) const {
  if (_newsBtn.contains(x, y)) return InfoChoice::News;
  if (_weatherBtn.contains(x, y)) return InfoChoice::Weather;
  return InfoChoice::None;
}
