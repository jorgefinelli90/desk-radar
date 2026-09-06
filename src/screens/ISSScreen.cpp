#include "screens/ISSScreen.h"
#include "config.h"
#include "utils/GeoUtils.h"

void ISSScreen::render(const ISSClient& iss) {
  TFT_eSPI& tft = _display.tft();
  tft.fillScreen(TFT_BLACK);

  _display.showStatusBar("< HOME  ISS", "wheretheiss.at");

  if (!iss.hasData()) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_SILVER, TFT_BLACK);
    tft.drawString("No se pudo traer", tft.width() / 2, tft.height() / 2 - 20, 2);
    tft.drawString("la posicion de la ISS", tft.width() / 2, tft.height() / 2, 2);

    String why = iss.lastError();
    if (why.length()) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString(why, tft.width() / 2, tft.height() / 2 + 26, 1);
    }
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Reintenta solo en 1 minuto", tft.width() / 2, tft.height() / 2 + 48, 1);
    return;
  }

  const ISSPosition& p = iss.now();

  // Encabezado: nombre + estado de visibilidad (dia/noche orbital)
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("ESTACION ESPACIAL INTERNACIONAL", tft.width() / 2, 46, 2);

  const char* visTxt = (p.visibility == ISSVisibility::Daylight) ? "A la luz del sol"
                      : (p.visibility == ISSVisibility::Eclipsed) ? "En la sombra de la Tierra"
                      : "Visibilidad desconocida";
  uint16_t visColor = (p.visibility == ISSVisibility::Daylight) ? TFT_YELLOW
                     : (p.visibility == ISSVisibility::Eclipsed) ? TFT_NAVY
                     : TFT_DARKGREY;
  tft.setTextColor(visColor, TFT_BLACK);
  tft.drawString(visTxt, tft.width() / 2, 68, 1);

  // Icono simple: un circulito (la Tierra) con un punto orbitando (la ISS),
  // ubicado segun el rumbo desde casa. Nada de bitmaps, como el resto de las
  // pantallas.
  int cx = tft.width() / 2;
  int cy = 116;
  int r  = 30;
  tft.drawCircle(cx, cy, r, TFT_DARKGREEN);
  double rad = GeoUtils::toRad(p.bearingDeg);
  int sx = cx + (int)(r * sin(rad));
  int sy = cy - (int)(r * cos(rad));
  tft.fillCircle(sx, sy, 4, TFT_WHITE);
  tft.drawLine(cx, cy, sx, sy, TFT_DARKGREY);

  tft.drawFastHLine(10, 160, tft.width() - 20, TFT_DARKGREEN);

  // Filas de datos, mismo estilo que DetailScreen
  int y = 172;
  const int rowH = 30;
  char buf[40];

  auto row = [&](const char* label, const String& value, uint16_t valueColor) {
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_SILVER, TFT_BLACK);
    tft.drawString(label, 14, y + 6, 2);

    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(valueColor, TFT_BLACK);
    tft.drawString(value, tft.width() - 14, y + 4, 2);

    y += rowH;
  };

  snprintf(buf, sizeof(buf), "%.0f km", p.distanceKm);
  row("Distancia", buf, TFT_WHITE);

  snprintf(buf, sizeof(buf), "%s  (%.0f)", GeoUtils::cardinal(p.bearingDeg), p.bearingDeg);
  row("Rumbo", buf, TFT_WHITE);

  snprintf(buf, sizeof(buf), "%.0f km", p.altitudeKm);
  row("Altura orbital", buf, TFT_WHITE);

  snprintf(buf, sizeof(buf), "%.0f km/h", p.velocityKmh);
  row("Velocidad", buf, TFT_WHITE);

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Datos del ultimo refresco", tft.width() / 2, tft.height() - 6, 1);
}
