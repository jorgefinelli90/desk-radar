#include "RadarScreen.h"
#include "config.h"
#include <math.h>
#include <algorithm>

const AircraftState* RadarScreen::hitTest(uint16_t x, uint16_t y) const {
  const RadarBlip* best = nullptr;
  long bestDistSq = 0;

  // Si varias zonas se superponen, gana la que tenga el centro más cerca del toque
  for (const auto& b : _blips) {
    if (!b.hitBox.contains(x, y)) continue;

    long dx = (long)x - (b.hitBox.x + b.hitBox.w / 2);
    long dy = (long)y - (b.hitBox.y + b.hitBox.h / 2);
    long distSq = dx * dx + dy * dy;

    if (!best || distSq < bestDistSq) {
      best = &b;
      bestDistSq = distSq;
    }
  }

  return best ? &best->aircraft : nullptr;
}

bool RadarScreen::hasNearbyTraffic(const std::vector<AircraftState>& aircraft) {
  for (auto& a : aircraft) {
    if (a.distanceKm <= RADAR_NEAR_KM) return true;
  }
  return false;
}

void RadarScreen::render(std::vector<AircraftState>& aircraft, bool fastMode) {
  TFT_eSPI& tft = _display.tft();
  tft.fillScreen(TFT_BLACK);

  _display.showStatusBar("< HOME  RADAR", fastMode ? "RAPIDO" : "NORMAL", fastMode);

  int cx = tft.width() / 2;
  int cy = 16 + (tft.height() - 16) / 2;
  int maxR = std::min(tft.width(), tft.height() - 16) / 2 - 10;

  // Anillos de referencia (25%, 50%, 75%, 100% del alcance)
  tft.drawCircle(cx, cy, maxR, TFT_DARKGREEN);
  tft.drawCircle(cx, cy, maxR * 3 / 4, TFT_DARKGREEN);
  tft.drawCircle(cx, cy, maxR / 2, TFT_DARKGREEN);
  tft.drawCircle(cx, cy, maxR / 4, TFT_DARKGREEN);

  // Cruz de referencia N-S / E-O
  tft.drawFastVLine(cx, cy - maxR, maxR * 2, TFT_DARKGREY);
  tft.drawFastHLine(cx - maxR, cy, maxR * 2, TFT_DARKGREY);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("N", cx, cy - maxR - 8, 1);

  // Casa en el centro
  tft.fillTriangle(cx - 4, cy + 4, cx + 4, cy + 4, cx, cy - 5, TFT_YELLOW);

  // Ordenar por distancia y quedarnos con el más cercano para el detalle
  std::sort(aircraft.begin(), aircraft.end(),
            [](const AircraftState& a, const AircraftState& b) {
              return a.distanceKm < b.distanceKm;
            });

  int shown = 0;
  const AircraftState* closest = nullptr;

  _blips.clear();

  for (auto& a : aircraft) {
    if (a.distanceKm > RADAR_RANGE_KM) continue;
    if (a.onGround) continue;

    double r = (a.distanceKm / RADAR_RANGE_KM) * maxR;
    double angleRad = (a.bearingDeg - 90) * PI / 180.0; // -90: 0 grados = arriba (Norte)

    int px = cx + (int)(r * cos(angleRad));
    int py = cy + (int)(r * sin(angleRad));

    bool near = a.distanceKm <= RADAR_NEAR_KM;
    uint16_t color = near ? TFT_RED : TFT_GREEN;

    tft.fillCircle(px, py, near ? 4 : 3, color);

    // Zona tocable más grande que el punto dibujado: con touch resistivo y
    // dedo, un blanco de 6px es imposible de acertar.
    const int TOUCH_PAD = 14;
    RadarBlip blip;
    blip.hitBox = { px - TOUCH_PAD, py - TOUCH_PAD, TOUCH_PAD * 2, TOUCH_PAD * 2 };
    blip.aircraft = a;
    _blips.push_back(blip);

    if (!closest) closest = &a;
    shown++;
  }

  // Panel inferior con info del avión más cercano
  int panelY = tft.height() - 46;
  tft.fillRect(0, panelY, tft.width(), 46, TFT_BLACK);
  tft.drawFastHLine(0, panelY, tft.width(), TFT_DARKGREEN);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  if (closest) {
    String cs = closest->callsign.length() ? closest->callsign : closest->icao24;
    char line1[48];
    snprintf(line1, sizeof(line1), "%s", cs.c_str());
    tft.drawString(line1, 6, panelY + 4, 2);

    char line2[64];
    snprintf(line2, sizeof(line2), "%.1f km  alt %.0f m  %.0f km/h",
              closest->distanceKm, closest->baroAltitudeM,
              closest->velocityMs * 3.6);
    tft.drawString(line2, 6, panelY + 24, 1);
  } else {
    tft.drawString("Sin trafico en rango", 6, panelY + 4, 2);
  }

  char countStr[24];
  snprintf(countStr, sizeof(countStr), "%d aviones", shown);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(countStr, tft.width() - 6, panelY + 4, 1);
}
