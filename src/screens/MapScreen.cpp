#include "MapScreen.h"
#include "GeoMap.h"
#include "config.h"
#include <math.h>
#include <algorithm>

// Rampa de altitud, misma idea que la escala de colores de FlightRadar24:
// naranja abajo, pasando por amarillo y verde, hasta cian/azul arriba.
// OpenSky devuelve metros; la aviación piensa en pies, así que los umbrales
// están en pies y la conversión se hace acá.
struct AltStop { float ft; uint16_t color; };
static const AltStop ALT_STOPS[] = {
  {     0.0f, 0xFB00 }, // naranja fuerte
  {  1000.0f, 0xFC80 }, // naranja
  {  4000.0f, 0xFFE0 }, // amarillo
  { 10000.0f, 0x87E0 }, // verde claro
  { 20000.0f, 0x07EF }, // verde/cian
  { 40000.0f, 0x04FF }, // cian/azul
};
static const int ALT_STOPS_N = sizeof(ALT_STOPS) / sizeof(ALT_STOPS[0]);

// Interpola en RGB565 entre los dos stops que rodean la altitud dada.
uint16_t MapScreen::altitudeColor(double meters) {
  float ft = (float)(meters * 3.28084);

  if (ft <= ALT_STOPS[0].ft) return ALT_STOPS[0].color;
  if (ft >= ALT_STOPS[ALT_STOPS_N - 1].ft) return ALT_STOPS[ALT_STOPS_N - 1].color;

  for (int i = 1; i < ALT_STOPS_N; i++) {
    if (ft > ALT_STOPS[i].ft) continue;

    const AltStop& a = ALT_STOPS[i - 1];
    const AltStop& b = ALT_STOPS[i];
    float t = (ft - a.ft) / (b.ft - a.ft);

    // Separar los canales, mezclar, y volver a armar
    int r1 = (a.color >> 11) & 0x1F, g1 = (a.color >> 5) & 0x3F, b1 = a.color & 0x1F;
    int r2 = (b.color >> 11) & 0x1F, g2 = (b.color >> 5) & 0x3F, b2 = b.color & 0x1F;
    int r = r1 + (int)lroundf((r2 - r1) * t);
    int g = g1 + (int)lroundf((g2 - g1) * t);
    int bl = b1 + (int)lroundf((b2 - b1) * t);
    return (uint16_t)((r << 11) | (g << 5) | bl);
  }

  return ALT_STOPS[ALT_STOPS_N - 1].color;
}

void MapScreen::onEnter() {
  _needsFullRedraw = true;
  _dirty.clear();
}

void MapScreen::toggleZoom() {
  _assetIdx = (_assetIdx + 1) % MAP_ASSET_COUNT;
  _needsFullRedraw = true;  // cambió la imagen de fondo entera
  _dirty.clear();
}

// Silueta de avión de ~12 px, rotada según el rumbo real.
// Local: la nariz mira a -Y (arriba = Norte). trackDeg 0 = Norte, horario.
void MapScreen::drawPlane(TFT_eSPI& tft, int x, int y, double trackDeg, uint16_t color) {
  float a = (float)(trackDeg * M_PI / 180.0);
  float ca = cosf(a), sa = sinf(a);

  // Rotación horaria en pantalla (Y crece hacia abajo)
  auto rx = [&](float lx, float ly) { return x + (int)lroundf(lx * ca - ly * sa); };
  auto ry = [&](float lx, float ly) { return y + (int)lroundf(lx * sa + ly * ca); };

  // Fuselaje
  tft.fillTriangle(rx(0, -6), ry(0, -6),
                   rx(-1.5f, 5), ry(-1.5f, 5),
                   rx(1.5f, 5), ry(1.5f, 5), color);

  // Alas en flecha
  tft.fillTriangle(rx(0, -1), ry(0, -1), rx(-6, 3), ry(-6, 3), rx(0, 2.5f), ry(0, 2.5f), color);
  tft.fillTriangle(rx(0, -1), ry(0, -1), rx(6, 3), ry(6, 3), rx(0, 2.5f), ry(0, 2.5f), color);

  // Estabilizador de cola
  tft.fillTriangle(rx(0, 3), ry(0, 3), rx(-2.5f, 6), ry(-2.5f, 6), rx(0, 5.5f), ry(0, 5.5f), color);
  tft.fillTriangle(rx(0, 3), ry(0, 3), rx(2.5f, 6), ry(2.5f, 6), rx(0, 5.5f), ry(0, 5.5f), color);
}

// Dibuja la casa y anota ella misma su zona a restaurar. Las dos cosas juntas a
// proposito: antes el rect se calculaba aparte en render() con (int)hx mientras
// el dibujo usaba lroundf(hx). Truncar y redondear difieren en un pixel, asi que
// el rect quedaba corrido y al repintar sobraba un pedazo del anillo blanco
// pegado al mapa. Con un solo entero para dibujar y para borrar no puede pasar.
void MapScreen::drawHome(TFT_eSPI& tft) {
  const MapAsset& asset = MAP_ASSETS[_assetIdx];
  float hx = GeoMap::screenX(HOME_LON, asset);
  float hy = GeoMap::screenY(HOME_LAT, asset);
  if (!GeoMap::inView(hx, hy)) return;

  const int ix = (int)lroundf(hx);   // coordenadas de la IMAGEN (las de _dirty)
  const int iy = (int)lroundf(hy);
  const int py = MAP_TOP + iy;       // y de PANTALLA

  // Anillo oscuro + punto amarillo: se distingue sobre cualquier fondo del mapa
  tft.drawCircle(ix, py, 5, TFT_BLACK);
  tft.drawCircle(ix, py, 4, TFT_WHITE);
  tft.fillCircle(ix, py, 3, TFT_YELLOW);

  // El circulo mas grande tiene radio 5 (ocupa ix-5..ix+5): un pixel de margen
  // de cada lado y el rect cubre el dibujo entero.
  const int R = 6;
  _dirty.push_back({ ix - R, iy - R, R * 2 + 1, R * 2 + 1 });
}

void MapScreen::drawLegend(TFT_eSPI& tft) {
  tft.fillRect(0, LEGEND_Y, tft.width(), LEGEND_H, TFT_BLACK);

  // Barra de gradiente: una columna de 1 px por cada paso de altitud
  const int barX = 4;
  const int barW = tft.width() - 8;
  const int barY = LEGEND_Y + 1;
  const int barH = 6;

  const float maxFt = ALT_STOPS[ALT_STOPS_N - 1].ft;
  for (int i = 0; i < barW; i++) {
    float ft = (float)i / barW * maxFt;
    tft.drawFastVLine(barX + i, barY, barH, altitudeColor(ft / 3.28084f));
  }

  // Marcas de referencia, en miles de pies
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  const int marks[] = { 0, 10000, 20000, 40000 };
  const char* labels[] = { "0", "10k", "20k", "40k ft" };
  for (int i = 0; i < 4; i++) {
    int mx = barX + (int)((marks[i] / maxFt) * barW);
    if (i == 3) {
      tft.setTextDatum(TR_DATUM);
      tft.drawString(labels[i], barX + barW, barY + barH + 1, 1);
    } else {
      tft.setTextDatum(TL_DATUM);
      tft.drawString(labels[i], mx, barY + barH + 1, 1);
    }
  }
}

void MapScreen::drawZoomButton(TFT_eSPI& tft) {
  tft.fillRect(_zoomBtn.x, _zoomBtn.y, _zoomBtn.w, _zoomBtn.h, TFT_NAVY);
  tft.drawFastHLine(_zoomBtn.x, _zoomBtn.y, _zoomBtn.w, TFT_BLUE);

  char label[40];
  int other = MAP_ASSETS[(_assetIdx + 1) % MAP_ASSET_COUNT].widthKm;
  snprintf(label, sizeof(label), "%d km   >   ver %d km",
           MAP_ASSETS[_assetIdx].widthKm, other);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_NAVY);
  tft.drawString(label, tft.width() / 2, _zoomBtn.y + _zoomBtn.h / 2, 2);
}

// Aviso compacto arriba del área del mapa. Va en dos renglones y no en el
// centro a propósito: los aviones se siguen dibujando encima, y un cartel
// grande en el medio los tapaba y quedaba ilegible (se vio en la placa).
void MapScreen::drawNoMapNotice(TFT_eSPI& tft) {
  const int h = 22;
  tft.fillRect(0, MAP_TOP, tft.width(), h, 0x2000); // rojo muy oscuro
  tft.drawFastHLine(0, MAP_TOP + h, tft.width(), TFT_MAROON);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_ORANGE, 0x2000);
  tft.drawString("Sin mapa de fondo", 6, MAP_TOP + 2, 1);
  tft.setTextColor(TFT_SILVER, 0x2000);
  tft.drawString("corre: pio run -t uploadfs", 6, MAP_TOP + 12, 1);
}

void MapScreen::render(std::vector<AircraftState>& aircraft) {
  TFT_eSPI& tft = _display.tft();
  const MapAsset& asset = MAP_ASSETS[_assetIdx];

  _zoomBtn = { 0, (int)tft.height() - ZOOM_BTN_H, (int)tft.width(), ZOOM_BTN_H };

  // --- Fondo ---------------------------------------------------------------
  if (!_tiles.ready()) {
    // Sin mapa seguimos mostrando los aviones, pero sobre negro. Acá NO se
    // puede usar pushRect para borrar los del frame anterior (no hay de dónde
    // leer), así que se limpia toda el área en cada render: si no, los aviones
    // van dejando rastro y _dirty crece sin fin.
    if (_needsFullRedraw) {
      tft.fillScreen(TFT_BLACK);
      drawLegend(tft);
      drawZoomButton(tft);
      _needsFullRedraw = false;
    }
    tft.fillRect(0, MAP_TOP, tft.width(), MAP_VIEW_H, TFT_BLACK);
    drawNoMapNotice(tft);
    _dirty.clear();
  } else if (_needsFullRedraw) {
    tft.fillScreen(TFT_BLACK);
    _tiles.pushFull(_assetIdx, MAP_TOP);
    drawLegend(tft);
    drawZoomButton(tft);
    _needsFullRedraw = false;
    _dirty.clear();
  } else {
    // Solo restauramos las zonas que ensuciamos en el frame anterior: releer los
    // 125 KB completos en cada refresco daría un repintado visible cada 5 s en
    // modo rápido.
    for (const auto& r : _dirty) {
      _tiles.pushRect(_assetIdx, r, MAP_TOP);
    }
    _dirty.clear();
  }

  // --- Casa ----------------------------------------------------------------
  // drawHome() anota tambien su zona a restaurar: el rect y el dibujo tienen
  // que salir del mismo redondeo (ver el comentario de la funcion).
  drawHome(tft);

  // --- Aviones -------------------------------------------------------------
  std::sort(aircraft.begin(), aircraft.end(),
            [](const AircraftState& a, const AircraftState& b) {
              return a.distanceKm < b.distanceKm;
            });

  _blips.clear();
  const AircraftState* closest = nullptr;
  int closestX = 0, closestY = 0;
  int shown = 0;

  for (auto& a : aircraft) {
    if (a.onGround) continue;
    if (_blips.size() >= MAX_PLANES) break;

    float fx = GeoMap::screenX(a.lon, asset);
    float fy = GeoMap::screenY(a.lat, asset);
    if (!GeoMap::inView(fx, fy)) continue;   // fuera del recorte del mapa

    int px = (int)lroundf(fx);
    int py = MAP_TOP + (int)lroundf(fy);

    drawPlane(tft, px, py, a.trackDeg, altitudeColor(a.baroAltitudeM));

    // Zona a restaurar: la silueta rotada entra en un cuadrado de 16 px
    _dirty.push_back({ px - 8, (int)lroundf(fy) - 8, 16, 16 });

    // Zona tocable más grande que el glifo: con touch resistivo y dedo,
    // un blanco de 12 px es imposible de acertar.
    const int TOUCH_PAD = 12;
    AircraftBlip blip;
    blip.hitBox = { px - TOUCH_PAD, py - TOUCH_PAD, TOUCH_PAD * 2, TOUCH_PAD * 2 };
    blip.aircraft = a;
    _blips.push_back(blip);

    if (!closest) { closest = &a; closestX = px; closestY = py; }
    shown++;
  }

  // --- Callsign del más cercano --------------------------------------------
  // Solo uno: a 240 px de ancho, diez etiquetas de 8 caracteres se pisan entre
  // sí y tapan el mapa. El resto se consulta tocando el avión.
  if (closest) {
    String cs = closest->callsign.length() ? closest->callsign : closest->icao24;
    int tw = tft.textWidth(cs, 1) + 4;
    int tx = closestX + 10;
    int ty = closestY - 4;
    if (tx + tw > MAP_VIEW_W) tx = closestX - 10 - tw; // se salía por la derecha
    if (tx < 0) tx = 0;
    if (ty < MAP_TOP) ty = MAP_TOP;
    if (ty + 10 > MAP_TOP + MAP_VIEW_H) ty = MAP_TOP + MAP_VIEW_H - 10;

    // Fondo opaco: sobre el mapa, texto sin fondo se vuelve ilegible
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.fillRect(tx, ty, tw, 10, TFT_BLACK);
    tft.drawString(cs, tx + 2, ty + 1, 1);

    _dirty.push_back({ tx, ty - MAP_TOP, tw, 10 });
  }

  char right[20];
  snprintf(right, sizeof(right), "%d aviones", shown);
  _display.showStatusBar("< HOME  MAPA", right, false);
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
