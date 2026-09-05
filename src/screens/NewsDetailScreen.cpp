#include "screens/NewsDetailScreen.h"
#include "utils/TextUtils.h"

void NewsDetailScreen::render(const NewsItem& item) {
  TFT_eSPI& tft = _display.tft();
  tft.fillScreen(TFT_BLACK);

  _display.showStatusBar("< VOLVER  NOTICIA", "");

  const int margin = 10;
  const int textW  = tft.width() - margin * 2;

  // --- Titular ---
  // Hasta 5 renglones de fuente 2. Un titular de GNews rara vez pasa de tres,
  // pero los de los medios deportivos se estiran.
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  std::vector<String> titulo = TextUtils::wrapToWidth(tft, item.title, textW, 2, 5);
  int y = 24;
  for (const auto& linea : titulo) {
    tft.drawString(linea, margin, y, 2);
    y += 18;
  }

  // --- Medio y hora ---
  y += 4;
  tft.drawFastHLine(margin, y, textW, TFT_DARKGREEN);
  y += 7;

  String meta = item.source;
  if (item.time.length()) {
    meta += meta.length() ? "  -  " : "";
    meta += item.time;
  }
  if (meta.length()) {
    tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    tft.drawString(TextUtils::fitToWidth(tft, meta, textW, 1), margin, y, 1);
    y += 14;
  }

  // --- Resumen ---
  // Fuente 1 para que entre la mayor cantidad de texto posible. Lo que no entre
  // se corta con "..." en el último renglón: es un resumen, no el artículo.
  y += 4;
  if (item.summary.length()) {
    const int lineH    = 11;
    const int disponible = tft.height() - y - 16; // dejando lugar para el pie
    const int maxLines = disponible / lineH;

    tft.setTextColor(TFT_SILVER, TFT_BLACK);
    std::vector<String> cuerpo =
        TextUtils::wrapToWidth(tft, item.summary, textW, 1, maxLines);
    for (const auto& linea : cuerpo) {
      tft.drawString(linea, margin, y, 1);
      y += lineH;
    }
  } else {
    // GNews no siempre manda description: mejor decirlo que dejar el hueco.
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Esta noticia vino sin resumen.", margin, y, 1);
  }

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Toca para volver", tft.width() / 2, tft.height() - 4, 1);
}
