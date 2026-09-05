#pragma once
#include <Arduino.h>
#include <math.h>
#include "MapAssets.h"

// Proyección Web Mercator, la misma que usan los tiles del mapa.
//
// OJO: no es equirectangular. La latitud NO se puede interpolar linealmente
// contra el alto de la imagen: hay que pasar por la fórmula de Mercator o los
// aviones quedan corridos en vertical (y el error crece hacia los bordes).
//
// Los valores de origen y escala salen de MapAssets.h, que lo genera el mismo
// script que arma los .bin, así que la proyección de acá y la imagen no se
// pueden desincronizar.
namespace GeoMap {

static const int TILE_SIZE = 256;

// Pixel global X (el mundo entero mide 256 * 2^zoom px de lado)
inline double lonToWorldPx(double lon, int zoom) {
  return ((lon + 180.0) / 360.0) * TILE_SIZE * pow(2.0, zoom);
}

// Pixel global Y
inline double latToWorldPy(double lat, int zoom) {
  double s = sin(lat * M_PI / 180.0);
  // Clamp: cerca de los polos el log diverge. A nuestras latitudes no pasa
  // nunca, pero un dato corrupto de OpenSky no debería producir un NaN.
  if (s > 0.9999) s = 0.9999;
  if (s < -0.9999) s = -0.9999;
  return (0.5 - log((1.0 + s) / (1.0 - s)) / (4.0 * M_PI)) * TILE_SIZE * pow(2.0, zoom);
}

// Coordenadas dentro del viewport del mapa (0,0 = esquina sup. izq. de la
// imagen, no de la pantalla: el offset vertical lo suma MapScreen).
inline float screenX(double lon, const MapAsset& a) {
  return (float)((lonToWorldPx(lon, a.zoom) - a.originPx) * a.scale);
}

inline float screenY(double lat, const MapAsset& a) {
  return (float)((latToWorldPy(lat, a.zoom) - a.originPy) * a.scale);
}

// true si el punto cae dentro de la imagen del mapa
inline bool inView(float x, float y) {
  return x >= 0 && x < MAP_VIEW_W && y >= 0 && y < MAP_VIEW_H;
}

} // namespace GeoMap
