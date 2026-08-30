#include "HomeScreen.h"

void HomeScreen::render() {
  TFT_eSPI& tft = _display.tft();
  tft.fillScreen(TFT_BLACK);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("FLIGHT RADAR", tft.width() / 2, 30, 4);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Toca una opcion", tft.width() / 2, 54, 2);

  // Cuatro botones tienen que entrar entre el subtitulo y el borde inferior de
  // los 320px, asi que son mas bajos y con menos aire que cuando eran tres.
  int margin = 20;
  int btnW = tft.width() - margin * 2;
  int btnH = 54;
  int gap = 8;
  int top = 70;

  _radarBtn   = { margin, top,                       btnW, btnH };
  _airportBtn = { margin, top + (btnH + gap),        btnW, btnH };
  _mapBtn     = { margin, top + (btnH + gap) * 2,    btnW, btnH };
  _infoBtn    = { margin, top + (btnH + gap) * 3,    btnW, btnH };

  auto drawBtn = [&](const UiRect& r, uint16_t fill, uint16_t border,
                     const char* title, uint16_t subColor, const char* subtitle) {
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 12, fill);
    tft.drawRoundRect(r.x, r.y, r.w, r.h, 12, border);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, fill);
    tft.drawString(title, r.x + r.w / 2, r.y + r.h / 2 - 10, 4);
    tft.setTextColor(subColor, fill);
    tft.drawString(subtitle, r.x + r.w / 2, r.y + r.h / 2 + 15, 1);
  };

  drawBtn(_radarBtn,   TFT_DARKGREEN, TFT_GREEN,   "RADAR",       TFT_GREENYELLOW, "Trafico cerca de casa");
  drawBtn(_airportBtn, TFT_NAVY,      TFT_BLUE,    "AEROPUERTOS", TFT_CYAN,        "Ezeiza / Aeroparque / Palomar");
  drawBtn(_mapBtn,     TFT_PURPLE,    TFT_MAGENTA, "MAPA BA",     TFT_PINK,        "Aviones sobre la provincia");
  drawBtn(_infoBtn,    TFT_MAROON,    TFT_ORANGE,  "MAS INFO",    TFT_ORANGE,      "Noticias y clima");
}

HomeChoice HomeScreen::hitTest(uint16_t x, uint16_t y) const {
  if (_radarBtn.contains(x, y)) return HomeChoice::Radar;
  if (_airportBtn.contains(x, y)) return HomeChoice::Airports;
  if (_mapBtn.contains(x, y)) return HomeChoice::Map;
  if (_infoBtn.contains(x, y)) return HomeChoice::Info;
  return HomeChoice::None;
}
