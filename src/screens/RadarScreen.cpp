#include "screens/RadarScreen.h"
#include "config.h"
#include "utils/GeoUtils.h"
#include <math.h>
#include <string.h>
#include <algorithm>

namespace {

void formatTrafficCountLabel(int shown, char* out, size_t outSize) {
  snprintf(out, outSize, "%d avion%s", shown, shown == 1 ? "" : "es");
}

}  // namespace

// El mapa de fondo se genera con el alcance que dice MapAssets.h. Si alguien
// cambia RADAR_RANGE_KM en config.h y no regenera el mapa, los anillos y el
// mapa quedan a distinta escala y los aviones caen sobre calles que no son.
// Mejor que directamente no compile.
static_assert((int)RADAR_RANGE_KM == RADAR_MAP_RANGE_KM,
              "RADAR_RANGE_KM (config.h) no coincide con el mapa generado. "
              "Corre tools/build-map.mjs despues de cambiarlo.");

// Lo mismo con el CENTRO, que era el ultimo dato del pipeline que se podia
// desincronizar en silencio: los .bin son un recorte alrededor de un punto fijo,
// asi que si alguien mueve la casa en config.h y no regenera, GeoMap sigue
// proyectando contra el origen viejo y los aviones caen sobre calles que no son.
// El firmware compilaba igual y no habia forma de notarlo mirando la pantalla.
//
// La tolerancia es medio pixel del mapa mas fino (el del radar: 200 px para
// 40 km de ancho, o sea ~200 m por pixel), asi un reajuste de decimales que no
// mueve nada visible no rompe el build, pero cambiar de barrio si. Se compara
// con dos restas en vez de fabs() porque fabs no es constexpr.
static constexpr double MAP_CENTER_TOLERANCE_DEG = 0.0005; // ~55 m en latitud

static_assert(HOME_LAT - MAP_ORIGIN_LAT <  MAP_CENTER_TOLERANCE_DEG &&
              HOME_LAT - MAP_ORIGIN_LAT > -MAP_CENTER_TOLERANCE_DEG,
              "HOME_LAT (config.h) no coincide con el centro del mapa generado. "
              "Corre tools/build-map.mjs despues de cambiarlo.");

static_assert(HOME_LON - MAP_ORIGIN_LON <  MAP_CENTER_TOLERANCE_DEG &&
              HOME_LON - MAP_ORIGIN_LON > -MAP_CENTER_TOLERANCE_DEG,
              "HOME_LON (config.h) no coincide con el centro del mapa generado. "
              "Corre tools/build-map.mjs despues de cambiarlo.");

// Paleta de 16 colores del disco (sprite a 4 bpp; a esa profundidad el "color"
// que reciben las primitivas ES el indice). Se arma en runtime porque los
// indices 1..5 son los grises del mapa, que salen del histograma real de la
// imagen y llegan en RADAR_MAP_GREYS.
//
// Al radar le quedan 10 slots: de ahi que la estela tenga 3 bandas y no 7.
void RadarScreen::buildPalette() {
  _palette[C_BG] = 0x0000; // negro: fuera del circulo del mapa

  for (int i = 0; i < RADAR_MAP_GREY_COUNT && i < 5; i++) {
    _palette[C_MAP0 + i] = RADAR_MAP_GREYS[i];
  }

  // Los anillos van bastante mas vivos que cuando el fondo era negro: ahora
  // tienen que leerse por encima del gris mas claro del mapa.
  _palette[C_RING]       = 0x0640; // verde medio
  _palette[C_TRAIL0]     = 0x0300; // estela tenue
  _palette[C_TRAIL0 + 1] = 0x0540;
  _palette[C_TRAIL_TOP]  = 0x0760; // estela viva
  _palette[C_EDGE]       = 0x8FF1; // borde de ataque, verde casi blanco
  _palette[C_BLIP]       = 0x07E0; // verde puro
  _palette[C_ALERT]      = 0xF800; // rojo
  _palette[C_PING]       = 0xFFFF; // blanco
  _palette[C_HOME]       = 0xFFE0; // amarillo
  _palette[C_LABEL]      = 0xC618; // plata
}

// El Palomar es el unico punto de referencia rotulado: el mapa de fondo se
// genera sin la capa de etiquetas de Esri a proposito.
//
// Se ubica por distancia+rumbo igual que los aviones, y no por Mercator, para
// que comparta exactamente el sistema de coordenadas de los blips. A 20 km de
// alcance las dos proyecciones difieren menos de un pixel.
void RadarScreen::computePalomar() {
  const AirportDef* palomar = nullptr;
  for (const auto& airport : AIRPORTS) {
    if (strcmp(airport.icao, "SADP") == 0) {
      palomar = &airport;
      break;
    }
  }
  if (!palomar) { _palomarInView = false; return; }

  double d = GeoUtils::distanceKm(HOME_LAT, HOME_LON, palomar->lat, palomar->lon);
  double b = GeoUtils::bearingDeg(HOME_LAT, HOME_LON, palomar->lat, palomar->lon);

  if (d > RADAR_RANGE_KM) { _palomarInView = false; return; }

  int r = (int)lround((d / RADAR_RANGE_KM) * RING_MAX);
  polar((float)b, r, _palomarX, _palomarY, CENTER, CENTER);
  _palomarInView = true;
}

void RadarScreen::polar(float deg, int r, int& x, int& y, int cx, int cy) {
  float a = deg * (float)PI / 180.0f;
  x = cx + (int)lroundf(r * sinf(a));   // 0 grados = arriba (Norte)
  y = cy - (int)lroundf(r * cosf(a));   // y crece hacia abajo
}

const AircraftState* RadarScreen::hitTest(uint16_t x, uint16_t y) const {
  const Blip* best = nullptr;
  long bestDistSq = 0;

  // Si varias zonas se superponen, gana la que tenga el centro más cerca del toque
  for (const auto& b : _blips) {
    if (!b.base.hitBox.contains(x, y)) continue;

    long dx = (long)x - (b.base.hitBox.x + b.base.hitBox.w / 2);
    long dy = (long)y - (b.base.hitBox.y + b.base.hitBox.h / 2);
    long distSq = dx * dx + dy * dy;

    if (!best || distSq < bestDistSq) {
      best = &b;
      bestDistSq = distSq;
    }
  }

  return best ? &best->base.aircraft : nullptr;
}

bool RadarScreen::hasNearbyTraffic(const std::vector<AircraftState>& aircraft) {
  for (const auto& a : aircraft) {
    if (a.distanceKm <= RADAR_NEAR_KM) return true;
  }
  return false;
}

void RadarScreen::onEnter() {
  _chromeValid = false;   // venimos de otra pantalla: hay que limpiar y redibujar
  _sweepDeg = 0;
  _prevSweepDeg = 0;
  _lastFrameMs = millis();
#if RADAR_DEBUG_TIMING
  _statsMs = millis();    // si no, el primer promedio sale sobre un tramo raro
  _frameCount = 0;
  _frameCostSum = 0;
#endif
}

void RadarScreen::onExit() {
  if (_discReady) {
    _disc.deleteSprite();
    _discReady = false;
  }
  if (_mapRam) {
    free(_mapRam);
    _mapRam = nullptr;
  }
  _mapReady = false;

  // Para que ensureDisc() vuelva a asignar la proxima vez. La paleta y la
  // posicion de El Palomar se recalculan ahi mismo: son cuentas, no memoria.
  _triedInit = false;
  _chromeValid = false;

  Serial.printf("[Radar] Sprite y mapa liberados, heap libre %u, bloque mayor %u\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
}

// Asigna el sprite del disco la primera vez. A 4 bpp son (200*200)/2 = 20 KB.
// Un sprite de 16 bpp del mismo tamaño costaría 80 KB, demasiado para convivir
// con el handshake TLS de OpenSky. Si la asignación falla igual seguimos
// andando: drawDiscDirect() dibuja sin buffer.
void RadarScreen::ensureDisc() {
  if (_triedInit) return;
  _triedInit = true;

  buildPalette();
  computePalomar();

  _disc.setColorDepth(4);
  if (_disc.createSprite(DISC_SIZE, DISC_SIZE) != nullptr) {
    _disc.createPalette(_palette, 16);
    _discReady = true;
    Serial.printf("[Radar] Sprite del disco OK (%d bytes), heap libre %u\n",
                  (DISC_SIZE * DISC_SIZE) / 2, (unsigned)ESP.getFreeHeap());
  } else {
    _discReady = false;
    Serial.printf("[Radar] Sin RAM para el sprite, voy a redibujo directo. Heap %u\n",
                  (unsigned)ESP.getFreeHeap());
    return;
  }

  // Copia del mapa de fondo en RAM. Se lee una sola vez y despues cada frame la
  // vuelca al sprite con un memcpy: releer 20 KB de LittleFS 22 veces por
  // segundo seria 440 KB/s de flash para nada.
  _mapRam = (uint8_t*)malloc(RADAR_MAP_BYTES);
  if (!_mapRam) {
    Serial.printf("[Radar] Sin RAM para el mapa de fondo (%u bytes), queda en negro\n",
                  (unsigned)RADAR_MAP_BYTES);
    _mapReady = false;
    return;
  }

  _mapReady = _tiles.loadRaw(RADAR_MAP.path, _mapRam, RADAR_MAP_BYTES);
  if (_mapReady) {
    Serial.printf("[Radar] Mapa de fondo cargado (%u bytes), heap libre %u\n",
                  (unsigned)RADAR_MAP_BYTES, (unsigned)ESP.getFreeHeap());
  } else {
    free(_mapRam);
    _mapRam = nullptr;
    Serial.println("[Radar] No se pudo cargar el mapa de fondo, el disco queda negro");
  }
}

// ---------------------------------------------------------------------------
//  Marco fijo: barra de estado, leyenda y panel inferior. Solo se redibuja al
//  entrar a la pantalla, no en cada frame ni en cada refresco de datos.
// ---------------------------------------------------------------------------
void RadarScreen::drawChrome() {
  TFT_eSPI& tft = _display.tft();
  tft.fillScreen(TFT_BLACK);

  // Leyenda de colores, debajo del disco y fuera de él
  const int legendY = 232;
  tft.fillCircle(16, legendY, 4, TFT_GREEN);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_SILVER, TFT_BLACK);
  tft.drawString("en rango", 26, legendY, 1);

  char alertLabel[20];
  snprintf(alertLabel, sizeof(alertLabel), "a menos de %.0f km", RADAR_NEAR_KM);
  tft.fillCircle(104, legendY, 4, TFT_RED);
  tft.drawString(alertLabel, 114, legendY, 1);

  tft.drawFastHLine(0, 248, tft.width(), TFT_DARKGREEN);
}

void RadarScreen::drawPanel(const AircraftState* closest, int shown) {
  TFT_eSPI& tft = _display.tft();
  char countStr[24];
  formatTrafficCountLabel(shown, countStr, sizeof(countStr));

  // Solo la franja del panel: no tocamos el disco ni la leyenda
  tft.fillRect(0, 250, tft.width(), tft.height() - 250, TFT_BLACK);

  tft.setTextDatum(TL_DATUM);

  if (closest) {
    const char* cs = closest->label();
    bool near = closest->distanceKm <= RADAR_NEAR_KM;

    tft.setTextColor(near ? TFT_RED : TFT_GREENYELLOW, TFT_BLACK);
    tft.drawString(cs, 8, 254, 4);

    char info[64];
    snprintf(info, sizeof(info), "%.1f km   alt %.0f m   %.0f km/h",
             closest->distanceKm, closest->baroAltitudeM,
             closest->velocityMs * 3.6);
    tft.setTextColor(TFT_SILVER, TFT_BLACK);
    tft.drawString(info, 8, 286, 1);
  } else {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Sin trafico en rango", 8, 262, 2);
  }

  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString(countStr, tft.width() - 8, 286, 1);
}

// ---------------------------------------------------------------------------
//  render(): se llama cuando llegan datos nuevos (cada 30s o 5s). Recalcula
//  los blips y repinta lo que no es el disco. El disco lo pinta drawFrame().
// ---------------------------------------------------------------------------
void RadarScreen::render(std::vector<AircraftState>& aircraft,
                         const String& status, uint16_t statusColor) {
  ensureDisc();

  if (!_chromeValid) {
    drawChrome();
    _chromeValid = true;
  }

  _display.showStatusBar("< HOME  RADAR", status, statusColor);

  // Ordenar por distancia: el primero en rango es el que va al panel inferior
  std::sort(aircraft.begin(), aircraft.end(),
            [](const AircraftState& a, const AircraftState& b) {
              return a.distanceKm < b.distanceKm;
            });

  // Conservamos los pings ya disparados para que un refresco de datos no corte
  // el destello a mitad de camino.
  auto previousPing = [&](const char* icao) -> uint32_t {
    for (const auto& old : _blips) {
      if (strcmp(old.base.aircraft.icao24, icao) == 0) return old.pingMs;
    }
    return 0;
  };

  std::vector<Blip> fresh;
  const AircraftState* closest = nullptr;
  int shown = 0;

  for (auto& a : aircraft) {
    if (a.distanceKm > RADAR_RANGE_KM) continue;
    if (a.onGround) continue;

    int r = (int)lround((a.distanceKm / RADAR_RANGE_KM) * RING_MAX);

    Blip b;
    b.bearing = (float)a.bearingDeg;
    b.near = a.distanceKm <= RADAR_NEAR_KM;
    b.pingMs = previousPing(a.icao24);

    int lx, ly;
    polar(b.bearing, r, lx, ly, CENTER, CENTER);
    b.sx = (int16_t)lx;
    b.sy = (int16_t)ly;

    // La zona tocable va en coordenadas de PANTALLA (el sprite es local), y es
    // más grande que el punto dibujado: con touch resistivo y dedo, un blanco
    // de 6px es imposible de acertar.
    const int TOUCH_PAD = 14;
    int scrX = DISC_X + lx;
    int scrY = DISC_Y + ly;
    b.base.hitBox = { scrX - TOUCH_PAD, scrY - TOUCH_PAD, TOUCH_PAD * 2, TOUCH_PAD * 2 };
    b.base.aircraft = a;

    fresh.push_back(b);

    if (!closest) closest = &a;
    shown++;
  }

  _blips.swap(fresh);

  drawPanel(closest, shown);

  // Sin sprite, el disco fijo (anillos, cruz, etiquetas, aviones) se pinta acá
  // una vez por refresco; después tick() solo mueve la línea del barrido.
  if (!_discReady) drawStaticDiscDirect();

  drawFrame();
}

// Disco completo dibujado directo sobre el TFT. Solo se usa en el fallback sin
// sprite: acá sí hay parpadeo, pero es el precio de no tener buffer.
void RadarScreen::drawStaticDiscDirect() {
  TFT_eSPI& tft = _display.tft();
  const int cx = DISC_X + CENTER;
  const int cy = DISC_Y + CENTER;

  tft.fillRect(DISC_X, DISC_Y, DISC_SIZE, DISC_SIZE, TFT_BLACK);

  for (int i = 1; i <= 4; i++) {
    tft.drawCircle(cx, cy, RING_MAX * i / 4, TFT_DARKGREEN);
  }
  tft.drawFastVLine(cx, cy - RING_MAX, RING_MAX * 2, TFT_DARKGREY);
  tft.drawFastHLine(cx - RING_MAX, cy, RING_MAX * 2, TFT_DARKGREY);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  for (int i = 1; i <= 4; i++) {
    int x, y;
    polar(45.0f, RING_MAX * i / 4, x, y, cx, cy);
    char lbl[8];
    snprintf(lbl, sizeof(lbl), "%.0f", RADAR_RANGE_KM * i / 4);
    tft.drawString(lbl, x, y, 1);
  }
  {
    int x, y;
    polar(0,   CARD_R, x, y, cx, cy); tft.drawString("N", x, y, 1);
    polar(90,  CARD_R, x, y, cx, cy); tft.drawString("E", x, y, 1);
    polar(180, CARD_R, x, y, cx, cy); tft.drawString("S", x, y, 1);
    polar(270, CARD_R, x, y, cx, cy); tft.drawString("O", x, y, 1);
  }

  for (const auto& b : _blips) {
    tft.fillCircle(DISC_X + b.sx, DISC_Y + b.sy, b.near ? 4 : 3,
                   b.near ? TFT_RED : TFT_GREEN);
  }

  tft.fillTriangle(cx - 4, cy + 4, cx + 4, cy + 4, cx, cy - 5, TFT_YELLOW);
}

// ---------------------------------------------------------------------------
//  Animación
// ---------------------------------------------------------------------------
void RadarScreen::tick() {
  uint32_t now = millis();
  uint32_t dt = now - _lastFrameMs;
  if (dt < RADAR_FRAME_MS) return;   // todavía no toca frame: volvemos ya

  _lastFrameMs = now;

  // El avance se calcula con el dt real, así la vuelta tarda SWEEP_PERIOD_MS
  // aunque un frame se retrase por un fetch o por el touch.
  if (dt > 500) dt = 500;            // tras un fetch largo, no pegar un salto feo
  _prevSweepDeg = _sweepDeg;
  _sweepDeg += 360.0f * (float)dt / (float)RADAR_SWEEP_PERIOD_MS;
  while (_sweepDeg >= 360.0f) _sweepDeg -= 360.0f;

  updatePings();

#if RADAR_DEBUG_TIMING
  uint32_t t0 = micros();
  drawFrame();
  uint32_t cost = micros() - t0;

  _frameCostSum += cost;
  _frameCount++;
  if (now - _statsMs >= 2000) {
    Serial.printf("[Radar] %u fps, %.1f ms por frame, %.1f grados por frame\n",
                  (unsigned)_frameCount / 2,
                  _frameCostSum / 1000.0f / _frameCount,
                  360.0f * (2000.0f / _frameCount) / RADAR_SWEEP_PERIOD_MS);
    _statsMs = now;
    _frameCount = 0;
    _frameCostSum = 0;
  }
#else
  drawFrame();
#endif
}

// Marca los aviones que el barrido acaba de cruzar en este frame.
void RadarScreen::updatePings() {
  float p = _prevSweepDeg;
  float c = _sweepDeg;
  uint32_t now = millis();

  for (auto& b : _blips) {
    bool crossed;
    if (p <= c) {
      crossed = (b.bearing > p && b.bearing <= c);
    } else {
      // el barrido pasó por 360/0 en este frame
      crossed = (b.bearing > p || b.bearing <= c);
    }
    if (crossed) b.pingMs = now;
  }
}

void RadarScreen::drawFrame() {
  if (_discReady) drawDiscSprite();
  else            drawDiscDirect();
}

// --- Camino normal: componer todo el disco en RAM y volcarlo de una ---------
void RadarScreen::drawDiscSprite() {
  TFT_eSprite& s = _disc;
  uint32_t now = millis();

  // Fondo: el mapa se vuelca de la copia en RAM directo al buffer del sprite.
  // Los dos usan el mismo empaquetado de 4 bpp ((x + y*w)>>1, nibble alto para
  // x par), asi que es un memcpy y no una conversion pixel por pixel.
  if (_mapReady) {
    memcpy(s.getPointer(), _mapRam, RADAR_MAP_BYTES);
  } else {
    s.fillSprite(C_BG);
  }

  // Anillos de alcance
  for (int i = 1; i <= 4; i++) {
    s.drawCircle(CENTER, CENTER, RING_MAX * i / 4, C_RING);
  }

  // Cruz de referencia
  s.drawFastVLine(CENTER, CENTER - RING_MAX, RING_MAX * 2, C_RING);
  s.drawFastHLine(CENTER - RING_MAX, CENTER, RING_MAX * 2, C_RING);

  // Estela del barrido: cuñas de brillo creciente hacia el borde de ataque.
  // Cada cuña es un triángulo centro-borde; con 10 grados de ancho el error
  // contra el arco real es de ~0.3 px, invisible a este radio.
  // Se dibujan de la más tenue a la más viva a propósito: las cuñas se solapan
  // 0.6 grados para que no queden costuras negras entre bandas, y así el solape
  // lo gana siempre la banda más brillante.
  const float step = (float)RADAR_TRAIL_DEG / RADAR_TRAIL_STEPS;
  for (int i = RADAR_TRAIL_STEPS - 1; i >= 0; i--) {
    float a1 = _sweepDeg - step * i;
    float a0 = a1 - step - 0.6f;
    int x0, y0, x1, y1;
    polar(a0, RING_MAX, x0, y0, CENTER, CENTER);
    polar(a1, RING_MAX, x1, y1, CENTER, CENTER);
    s.fillTriangle(CENTER, CENTER, x0, y0, x1, y1,
                   C_TRAIL_TOP - i);  // i=0 es la banda pegada al barrido
  }

  // Borde de ataque
  {
    int ex, ey;
    polar(_sweepDeg, RING_MAX, ex, ey, CENTER, CENTER);
    s.drawLine(CENTER, CENTER, ex, ey, C_EDGE);
  }

  // Etiquetas de distancia sobre la diagonal NE, para no pisar la cruz.
  // Van después del barrido para que sigan legibles cuando pasa por encima.
  s.setTextDatum(MC_DATUM);
  s.setTextColor(C_LABEL);
  for (int i = 1; i <= 4; i++) {
    int r = RING_MAX * i / 4;
    int lx, ly;
    polar(45.0f, r, lx, ly, CENTER, CENTER);
    char lbl[8];
    snprintf(lbl, sizeof(lbl), "%.0f", RADAR_RANGE_KM * i / 4);
    s.drawString(lbl, lx, ly, 1);
  }
  // "km" una sola vez, pegado al anillo exterior
  {
    int lx, ly;
    polar(45.0f, RING_MAX, lx, ly, CENTER, CENTER);
    s.drawString("km", lx + 14, ly, 1);
  }

  // Puntos cardinales. Van fuera del anillo exterior, sobre el negro que deja
  // el recorte circular del mapa, para que se lean limpios.
  {
    int x, y;
    polar(0,   CARD_R, x, y, CENTER, CENTER); s.drawString("N", x, y, 1);
    polar(90,  CARD_R, x, y, CENTER, CENTER); s.drawString("E", x, y, 1);
    polar(180, CARD_R, x, y, CENTER, CENTER); s.drawString("S", x, y, 1);
    polar(270, CARD_R, x, y, CENTER, CENTER); s.drawString("O", x, y, 1);
  }

  // El Palomar: unico punto de referencia rotulado. Simbolo de aeropuerto
  // (circulo con pista cruzada) y la etiqueta corrida para no pisar la casa,
  // que esta a solo ~3 km y queda muy cerca en pantalla.
  if (_palomarInView) {
    s.drawCircle(_palomarX, _palomarY, 4, C_PING);
    s.drawLine(_palomarX - 3, _palomarY + 3, _palomarX + 3, _palomarY - 3, C_PING);

    s.setTextDatum(ML_DATUM);
    s.setTextColor(C_LABEL);
    s.drawString("El Palomar", _palomarX + 7, _palomarY - 6, 1);
    s.setTextDatum(MC_DATUM);
  }

  // Aviones, encima del barrido para que nunca queden tapados
  for (const auto& b : _blips) {
    uint8_t base = b.near ? C_ALERT : C_BLIP;
    int radius = b.near ? 4 : 3;

    uint32_t age = now - b.pingMs;
    if (b.pingMs != 0 && age < RADAR_PING_MS) {
      // Destello de detección: crece y se apaga hasta volver al estado normal
      float t = 1.0f - (float)age / (float)RADAR_PING_MS;   // 1 -> 0
      radius += (int)lroundf(3.0f * t);
      if (t > 0.55f) base = C_PING;

      // Halo que se expande y desaparece
      int halo = radius + 3 + (int)lroundf(6.0f * (1.0f - t));
      s.drawCircle(b.sx, b.sy, halo, t > 0.3f ? C_TRAIL_TOP : C_TRAIL0 + 2);
    }

    s.fillCircle(b.sx, b.sy, radius, base);
  }

  // Casa en el centro
  s.fillTriangle(CENTER - 4, CENTER + 4, CENTER + 4, CENTER + 4,
                 CENTER, CENTER - 5, C_HOME);

  s.pushSprite(DISC_X, DISC_Y);
}

// --- Fallback sin sprite: redibujar solo lo que cambia ----------------------
// Si no hubo RAM para el buffer, animamos igual con un barrido de una sola
// línea (sin estela): borramos la línea anterior en negro, re-estampamos los
// anillos y la cruz en los puntos que tapaba, y dibujamos la nueva.
void RadarScreen::drawDiscDirect() {
  TFT_eSPI& tft = _display.tft();
  const int cx = DISC_X + CENTER;
  const int cy = DISC_Y + CENTER;

  auto ray = [&](float deg, uint16_t color) {
    int x, y;
    polar(deg, RING_MAX, x, y, cx, cy);
    tft.drawLine(cx, cy, x, y, color);
  };

  // Marco fijo la primera vez (o después de volver de otra pantalla)
  if (!_chromeValid) return; // render() todavía no corrió

  ray(_prevSweepDeg, TFT_BLACK);

  // Re-estampar los cruces con los anillos y con la cruz que acabamos de borrar
  for (int i = 1; i <= 4; i++) {
    int x, y;
    polar(_prevSweepDeg, RING_MAX * i / 4, x, y, cx, cy);
    tft.drawPixel(x, y, TFT_DARKGREEN);
  }
  {
    int x, y;
    polar(_prevSweepDeg, RING_MAX, x, y, cx, cy);
    // el eje vertical/horizontal solo se toca cerca de los múltiplos de 90
    float m = fmodf(_prevSweepDeg, 90.0f);
    if (m < 3.0f || m > 87.0f) {
      tft.drawLine(cx, cy, x, y, TFT_DARKGREY);
    }
  }

  // Los aviones que la línea pudo haber tapado
  for (const auto& b : _blips) {
    float d = fabsf(b.bearing - _prevSweepDeg);
    if (d > 180.0f) d = 360.0f - d;
    if (d > 6.0f) continue;
    tft.fillCircle(DISC_X + b.sx, DISC_Y + b.sy, b.near ? 4 : 3,
                   b.near ? TFT_RED : TFT_GREEN);
  }

  ray(_sweepDeg, TFT_GREEN);
}
