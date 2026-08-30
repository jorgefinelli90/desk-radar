#pragma once
#include <stdint.h>

// ===========================================================================
//  ARCHIVO GENERADO POR tools/build-map.mjs - NO EDITAR A MANO
//
//  Cualquier cambio se pierde la proxima vez que corras el generador. Si
//  necesitas mover el area o cambiar los zooms, edita HOME_LAT/HOME_LON o
//  MODES en tools/build-map.mjs y volve a correrlo.
//
//  Que los .bin y estas constantes salgan del mismo script es lo que garantiza
//  que la proyeccion del firmware y la imagen no se puedan desincronizar.
//
//  Mapa (c) Esri, HERE, Garmin, (c) OpenStreetMap contributors.
// ===========================================================================

// Tamano del viewport del mapa en pantalla, en pixeles.
static const int MAP_VIEW_W = 240;
static const int MAP_VIEW_H = 262;

// Bytes de cada .bin (RGB565, 2 bytes por pixel, ya en big-endian).
static const uint32_t MAP_BIN_BYTES = (uint32_t)MAP_VIEW_W * MAP_VIEW_H * 2;

struct MapAsset {
  const char* path;    // ruta dentro de LittleFS
  int         zoom;    // nivel de zoom Web Mercator de los tiles de origen
  double      originPx; // pixel global X de la esquina sup. izq. del recorte
  double      originPy; // pixel global Y de la esquina sup. izq. del recorte
  float       scale;   // factor de reduccion aplicado (pantalla / fuente)
  int         widthKm; // ancho real que cubre la pantalla, en km
};

static const MapAsset MAP_ASSETS[] = {
  // Modo 80 km: zoom OSM 9, fuente 318x347 reducida x0.7547
  { "/map80.bin", 9, 44044.4118, 78797.5284, 0.75471698f, 80 },
  // Modo 40 km: zoom OSM 10, fuente 318x347 reducida x0.7547
  { "/map40.bin", 10, 88247.8237, 157768.5569, 0.75471698f, 40 },
};
static const int MAP_ASSET_COUNT = sizeof(MAP_ASSETS) / sizeof(MAP_ASSETS[0]);
