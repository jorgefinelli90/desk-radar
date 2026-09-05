#include "screens/WeatherScreen.h"
#include "utils/TextUtils.h"
#include "config.h"
#include <math.h>

// --- Primitivas de los íconos ---------------------------------------------

void WeatherScreen::drawSun(TFT_eSPI& tft, int cx, int cy, int r, uint16_t color) {
  tft.fillCircle(cx, cy, r, color);
  // 8 rayos alrededor
  for (int i = 0; i < 8; i++) {
    double a = i * PI / 4.0;
    int x1 = cx + (int)((r + 4) * cos(a));
    int y1 = cy + (int)((r + 4) * sin(a));
    int x2 = cx + (int)((r + 11) * cos(a));
    int y2 = cy + (int)((r + 11) * sin(a));
    tft.drawLine(x1, y1, x2, y2, color);
    tft.drawLine(x1 + 1, y1, x2 + 1, y2, color); // 2px de grosor: se ve mejor
  }
}

void WeatherScreen::drawCloud(TFT_eSPI& tft, int cx, int cy, int w, uint16_t color) {
  int r = w / 4;
  tft.fillCircle(cx - r, cy, r * 3 / 4, color);
  tft.fillCircle(cx + r, cy, r * 3 / 4, color);
  tft.fillCircle(cx, cy - r / 2, r, color);
  tft.fillRoundRect(cx - w / 2, cy - r / 3, w, r * 4 / 3, r / 2, color);
}

void WeatherScreen::drawDrops(TFT_eSPI& tft, int cx, int cy, int count, int len, uint16_t color) {
  const int spacing = 16;
  int startX = cx - ((count - 1) * spacing) / 2;
  for (int i = 0; i < count; i++) {
    int x = startX + i * spacing;
    // Gotitas inclinadas, como cae la lluvia con viento
    tft.drawLine(x, cy, x - 4, cy + len, color);
    tft.drawLine(x + 1, cy, x - 3, cy + len, color);
  }
}

void WeatherScreen::drawFlakes(TFT_eSPI& tft, int cx, int cy, uint16_t color) {
  const int spacing = 18;
  for (int i = -1; i <= 1; i++) {
    int x = cx + i * spacing;
    int y = cy + ((i == 0) ? 8 : 0);
    tft.drawLine(x - 5, y, x + 5, y, color);
    tft.drawLine(x, y - 5, x, y + 5, color);
    tft.drawLine(x - 4, y - 4, x + 4, y + 4, color);
    tft.drawLine(x - 4, y + 4, x + 4, y - 4, color);
  }
}

void WeatherScreen::drawBolt(TFT_eSPI& tft, int cx, int cy, uint16_t color) {
  // Zigzag armado con dos triángulos
  tft.fillTriangle(cx + 5, cy,      cx - 6, cy + 17, cx + 3, cy + 17, color);
  tft.fillTriangle(cx + 3, cy + 15, cx + 12, cy + 11, cx - 2, cy + 32, color);
}

void WeatherScreen::drawIcon(TFT_eSPI& tft, WeatherIcon icon, int cx, int cy) {
  switch (icon) {
    case WeatherIcon::Sun:
      drawSun(tft, cx, cy, 20, TFT_YELLOW);
      break;

    case WeatherIcon::PartlyCloudy:
      drawSun(tft, cx - 16, cy - 14, 13, TFT_YELLOW);
      drawCloud(tft, cx + 8, cy + 10, 58, TFT_LIGHTGREY);
      break;

    case WeatherIcon::Cloud:
      drawCloud(tft, cx - 8, cy - 6, 50, TFT_DARKGREY);
      drawCloud(tft, cx + 6, cy + 8, 62, TFT_LIGHTGREY);
      break;

    case WeatherIcon::Fog:
      drawCloud(tft, cx, cy - 8, 60, TFT_DARKGREY);
      for (int i = 0; i < 3; i++) {
        int y = cy + 16 + i * 8;
        int half = (i % 2 == 0) ? 32 : 24;
        tft.drawFastHLine(cx - half, y, half * 2, TFT_SILVER);
        tft.drawFastHLine(cx - half, y + 1, half * 2, TFT_SILVER);
      }
      break;

    case WeatherIcon::Drizzle:
      drawCloud(tft, cx, cy - 10, 62, TFT_LIGHTGREY);
      drawDrops(tft, cx, cy + 14, 3, 7, TFT_CYAN);
      break;

    case WeatherIcon::Rain:
      drawCloud(tft, cx, cy - 12, 62, TFT_DARKGREY);
      drawDrops(tft, cx, cy + 12, 4, 14, TFT_CYAN);
      break;

    case WeatherIcon::Snow:
      drawCloud(tft, cx, cy - 12, 62, TFT_LIGHTGREY);
      drawFlakes(tft, cx, cy + 18, TFT_WHITE);
      break;

    case WeatherIcon::Storm:
      drawCloud(tft, cx, cy - 12, 62, TFT_DARKGREY);
      drawBolt(tft, cx, cy + 6, TFT_YELLOW);
      break;

    default:
      drawCloud(tft, cx, cy - 4, 58, TFT_DARKGREY);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_SILVER, TFT_BLACK);
      tft.drawString("?", cx, cy + 24, 4);
      break;
  }
}

uint16_t WeatherScreen::tempColor(double c) {
  if (c < 8)  return TFT_CYAN;
  if (c < 18) return TFT_WHITE;
  if (c < 27) return TFT_GREENYELLOW;
  if (c < 33) return TFT_ORANGE;
  return TFT_RED;
}

// --- Pantalla --------------------------------------------------------------

void WeatherScreen::render(const WeatherClient& weather) {
  TFT_eSPI& tft = _display.tft();
  tft.fillScreen(TFT_BLACK);

  _display.showStatusBar("< HOME  CLIMA", "Open-Meteo");

  if (!weather.hasData()) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_SILVER, TFT_BLACK);
    tft.drawString("No se pudo traer", tft.width() / 2, tft.height() / 2 - 20, 2);
    tft.drawString("el clima", tft.width() / 2, tft.height() / 2, 2);

    String why = weather.lastError();
    if (why.length()) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString(why, tft.width() / 2, tft.height() / 2 + 26, 1);
    }
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Reintenta solo en 1 minuto", tft.width() / 2, tft.height() / 2 + 48, 1);
    return;
  }

  const WeatherNow& w = weather.now();

  // Coordenadas de referencia (las de casa, de config.h)
  char coords[32];
  snprintf(coords, sizeof(coords), "%.3f, %.3f", HOME_LAT, HOME_LON);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString(coords, tft.width() / 2, 26, 1);

  drawIcon(tft, w.icon, tft.width() / 2, 78);

  // --- Temperatura grande ---
  // La fuente más grande cargada es la 4 (26px), así que la agrandamos x2.
  uint16_t tc = tempColor(w.tempC);
  char tempStr[8];
  snprintf(tempStr, sizeof(tempStr), "%d", (int)lround(w.tempC));

  tft.setTextSize(2);
  int numW = tft.textWidth(tempStr, 4);
  tft.setTextSize(1);
  int unitW = tft.textWidth("C", 4);

  const int gradeGap = 12;              // hueco para el circulito de grados
  int totalW = numW + gradeGap + unitW;
  int x0 = (tft.width() - totalW) / 2;
  const int tempTop = 124;

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(tc, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString(tempStr, x0, tempTop, 4);
  tft.setTextSize(1);

  // Símbolo de grado dibujado a mano: las fuentes ASCII no lo traen
  int degX = x0 + numW + 5;
  tft.drawCircle(degX, tempTop + 8, 4, tc);
  tft.drawCircle(degX, tempTop + 8, 3, tc);
  tft.drawString("C", x0 + numW + gradeGap, tempTop + 4, 4);

  // --- Sensación térmica ---
  char feels[40];
  snprintf(feels, sizeof(feels), "Sensacion %d", (int)lround(w.feelsLikeC));
  const int feelsY = 190;
  int feelsW = tft.textWidth(feels, 2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_SILVER, TFT_BLACK);
  tft.drawString(feels, tft.width() / 2, feelsY, 2);

  // grado + C chiquitos, pegados al número
  int fx = tft.width() / 2 + feelsW / 2 + 6;
  tft.drawCircle(fx, feelsY - 4, 2, TFT_SILVER);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("C", fx + 5, feelsY, 2);

  // --- Condición (despejado / nublado / lluvia / ...) ---
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  std::vector<String> condLines =
      TextUtils::wrapToWidth(tft, w.description, tft.width() - 16, 2, 2);
  int condY = 216;
  for (const auto& line : condLines) {
    tft.drawString(line, tft.width() / 2, condY, 2);
    condY += 18;
  }

  tft.drawFastHLine(20, 246, tft.width() - 40, TFT_DARKGREEN);

  // --- Humedad y viento, en el mismo estilo de filas que DetailScreen ---
  int y = 256;
  auto row = [&](const char* label, const String& value) {
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_SILVER, TFT_BLACK);
    tft.drawString(label, 20, y, 2);

    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(value, tft.width() - 20, y, 2);

    y += 24;
  };

  row("Humedad", String(w.humidityPct) + " %");
  row("Viento", String((int)lround(w.windKmh)) + " km/h");

  // Pie: hora de la medición que devolvió la API (ya viene en hora local)
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  String footer = w.observedAt.length() ? ("Medido a las " + w.observedAt)
                                        : String("Datos del ultimo refresco");
  tft.drawString(footer, tft.width() / 2, tft.height() - 5, 1);
}
