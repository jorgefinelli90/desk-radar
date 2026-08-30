#include "HomeScreen.h"

void HomeScreen::render() {
  TFT_eSPI& tft = _display.tft();
  tft.fillScreen(TFT_BLACK);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("FLIGHT RADAR", tft.width() / 2, 30, 4);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Toca una opcion", tft.width() / 2, 54, 2);

  // Cinco botones en 320 px de alto: cada vez que se suma uno hay que achicar
  // la grilla. Con 44 px de alto el titulo (fuente 4, 26 px) y el subtitulo
  // (fuente 1, 8 px) entran justos.
  int margin = 20;
  int btnW = tft.width() - margin * 2;
  int btnH = 44;
  int gap = 6;
  int top = 70;

  _radarBtn   = { margin, top,                       btnW, btnH };
  _airportBtn = { margin, top + (btnH + gap),        btnW, btnH };
  _mapBtn     = { margin, top + (btnH + gap) * 2,    btnW, btnH };
  _infoBtn    = { margin, top + (btnH + gap) * 3,    btnW, btnH };
  _settingsBtn= { margin, top + (btnH + gap) * 4,    btnW, btnH };

  auto drawBtn = [&](const UiRect& r, uint16_t fill, uint16_t border,
                     const char* title, uint16_t subColor, const char* subtitle) {
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 12, fill);
    tft.drawRoundRect(r.x, r.y, r.w, r.h, 12, border);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, fill);
    tft.drawString(title, r.x + r.w / 2, r.y + r.h / 2 - 8, 4);
    tft.setTextColor(subColor, fill);
    tft.drawString(subtitle, r.x + r.w / 2, r.y + r.h / 2 + 13, 1);
  };

  drawBtn(_radarBtn,   TFT_DARKGREEN, TFT_GREEN,   "RADAR",       TFT_GREENYELLOW, "Trafico cerca de casa");
  drawBtn(_airportBtn, TFT_NAVY,      TFT_BLUE,    "AEROPUERTOS", TFT_CYAN,        "Ezeiza / Aeroparque / Palomar");
  drawBtn(_mapBtn,     TFT_PURPLE,    TFT_MAGENTA, "MAPA BA",     TFT_PINK,        "Aviones sobre la provincia");
  drawBtn(_infoBtn,    TFT_MAROON,    TFT_ORANGE,  "MAS INFO",    TFT_ORANGE,      "Noticias y clima");
  drawBtn(_settingsBtn,0x2124,        TFT_DARKGREY,"AJUSTES",     TFT_SILVER,      "WiFi, IP y configuracion web");
}

HomeChoice HomeScreen::hitTest(uint16_t x, uint16_t y) const {
  if (_radarBtn.contains(x, y)) return HomeChoice::Radar;
  if (_airportBtn.contains(x, y)) return HomeChoice::Airports;
  if (_mapBtn.contains(x, y)) return HomeChoice::Map;
  if (_infoBtn.contains(x, y)) return HomeChoice::Info;
  if (_settingsBtn.contains(x, y)) return HomeChoice::Settings;
  return HomeChoice::None;
}
