// ---------------------------------------------------------------------------
//  Generador del mapa raster pre-renderizado de desk-radar
//
//  Baja tiles de basemap oscuro, los pega, recorta el area exacta alrededor de
//  tu casa, la reduce al tamano del viewport del ESP32 y la escribe como RGB565
//  crudo. El ESP32 despues solo hace pushImage: ni decodifica PNG ni baja nada.
//
//  Uso:
//      cd tools
//      npm install
//      node build-map.mjs
//      cd .. && pio run -t uploadfs
//
//  Salidas:
//      ../data/map80.bin      imagen cruda RGB565 del modo 80 km
//      ../data/map40.bin      idem 40 km
//      ../src/MapAssets.h     constantes de proyeccion (GENERADO, no editar)
//      preview80.png          para mirar el resultado antes de flashear
//      preview40.png
//
//  Tiles (c) Esri - World Dark Gray Canvas (Esri, HERE, Garmin,
//  (c) OpenStreetMap contributors y la comunidad GIS).
// ---------------------------------------------------------------------------

import { mkdir, writeFile } from 'node:fs/promises';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import sharp from 'sharp';

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, '..');

// --- Parametros (tienen que coincidir con include/config.h) -----------------
const HOME_LAT = -34.5858006;
const HOME_LON = -58.5917033;

// Viewport del mapa en la pantalla del ESP32 (240x320 menos barra de estado,
// leyenda de altitud y boton de zoom). Si cambias esto, cambia MAP_VIEW_* en
// MapScreen.cpp tambien.
const VIEW_W = 240;
const VIEW_H = 262;

// Un modo por cada zoom tactil. widthKm es el ancho real que cubre la pantalla.
const MODES = [
  { name: '80', widthKm: 80, zoom: 9 },
  { name: '40', widthKm: 40, zoom: 10 },
];

// Basemap oscuro de Esri. Se usa porque no pide API key: CARTO y Stadia hoy
// devuelven HTTP 200 con un tile marcado "API KEY REQUIRED" en vez de un error,
// asi que chequear el status code no alcanza para darse cuenta.
//
// OJO con el orden de la URL: Esri usa {z}/{fila}/{columna}, o sea Y ANTES QUE X,
// al reves de la convencion de OSM. Es la fuente de error tipica con este server.
//
// Esri sirve el fondo y las etiquetas como capas separadas: hay que componer
// Base (JPEG, opaco) + Reference (PNG con alpha, solo texto y limites).
const TILE_BASE_URL = (z, x, y) =>
  `https://server.arcgisonline.com/ArcGIS/rest/services/Canvas/World_Dark_Gray_Base/MapServer/tile/${z}/${y}/${x}`;
const TILE_REF_URL = (z, x, y) =>
  `https://server.arcgisonline.com/ArcGIS/rest/services/Canvas/World_Dark_Gray_Reference/MapServer/tile/${z}/${y}/${x}`;

const TILE_SIZE = 256;
const USER_AGENT = 'desk-radar/1.0 (personal ESP32 flight radar; https://github.com/jorgefinelli90/desk-radar)';

// --- Web Mercator ----------------------------------------------------------
const EARTH_CIRCUMFERENCE = 2 * Math.PI * 6378137;

// Metros por pixel a un zoom y latitud dados
function resolution(zoom, lat) {
  return (EARTH_CIRCUMFERENCE / TILE_SIZE) / 2 ** zoom * Math.cos((lat * Math.PI) / 180);
}

// Coordenadas de pixel global (el mundo entero es 256 * 2^zoom px de lado)
function lonToWorldPx(lon, zoom) {
  return ((lon + 180) / 360) * TILE_SIZE * 2 ** zoom;
}

function latToWorldPy(lat, zoom) {
  const s = Math.sin((lat * Math.PI) / 180);
  return (0.5 - Math.log((1 + s) / (1 - s)) / (4 * Math.PI)) * TILE_SIZE * 2 ** zoom;
}

// --- Descarga de tiles -----------------------------------------------------
async function fetchUrl(url, what) {
  const res = await fetch(url, { headers: { 'User-Agent': USER_AGENT } });
  if (!res.ok) {
    throw new Error(`${what} -> HTTP ${res.status}`);
  }
  const buf = Buffer.from(await res.arrayBuffer());
  if (buf.length < 200) {
    throw new Error(`${what} -> respuesta sospechosamente chica (${buf.length} bytes)`);
  }
  return buf;
}

const fetchBaseTile = (z, x, y) => fetchUrl(TILE_BASE_URL(z, x, y), `base ${z}/${x}/${y}`);
const fetchRefTile = (z, x, y) => fetchUrl(TILE_REF_URL(z, x, y), `ref ${z}/${x}/${y}`);

// --- Conversion a RGB565 ---------------------------------------------------
// OJO: escribimos BIG-ENDIAN a proposito. El ILI9341 espera el byte alto
// primero, asi que guardando ya swapeado el ESP32 puede hacer
// setSwapBytes(false) y empujar el buffer tal cual, sin tocar un solo byte.
// Si algun dia los colores salen psicodelicos, el sospechoso es esta funcion.
function toRgb565BE(rgb, width, height) {
  const out = Buffer.alloc(width * height * 2);
  for (let i = 0, o = 0; i < width * height; i++, o += 2) {
    const r = rgb[i * 3];
    const g = rgb[i * 3 + 1];
    const b = rgb[i * 3 + 2];
    const v = ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3);
    out[o] = (v >> 8) & 0xff; // byte alto primero
    out[o + 1] = v & 0xff;
  }
  return out;
}

// --- Construccion de un modo ----------------------------------------------
async function buildMode(mode) {
  const { name, widthKm, zoom } = mode;

  const mPerPxNative = resolution(zoom, HOME_LAT);
  const mPerPxTarget = (widthKm * 1000) / VIEW_W;

  // Region fuente: la que, reducida a VIEW_W, da exactamente widthKm de ancho
  const srcW = Math.round((widthKm * 1000) / mPerPxNative);
  const srcH = Math.round((srcW * VIEW_H) / VIEW_W);
  const scale = VIEW_W / srcW;

  const centerPx = lonToWorldPx(HOME_LON, zoom);
  const centerPy = latToWorldPy(HOME_LAT, zoom);

  // Esquina superior izquierda de la region fuente, en pixeles globales
  const originPx = centerPx - srcW / 2;
  const originPy = centerPy - srcH / 2;

  const tx0 = Math.floor(originPx / TILE_SIZE);
  const tx1 = Math.floor((originPx + srcW) / TILE_SIZE);
  const ty0 = Math.floor(originPy / TILE_SIZE);
  const ty1 = Math.floor((originPy + srcH) / TILE_SIZE);
  const nTiles = (tx1 - tx0 + 1) * (ty1 - ty0 + 1);

  console.log(`\n--- modo ${name} km (zoom OSM ${zoom}) ---`);
  console.log(`  nativo ${mPerPxNative.toFixed(1)} m/px  ->  objetivo ${mPerPxTarget.toFixed(1)} m/px`);
  console.log(`  fuente ${srcW}x${srcH} px  ->  ${VIEW_W}x${VIEW_H} (factor ${scale.toFixed(3)})`);
  console.log(`  cobertura ${widthKm} x ${((VIEW_H * mPerPxTarget) / 1000).toFixed(1)} km`);
  console.log(`  bajando ${nTiles} tiles (fondo + etiquetas = ${nTiles * 2} requests)...`);

  // Lienzo donde pegamos los tiles crudos
  const mosaicW = (tx1 - tx0 + 1) * TILE_SIZE;
  const mosaicH = (ty1 - ty0 + 1) * TILE_SIZE;

  // sharp aplica los composites en orden, asi que primero todos los fondos y
  // despues todas las etiquetas: si se intercalaran, un fondo opaco taparia la
  // etiqueta del tile de al lado.
  const bases = [];
  const refs = [];
  for (let tx = tx0; tx <= tx1; tx++) {
    for (let ty = ty0; ty <= ty1; ty++) {
      const left = (tx - tx0) * TILE_SIZE;
      const top = (ty - ty0) * TILE_SIZE;
      bases.push({ input: await fetchBaseTile(zoom, tx, ty), left, top });
      refs.push({ input: await fetchRefTile(zoom, tx, ty), left, top });
    }
  }
  const composites = [...bases, ...refs];

  const mosaic = sharp({
    create: {
      width: mosaicW,
      height: mosaicH,
      channels: 3,
      background: { r: 0, g: 0, b: 0 },
    },
  }).composite(composites);

  // Recorte exacto dentro del mosaico + reduccion con Lanczos
  const cropLeft = Math.round(originPx - tx0 * TILE_SIZE);
  const cropTop = Math.round(originPy - ty0 * TILE_SIZE);

  const resized = sharp(await mosaic.png().toBuffer())
    .extract({ left: cropLeft, top: cropTop, width: srcW, height: srcH })
    .resize(VIEW_W, VIEW_H, { kernel: 'lanczos3' });

  // Preview para mirar en la PC antes de flashear
  await resized.clone().png().toFile(join(HERE, `preview${name}.png`));

  const { data, info } = await resized.removeAlpha().raw().toBuffer({ resolveWithObject: true });
  if (info.width !== VIEW_W || info.height !== VIEW_H || info.channels !== 3) {
    throw new Error(`salida inesperada: ${info.width}x${info.height}x${info.channels}`);
  }

  const bin = toRgb565BE(data, VIEW_W, VIEW_H);
  const outPath = join(ROOT, 'data', `map${name}.bin`);
  await writeFile(outPath, bin);
  console.log(`  escrito data/map${name}.bin (${bin.length.toLocaleString()} bytes)`);
  console.log(`  preview tools/preview${name}.png`);

  return { name, widthKm, zoom, srcW, srcH, scale, originPx, originPy, mPerPxTarget };
}

// --- Header generado para el firmware --------------------------------------
function renderHeader(results) {
  const entries = results
    .map(
      (r) => `  // Modo ${r.name} km: zoom OSM ${r.zoom}, fuente ${r.srcW}x${r.srcH} reducida x${r.scale.toFixed(4)}
  { "/map${r.name}.bin", ${r.zoom}, ${r.originPx.toFixed(4)}, ${r.originPy.toFixed(4)}, ${r.scale.toFixed(8)}f, ${r.widthKm} },`
    )
    .join('\n');

  return `#pragma once
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
static const int MAP_VIEW_W = ${VIEW_W};
static const int MAP_VIEW_H = ${VIEW_H};

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
${entries}
};
static const int MAP_ASSET_COUNT = sizeof(MAP_ASSETS) / sizeof(MAP_ASSETS[0]);
`;
}

// --- Main ------------------------------------------------------------------
async function main() {
  console.log('Generando el mapa de desk-radar');
  console.log(`  centro: ${HOME_LAT}, ${HOME_LON}`);
  console.log(`  viewport: ${VIEW_W}x${VIEW_H} px`);

  await mkdir(join(ROOT, 'data'), { recursive: true });

  const results = [];
  for (const mode of MODES) {
    results.push(await buildMode(mode));
  }

  const headerPath = join(ROOT, 'src', 'MapAssets.h');
  await writeFile(headerPath, renderHeader(results));
  console.log(`\nescrito src/MapAssets.h`);

  const total = results.length * VIEW_W * VIEW_H * 2;
  console.log(`\nTotal en flash: ${total.toLocaleString()} bytes`);
  console.log('\nAhora:  pio run -t uploadfs   (sube los mapas)');
  console.log('        pio run -t upload     (sube el firmware)');
}

main().catch((err) => {
  console.error('\nFallo la generacion del mapa:');
  console.error(err.message);
  process.exit(1);
});
