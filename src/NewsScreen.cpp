#include "NewsScreen.h"
#include "TextUtils.h"

void NewsScreen::render(const NewsClient& news) {
  TFT_eSPI& tft = _display.tft();
  tft.fillScreen(TFT_BLACK);

  _display.showStatusBar("< HOME  NOTICIAS", "GNews", false);

  if (!news.hasData()) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_SILVER, TFT_BLACK);
    tft.drawString("No se pudieron traer", tft.width() / 2, tft.height() / 2 - 20, 2);
    tft.drawString("los titulares", tft.width() / 2, tft.height() / 2, 2);

    String why = news.lastError();
    if (why.length()) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString(why, tft.width() / 2, tft.height() / 2 + 26, 1);
    }
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Reintenta solo en 1 minuto", tft.width() / 2, tft.height() / 2 + 48, 1);
    return;
  }

  const auto& items = news.items();

  // Márgenes: la barra lateral de color come 14px, dejamos 8px del borde derecho
  const int textX = 14;
  const int textW = tft.width() - textX - 8;
  const int lineH = 17;   // alto de un renglón en fuente 2
  const int rowH  = 58;

  int y = 20;

  for (size_t i = 0; i < items.size(); i++) {
    if (y + rowH > tft.height()) break; // no dibujamos filas cortadas

    const NewsItem& it = items[i];

    tft.drawFastHLine(0, y, tft.width(), TFT_DARKGREY);

    // Acento de color a la izquierda, para separar visualmente los titulares
    tft.fillRect(4, y + 8, 3, 30, (i == 0) ? TFT_GREENYELLOW : TFT_DARKCYAN);

    // El titular se parte en hasta 2 renglones; si aun asi no entra, se recorta
    std::vector<String> lines = TextUtils::wrapToWidth(tft, it.title, textW, 2, 2);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    int ty = y + 6;
    for (const auto& line : lines) {
      tft.drawString(line, textX, ty, 2);
      ty += lineH;
    }

    // Medio y hora de publicación, pegados debajo del último renglón
    String meta = it.source;
    if (it.time.length()) {
      meta += meta.length() ? "  -  " : "";
      meta += it.time;
    }
    if (meta.length()) {
      tft.setTextColor(TFT_SILVER, TFT_BLACK);
      tft.drawString(TextUtils::fitToWidth(tft, meta, textW, 1), textX, ty + 2, 1);
    }

    y += rowH;
  }
}
