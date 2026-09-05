#include "screens/AirportScreen.h"
#include <algorithm>

void AirportScreen::render(const AirportDef& airport,
                            std::vector<AircraftState>& aircraft,
                            const String& status, uint16_t statusColor) {
  TFT_eSPI& tft = _display.tft();
  tft.fillScreen(TFT_BLACK);

  String leftLabel = String("< HOME  ") + airport.name;
  _display.showStatusBar(leftLabel, status, statusColor);

  // Reservamos una franja abajo para el botón "Siguiente >"
  const int btnH = 34;
  _nextBtn = { 0, tft.height() - btnH, tft.width(), btnH };
  int listBottom = _nextBtn.y;

  // Filtrar por altitud baja: "operando cerca del aeropuerto"
  std::vector<const AircraftState*> relevant;
  for (auto& a : aircraft) {
    if (a.onGround) continue;
    if (a.baroAltitudeM <= AIRPORT_MAX_ALT_M) {
      relevant.push_back(&a);
    }
  }

  std::sort(relevant.begin(), relevant.end(),
            [](const AircraftState* a, const AircraftState* b) {
              return a->baroAltitudeM < b->baroAltitudeM;
            });

  tft.setTextDatum(TL_DATUM);
  int y = 24;
  const int rowH = 34;

  if (relevant.empty()) {
    tft.setTextColor(TFT_SILVER, TFT_BLACK);
    tft.drawString("Sin trafico bajo detectado", 8, y + 10, 2);
  }

  for (size_t i = 0; i < relevant.size() && y + rowH < listBottom; i++) {
    const AircraftState* a = relevant[i];
    String cs = a->callsign.length() ? a->callsign : a->icao24;

    tft.drawFastHLine(0, y, tft.width(), TFT_DARKGREEN);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(cs, 8, y + 4, 2);

    char info[48];
    snprintf(info, sizeof(info), "alt %.0fm  %.0fkm/h  %.0fkm de vos",
              a->baroAltitudeM, a->velocityMs * 3.6, a->distanceKm);
    tft.setTextColor(TFT_SILVER, TFT_BLACK);
    tft.drawString(info, 8, y + 20, 1);

    y += rowH;
  }

  // Botón táctil para rotar al siguiente aeropuerto
  tft.fillRect(_nextBtn.x, _nextBtn.y, _nextBtn.w, _nextBtn.h, TFT_NAVY);
  tft.drawFastHLine(_nextBtn.x, _nextBtn.y, _nextBtn.w, TFT_BLUE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_NAVY);
  String nextLabel = String(airport.icao) + "   Siguiente aeropuerto >";
  tft.drawString(nextLabel, tft.width() / 2, _nextBtn.y + _nextBtn.h / 2, 2);
}
