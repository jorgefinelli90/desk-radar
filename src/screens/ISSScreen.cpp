#include "screens/ISSScreen.h"
#include "config.h"
#include "utils/GeoUtils.h"
#include "models/WorldMapAsset.h"
#include <math.h>

// Mismo mapa que usa el bitmap: proyeccion equirectangular, x = longitud
// lineal, y = latitud lineal. No es la proyeccion de GeoMap.h (esa es Web
// Mercator, para el mapa local de alta resolucion): a esta escala, del
// planeta entero, Mercator estira los polos hasta el infinito y no vale la
// pena; equirectangular alcanza para "en que continente/oceano esta".
static int worldX(double lon, int mapX, int mapW) {
  return mapX + (int)((lon + 180.0) / 360.0 * mapW);
}
static int worldY(double lat, int mapY, int mapH) {
  return mapY + (int)((90.0 - lat) / 180.0 * mapH);
}

static bool isLand(int px, int py) {
  if (px < 0 || px >= WORLD_MAP_W || py < 0 || py >= WORLD_MAP_H) return false;
  int byteIdx = py * WORLD_MAP_ROW_BYTES + (px >> 3);
  int bitIdx = 7 - (px & 7);
  return (WORLD_MAP_BITS[byteIdx] >> bitIdx) & 1;
}

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

  // --- Mapamundi con la ISS y casa marcadas -------------------------------
  // Escalado entero (no 1:1) para que el bitmap de 220x110 llene el ancho de
  // la pantalla: 240/220 no es entero, asi que se dibuja a razon 1 pixel de
  // mapa -> 1 pixel de pantalla y se centra, en vez de escalar con huecos.
  const int mapX = (tft.width() - WORLD_MAP_W) / 2;
  const int mapY = 34;

  tft.fillRect(mapX, mapY, WORLD_MAP_W, WORLD_MAP_H, TFT_NAVY); // oceano
  for (int y = 0; y < WORLD_MAP_H; y++) {
    int runStart = -1;
    for (int x = 0; x <= WORLD_MAP_W; x++) {
      bool land = (x < WORLD_MAP_W) && isLand(x, y);
      if (land && runStart < 0) {
        runStart = x;
      } else if (!land && runStart >= 0) {
        // Una linea horizontal por corrida de tierra: mucho mas rapido que un
        // drawPixel por pixel (220x110 = 24200 posibles) sobre SPI.
        tft.drawFastHLine(mapX + runStart, mapY + y, x - runStart, TFT_DARKGREEN);
        runStart = -1;
      }
    }
  }
  tft.drawRect(mapX, mapY, WORLD_MAP_W, WORLD_MAP_H, TFT_DARKGREY);

  // Traza de la orbita: por donde vino (celeste) y hacia donde va (naranja).
  // Una polilinea por tramo, cortada cuando el salto de longitud entre dos
  // puntos consecutivos pasa los 180 grados: eso es un cruce del
  // antimeridiano (+180/-180), no un desplazamiento real, y conectarlo
  // dibujaria una linea atravesando toda la pantalla de punta a punta.
  //
  // drawWideLine (no drawLine) porque un trazo de 1px de un color parecido al
  // oceano quedaba invisible en la práctica: se probó en la placa real con
  // TFT_DARKCYAN y a simple vista, sobre TFT_NAVY, no se distinguia. Con
  // TFT_CYAN (bien mas claro que el oceano) y 1.6px de ancho se ve incluso
  // sobre tierra.
  static const float TRACK_WIDTH = 1.6f;
  auto drawTrack = [&](const ISSTrackPoint* pts, int count, uint16_t color) {
    for (int i = 0; i + 1 < count; i++) {
      if (fabs(pts[i + 1].lon - pts[i].lon) > 180.0) continue;
      tft.drawWideLine(worldX(pts[i].lon, mapX, WORLD_MAP_W), worldY(pts[i].lat, mapY, WORLD_MAP_H),
                        worldX(pts[i + 1].lon, mapX, WORLD_MAP_W), worldY(pts[i + 1].lat, mapY, WORLD_MAP_H),
                        TRACK_WIDTH, color);
    }
  };
  drawTrack(iss.trackPast(), iss.trackPastCount(), TFT_CYAN);
  drawTrack(iss.trackFuture(), iss.trackFutureCount(), TFT_ORANGE);

  // ISS: un punto grande, coloreado segun si esta a la luz del sol o en la
  // sombra de la Tierra. El punto puede salirse un pixel del recuadro sobre
  // los bordes (px=0 o px=219): fillCircle lo recorta solo, no rompe nada.
  int sx = worldX(p.lon, mapX, WORLD_MAP_W);
  int sy = worldY(p.lat, mapY, WORLD_MAP_H);

  // Conecta la traza con la posicion en vivo (llegan de dos fetches
  // distintos, no calzan al pixel exacto, pero a esta escala no se nota).
  if (iss.trackPastCount() > 0) {
    const ISSTrackPoint& last = iss.trackPast()[iss.trackPastCount() - 1];
    if (fabs(p.lon - last.lon) <= 180.0) {
      tft.drawWideLine(worldX(last.lon, mapX, WORLD_MAP_W), worldY(last.lat, mapY, WORLD_MAP_H), sx, sy,
                        TRACK_WIDTH, TFT_CYAN);
    }
  }
  if (iss.trackFutureCount() > 0) {
    const ISSTrackPoint& first = iss.trackFuture()[0];
    if (fabs(first.lon - p.lon) <= 180.0) {
      tft.drawWideLine(sx, sy, worldX(first.lon, mapX, WORLD_MAP_W), worldY(first.lat, mapY, WORLD_MAP_H),
                        TRACK_WIDTH, TFT_ORANGE);
    }
  }

  // Casa: una cruz chica, siempre en el mismo lugar del mapa.
  int hx = worldX(HOME_LON, mapX, WORLD_MAP_W);
  int hy = worldY(HOME_LAT, mapY, WORLD_MAP_H);
  tft.drawLine(hx - 3, hy, hx + 3, hy, TFT_WHITE);
  tft.drawLine(hx, hy - 3, hx, hy + 3, TFT_WHITE);

  uint16_t issColor = (p.visibility == ISSVisibility::Eclipsed) ? TFT_SKYBLUE : TFT_YELLOW;
  tft.drawCircle(sx, sy, 5, TFT_WHITE);
  tft.fillCircle(sx, sy, 3, issColor);

  int y = mapY + WORLD_MAP_H + 6;

  // Referencia de colores de la traza, debajo del mapa
  tft.fillRect(mapX + 4, y, 8, 8, TFT_CYAN);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_SILVER, TFT_BLACK);
  tft.drawString("Recorrida", mapX + 16, y + 4, 1);

  int futureX = mapX + WORLD_MAP_W / 2 + 6;
  tft.fillRect(futureX, y, 8, 8, TFT_ORANGE);
  tft.drawString("Por venir", futureX + 12, y + 4, 1);
  y += 16;

  // Estado de visibilidad, debajo de la referencia
  tft.setTextDatum(MC_DATUM);
  const char* visTxt = (p.visibility == ISSVisibility::Daylight) ? "A la luz del sol"
                      : (p.visibility == ISSVisibility::Eclipsed) ? "En la sombra de la Tierra"
                      : "Visibilidad desconocida";
  tft.setTextColor(issColor, TFT_BLACK);
  tft.drawString(visTxt, tft.width() / 2, y, 1);
  y += 14;

  tft.drawFastHLine(10, y, tft.width() - 20, TFT_DARKGREEN);
  y += 12;

  // Filas de datos, mismo estilo que DetailScreen
  const int rowH = 26;
  char buf[40];

  auto row = [&](const char* label, const String& value, uint16_t valueColor) {
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_SILVER, TFT_BLACK);
    tft.drawString(label, 14, y + 4, 2);

    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(valueColor, TFT_BLACK);
    tft.drawString(value, tft.width() - 14, y + 2, 2);

    y += rowH;
  };

  snprintf(buf, sizeof(buf), "%.0f km  %s (%.0f)", p.distanceKm,
           GeoUtils::cardinal(p.bearingDeg), p.bearingDeg);
  row("Distancia", buf, TFT_WHITE);

  snprintf(buf, sizeof(buf), "%.0f km", p.altitudeKm);
  row("Altura orbital", buf, TFT_WHITE);

  snprintf(buf, sizeof(buf), "%.0f km/h", p.velocityKmh);
  row("Velocidad", buf, TFT_WHITE);

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Datos del ultimo refresco", tft.width() / 2, tft.height() - 6, 1);
}
