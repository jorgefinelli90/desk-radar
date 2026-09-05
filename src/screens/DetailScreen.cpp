#include "screens/DetailScreen.h"
#include "config.h"

const char* DetailScreen::cardinal(double deg) {
  static const char* dirs[] = {"N", "NE", "E", "SE", "S", "SO", "O", "NO"};
  int idx = (int)((deg + 22.5) / 45.0) % 8;
  return dirs[idx];
}

void DetailScreen::render(const AircraftState& a) {
  TFT_eSPI& tft = _display.tft();
  tft.fillScreen(TFT_BLACK);

  _display.showStatusBar("< VOLVER", "", false);

  String cs = a.callsign.length() ? a.callsign : a.icao24;

  // Encabezado con el callsign bien grande
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  tft.drawString(cs, tft.width() / 2, 46, 4);

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("ICAO24  " + a.icao24, tft.width() / 2, 70, 1);

  tft.drawFastHLine(10, 86, tft.width() - 20, TFT_DARKGREEN);

  // Filas de datos: etiqueta a la izquierda, valor a la derecha
  int y = 100;
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

  bool near = a.distanceKm <= RADAR_NEAR_KM;

  snprintf(buf, sizeof(buf), "%.1f km", a.distanceKm);
  row("Distancia", buf, near ? TFT_RED : TFT_WHITE);

  snprintf(buf, sizeof(buf), "%s  (%.0f)", cardinal(a.bearingDeg), a.bearingDeg);
  row("Rumbo", buf, TFT_WHITE);

  snprintf(buf, sizeof(buf), "%.0f m", a.baroAltitudeM);
  row("Altitud", buf, TFT_WHITE);

  snprintf(buf, sizeof(buf), "%.0f km/h", a.velocityMs * 3.6);
  row("Velocidad", buf, TFT_WHITE);

  snprintf(buf, sizeof(buf), "%.4f", a.lat);
  row("Latitud", buf, TFT_DARKGREY);

  snprintf(buf, sizeof(buf), "%.4f", a.lon);
  row("Longitud", buf, TFT_DARKGREY);

  if (near) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("PASANDO CERCA", tft.width() / 2, y + 16, 2);
  }

  // Pie: los datos son del último refresco, no vivos
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Datos del ultimo refresco", tft.width() / 2, tft.height() - 6, 1);
}
