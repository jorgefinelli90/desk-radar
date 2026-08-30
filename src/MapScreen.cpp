#include "MapScreen.h"
#include "config.h"
#include <math.h>

// Contorno simplificado de la provincia de Buenos Aires, en sentido horario
// desde el noroeste. No es cartografía fina: son los ~23 puntos que hacen
// reconocible la silueta (delta del Paraná, Río de la Plata, Bahía
// Samborombón, Cabo San Antonio, costa atlántica, ría de Bahía Blanca,
// Patagones y el límite recto con La Pampa).
struct GeoPt { double lat; double lon; };

static const GeoPt BA_OUTLINE[] = {
  {-34.00, -63.40}, {-33.90, -62.10}, {-33.55, -60.90}, {-33.25, -60.20},
  {-33.70, -59.40}, {-34.10, -58.55}, {-34.60, -58.37}, {-34.95, -57.85},
  {-35.40, -57.15}, {-36.05, -57.35}, {-36.40, -56.75}, {-37.10, -56.70},
  {-38.00, -57.53}, {-38.55, -58.75}, {-38.90, -60.30}, {-39.05, -61.30},
  {-38.95, -62.10}, {-39.60, -62.30}, {-40.30, -62.30}, {-40.90, -62.95},
  {-39.30, -63.40}, {-37.50, -63.40}, {-35.60, -63.40},
};
static const int BA_OUTLINE_N = sizeof(BA_OUTLINE) / sizeof(BA_OUTLINE[0]);

struct CityRef { const char* name; double lat; double lon; };
static const CityRef CITIES[] = {
  { "BsAs",      -34.61, -58.38 },
  { "Junin",     -34.59, -60.94 },
  { "S.Nicolas", -33.34, -60.22 },
  { "Tandil",    -37.32, -59.13 },
  { "MdP",       -38.00, -57.55 },
  { "B.Blanca",  -38.72, -62.27 },
};
static const int CITIES_N = sizeof(CITIES) / sizeof(CITIES[0]);

static const size_t MAX_PLANES = 80; // tope de seguridad para el render/hit-test

void MapScreen::computeMapArea(TFT_eSPI& tft) {
  int availX = 4;
  int availY = DisplayManager::STATUS_BAR_HEIGHT + 4;
  int availW = tft.width() - 8;
  int availH = (tft.height() - 26) - availY - 2; // deja la franja inferior de leyenda

  // Aspecto real: 1 grado de longitud "mide" menos km que uno de latitud.
  double midLat = (MAP_BA_LAT_MIN + MAP_BA_LAT_MAX) / 2.0;
  double lonKm = (MAP_BA_LON_MAX - MAP_BA_LON_MIN) * 111.32 * cos(midLat * DEG_TO_RAD);
  double latKm = (MAP_BA_LAT_MAX - MAP_BA_LAT_MIN) * 111.32;
  double aspect = lonKm / latKm; // ancho / alto

  int w = availW;
  int h = (int)(w / aspect);
  if (h > availH) { h = availH; w = (int)(h * aspect); }

  _mapArea = { availX + (availW - w) / 2, availY + (availH - h) / 2, w, h };
}

int MapScreen::mapX(double lon) const {
  return _mapArea.x + (int)((lon - MAP_BA_LON_MIN) / (MAP_BA_LON_MAX - MAP_BA_LON_MIN) * _mapArea.w);
}

int MapScreen::mapY(double lat) const {
  // Latitud mayor (norte) arriba.
  return _mapArea.y + (int)((MAP_BA_LAT_MAX - lat) / (MAP_BA_LAT_MAX - MAP_BA_LAT_MIN) * _mapArea.h);
}

bool MapScreen::inBounds(double lat, double lon) const {
  return lat >= MAP_BA_LAT_MIN && lat <= MAP_BA_LAT_MAX &&
         lon >= MAP_BA_LON_MIN && lon <= MAP_BA_LON_MAX;
}

void MapScreen::drawOutline(TFT_eSPI& tft) {
  for (int i = 0; i < BA_OUTLINE_N; i++) {
    const GeoPt& a = BA_OUTLINE[i];
    const GeoPt& b = BA_OUTLINE[(i + 1) % BA_OUTLINE_N];
    tft.drawLine(mapX(a.lon), mapY(a.lat), mapX(b.lon), mapY(b.lat), TFT_DARKCYAN);
  }
}

void MapScreen::drawCities(TFT_eSPI& tft) {
  tft.setTextColor(TFT_SILVER, TFT_BLACK);
  for (int i = 0; i < CITIES_N; i++) {
    int px = mapX(CITIES[i].lon);
    int py = mapY(CITIES[i].lat);
    tft.fillCircle(px, py, 1, TFT_SILVER);

    // Ciudades del tercio este: etiqueta a la izquierda para no salirse del mapa.
    if (px > _mapArea.x + _mapArea.w * 3 / 5) {
      tft.setTextDatum(MR_DATUM);
      tft.drawString(CITIES[i].name, px - 3, py, 1);
    } else {
      tft.setTextDatum(ML_DATUM);
      tft.drawString(CITIES[i].name, px + 3, py, 1);
    }
  }
}

void MapScreen::drawPlane(TFT_eSPI& tft, int x, int y, double trackDeg, uint16_t color) {
  double h = (trackDeg - 90.0) * DEG_TO_RAD; // 0 grados = Norte = arriba
  const double s = 5.0;
  int nx = x + (int)lround(cos(h) * s);
  int ny = y + (int)lround(sin(h) * s);
  int lx = x + (int)lround(cos(h + 2.5) * s);
  int ly = y + (int)lround(sin(h + 2.5) * s);
  int rx = x + (int)lround(cos(h - 2.5) * s);
  int ry = y + (int)lround(sin(h - 2.5) * s);
  tft.fillTriangle(nx, ny, lx, ly, rx, ry, color);
}

void MapScreen::render(std::vector<AircraftState>& aircraft) {
  TFT_eSPI& tft = _display.tft();
  tft.fillScreen(TFT_BLACK);

  computeMapArea(tft);

  // Marco del mapa + contorno de la provincia + ciudades de referencia
  tft.drawRect(_mapArea.x, _mapArea.y, _mapArea.w, _mapArea.h, TFT_DARKGREEN);
  drawOutline(tft);
  drawCities(tft);

  // Tu casa
  int hx = mapX(HOME_LON);
  int hy = mapY(HOME_LAT);
  tft.fillTriangle(hx - 3, hy + 3, hx + 3, hy + 3, hx, hy - 4, TFT_YELLOW);

  // Aviones sobre la provincia
  _blips.clear();
  int shown = 0;
  for (auto& a : aircraft) {
    if (a.onGround) continue;
    if (!inBounds(a.lat, a.lon)) continue;
    if (_blips.size() >= MAX_PLANES) break;

    int px = mapX(a.lon);
    int py = mapY(a.lat);

    uint16_t color = TFT_GREEN;
    if (a.distanceKm <= RADAR_NEAR_KM)          color = TFT_RED;    // pasando cerca de casa
    else if (a.baroAltitudeM <= AIRPORT_MAX_ALT_M) color = TFT_YELLOW; // vuelo bajo (despega/aterriza)

    drawPlane(tft, px, py, a.trackDeg, color);

    // Zona tocable más grande que el glifo: con touch resistivo y dedo,
    // un blanco de 6px es imposible de acertar.
    const int TOUCH_PAD = 12;
    AircraftBlip blip;
    blip.hitBox = { px - TOUCH_PAD, py - TOUCH_PAD, TOUCH_PAD * 2, TOUCH_PAD * 2 };
    blip.aircraft = a;
    _blips.push_back(blip);

    shown++;
  }

  char right[20];
  snprintf(right, sizeof(right), "%d aviones", shown);
  _display.showStatusBar("< HOME  MAPA BA", right, false);

  // Franja inferior con la leyenda de colores
  int panelY = tft.height() - 26;
  tft.drawFastHLine(0, panelY, tft.width(), TFT_DARKGREEN);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Amarillo: vuelo bajo (<3000m)", 6, panelY + 4, 1);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("Rojo: a menos de 10 km de casa", 6, panelY + 15, 1);
}

const AircraftState* MapScreen::hitTest(uint16_t x, uint16_t y) const {
  const AircraftBlip* best = nullptr;
  long bestDistSq = 0;

  // Si varias zonas se superponen, gana la que tenga el centro más cerca del toque.
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
