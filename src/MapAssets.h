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

// Centro con el que se genero este mapa, leido de include/config.h. Tiene que
// seguir coincidiendo con HOME_LAT/HOME_LON: RadarScreen.cpp lo verifica con un
// static_assert. Sin esto, mover la casa en config.h y no volver a correr el
// generador compilaba igual y dejaba los aviones sobre calles que no son.
static constexpr double MAP_ORIGIN_LAT = -34.5858006;
static constexpr double MAP_ORIGIN_LON = -58.5917033;

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

// ---------------------------------------------------------------------------
//  Mapa de fondo del radar
// ---------------------------------------------------------------------------
// Va aparte de MAP_ASSETS porque tiene otro formato: 4 bpp indexado y ya
// empaquetado como lo espera TFT_eSprite, para poder meterlo al sprite con un
// memcpy. Ademas viene sin etiquetas de ciudades y recortado en circulo.

static const int RADAR_DISC_SIZE = 200;
static const int RADAR_RING_MAX  = 88;

// Alcance real del anillo exterior. Tiene que coincidir con RADAR_RANGE_KM de
// config.h: RadarScreen.cpp lo verifica con un static_assert.
static const int RADAR_MAP_RANGE_KM = 20;

// (200 * 200) / 2 = 20.000 bytes
static const uint32_t RADAR_MAP_BYTES =
    (uint32_t)RADAR_DISC_SIZE * RADAR_DISC_SIZE / 2;

static const MapAsset RADAR_MAP =
    { "/radar.bin", 10, 88226.3237, 157761.5569, 0.55401662f, 20 };

// Los 5 grises del mapa, en RGB565. Salen de los cuantiles del
// histograma real de la imagen, no de una escala fija: el basemap oscuro usa un
// rango angosto y repartir niveles parejos entre negro y blanco desperdiciaria
// casi todos. Ocupan los indices 1..5 de la paleta del disco.
static const int RADAR_MAP_GREY_COUNT = 5;
static const uint16_t RADAR_MAP_GREYS[RADAR_MAP_GREY_COUNT] = {
  0x10A2, // luminancia 20
  0x39E7, // luminancia 61
  0x4A69, // luminancia 79
  0x5ACB, // luminancia 90
  0x6B4D, // luminancia 105
};
