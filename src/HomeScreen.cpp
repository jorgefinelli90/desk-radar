#include "HomeScreen.h"

void HomeScreen::render() {
  TFT_eSPI& tft = _display.tft();
  tft.fillScreen(TFT_BLACK);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("FLIGHT RADAR", tft.width() / 2, 40, 4);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Toca una opcion", tft.width() / 2, 66, 2);

  int margin = 20;
  int btnW = tft.width() - margin * 2;
  int btnH = 90;

  _radarBtn   = { margin, 100, btnW, btnH };
  _airportBtn = { margin, 100 + btnH + 20, btnW, btnH };

  tft.fillRoundRect(_radarBtn.x, _radarBtn.y, _radarBtn.w, _radarBtn.h, 12, TFT_DARKGREEN);
  tft.drawRoundRect(_radarBtn.x, _radarBtn.y, _radarBtn.w, _radarBtn.h, 12, TFT_GREEN);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
  tft.drawString("RADAR", _radarBtn.x + _radarBtn.w / 2, _radarBtn.y + _radarBtn.h / 2 - 10, 4);
  tft.setTextColor(TFT_GREENYELLOW, TFT_DARKGREEN);
  tft.drawString("Trafico cerca de casa", _radarBtn.x + _radarBtn.w / 2, _radarBtn.y + _radarBtn.h / 2 + 18, 1);

  tft.fillRoundRect(_airportBtn.x, _airportBtn.y, _airportBtn.w, _airportBtn.h, 12, TFT_NAVY);
  tft.drawRoundRect(_airportBtn.x, _airportBtn.y, _airportBtn.w, _airportBtn.h, 12, TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString("AEROPUERTOS", _airportBtn.x + _airportBtn.w / 2, _airportBtn.y + _airportBtn.h / 2 - 10, 4);
  tft.setTextColor(TFT_CYAN, TFT_NAVY);
  tft.drawString("Ezeiza / Aeroparque / Palomar", _airportBtn.x + _airportBtn.w / 2, _airportBtn.y + _airportBtn.h / 2 + 18, 1);
}

HomeChoice HomeScreen::hitTest(uint16_t x, uint16_t y) const {
  if (_radarBtn.contains(x, y)) return HomeChoice::Radar;
  if (_airportBtn.contains(x, y)) return HomeChoice::Airports;
  return HomeChoice::None;
}
