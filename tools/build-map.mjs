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

import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import sharp from 'sharp';

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, '..');

// --- Parametros (tienen que coincidir con include/config.h) -----------------
// El centro NO se escribe aca: se lee de include/config.h, que es la fuente de
// verdad del firmware. Tenerlo duplicado era el modo de falla mas silencioso que
// quedaba en el pipeline: cambiabas la casa en config.h, no regenerabas el mapa,
// y el firmware seguia proyectando sobre el recorte viejo sin que nada se
// quejara. Ahora ademas el header generado lleva el centro y RadarScreen.cpp lo
// verifica con un static_assert, asi que las dos mitades no se pueden separar.
let HOME_LAT;
let HOME_LON;

async function readHomeFromConfig() {
  const path = join(ROOT, 'include', 'config.h');
  const src = await readFile(path, 'utf8');
  const grab = (name) => {
    // String.raw y no un template comun: en un template literal `\s` se come la
    // barra y el regex queda buscando la letra "s".
    const re = new RegExp(
      String.raw`static\s+(?:constexpr|const)\s+double\s+` +
      name + String.raw`\s*=\s*(-?[0-9.]+)`);
    const m = src.match(re);
    if (!m) throw new Error(`no encontre ${name} en include/config.h`);
    return parseFloat(m[1]);
  };
  return { lat: grab('HOME_LAT'), lon: grab('HOME_LON') };
}

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

// --- Mapa de fondo del radar -----------------------------------------------
// El disco del radar es un sprite de 4 bpp (16 colores) porque uno de 16 bpp
// costaria 80 KB y no convive con el handshake TLS. Asi que el mapa de fondo va
// cuantizado a 5 grises y se guarda YA EMPAQUETADO en 4 bpp con el mismo layout
// que usa TFT_eSprite: (x + y*w)>>1, nibble alto para x par. Asi el firmware lo
// mete al sprite con un memcpy en vez de convertir pixel por pixel.
//
// Los 11 indices que sobran son para el radar (anillos, estela, blips, casa).
//
// Estos tres valores TIENEN que coincidir con RadarScreen.h y config.h; el
// header generado incluye un static_assert que lo verifica al compilar.
const RADAR_DISC_SIZE = 200;  // lado del sprite del disco
const RADAR_RING_MAX  = 88;   // radio en px del anillo exterior
const RADAR_RANGE_KM  = 20;   // alcance real del anillo exterior
const RADAR_ZOOM      = 10;   // zoom de los tiles de origen
const RADAR_MASK_R    = 90;   // fuera de este radio el mapa queda negro
const RADAR_GREYS     = 5;    // niveles de gris del mapa
// Rango de luminancia al que se estiran esos niveles para mostrar. El mapa tiene
// que leerse en una TFT chica sin robarle protagonismo al radar: subir el max
// hace el mapa mas contrastado, bajarlo lo deja mas discreto.
const RADAR_STRETCH_MIN = 20;
const RADAR_STRETCH_MAX = 105;

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

// --- Mapa del radar: 4 bpp indexado, en escala de grises y recortado en circulo
async function buildRadarMap() {
  const size = RADAR_DISC_SIZE;
  const mPerPxNative = resolution(RADAR_ZOOM, HOME_LAT);
  const mPerPxTarget = (RADAR_RANGE_KM * 1000) / RADAR_RING_MAX;

  const srcSize = Math.round((size * mPerPxTarget) / mPerPxNative);
  const scale = size / srcSize;

  const centerPx = lonToWorldPx(HOME_LON, RADAR_ZOOM);
  const centerPy = latToWorldPy(HOME_LAT, RADAR_ZOOM);
  const originPx = centerPx - srcSize / 2;
  const originPy = centerPy - srcSize / 2;

  const tx0 = Math.floor(originPx / TILE_SIZE);
  const tx1 = Math.floor((originPx + srcSize) / TILE_SIZE);
  const ty0 = Math.floor(originPy / TILE_SIZE);
  const ty1 = Math.floor((originPy + srcSize) / TILE_SIZE);
  const nTiles = (tx1 - tx0 + 1) * (ty1 - ty0 + 1);

  console.log(`\n--- mapa del radar (zoom OSM ${RADAR_ZOOM}) ---`);
  console.log(`  alcance ${RADAR_RANGE_KM} km en ${RADAR_RING_MAX} px  ->  ${mPerPxTarget.toFixed(1)} m/px`);
  console.log(`  fuente ${srcSize}x${srcSize} px  ->  ${size}x${size} (factor ${scale.toFixed(3)})`);
  console.log(`  bajando ${nTiles} tiles (solo fondo, sin etiquetas)...`);

  // Solo la capa Base: el radar no lleva nombres de ciudades, unicamente el
  // marcador de El Palomar que dibuja el firmware.
  const mosaicSide = (tx1 - tx0 + 1) * TILE_SIZE;
  const bases = [];
  for (let tx = tx0; tx <= tx1; tx++) {
    for (let ty = ty0; ty <= ty1; ty++) {
      bases.push({
        input: await fetchBaseTile(RADAR_ZOOM, tx, ty),
        left: (tx - tx0) * TILE_SIZE,
        top: (ty - ty0) * TILE_SIZE,
      });
    }
  }

  const mosaic = sharp({
    create: {
      width: mosaicSide,
      height: (ty1 - ty0 + 1) * TILE_SIZE,
      channels: 3,
      background: { r: 0, g: 0, b: 0 },
    },
  }).composite(bases);

  const cropLeft = Math.round(originPx - tx0 * TILE_SIZE);
  const cropTop = Math.round(originPy - ty0 * TILE_SIZE);

  const shrunk = sharp(await mosaic.png().toBuffer())
    .extract({ left: cropLeft, top: cropTop, width: srcSize, height: srcSize })
    .resize(size, size, { kernel: 'lanczos3' })
    .greyscale();

  await shrunk.clone().png().toFile(join(HERE, 'previewradar.png'));

  const { data } = await shrunk.removeAlpha().raw().toBuffer({ resolveWithObject: true });
  // greyscale() deja 1 canal
  const lum = data;

  // --- Cuantizacion a RADAR_GREYS niveles ---
  // Se usa k-means (Lloyd) sobre el histograma, NO cuantiles por poblacion.
  //
  // El basemap oscuro tiene tres picos enormes (agua ~35, tierra ~71 y ~78) y
  // las rutas viven dispersas entre 82 y 98 con muy pocos pixeles. Repartir los
  // niveles por poblacion mete 4 de los 5 dentro del rango 75-80 y se come las
  // rutas, que son justamente lo que hace reconocible el mapa. k-means busca
  // los clusters reales, asi que separa agua, tierra y rutas.
  const c = size / 2;
  const hist = new Array(256).fill(0);
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const dx = x - c + 0.5, dy = y - c + 0.5;
      if (dx * dx + dy * dy <= RADAR_MASK_R * RADAR_MASK_R) hist[lum[y * size + x]]++;
    }
  }

  let lo = hist.findIndex((n) => n > 0);
  let hi = 255; while (hi > lo && hist[hi] === 0) hi--;

  // Centros iniciales repartidos parejo sobre el rango de valores
  let centers = [];
  for (let i = 0; i < RADAR_GREYS; i++) {
    centers.push(lo + ((hi - lo) * i) / (RADAR_GREYS - 1));
  }

  for (let iter = 0; iter < 30; iter++) {
    const sum = new Array(RADAR_GREYS).fill(0);
    const cnt = new Array(RADAR_GREYS).fill(0);
    for (let v = lo; v <= hi; v++) {
      if (!hist[v]) continue;
      let best = 0, bestD = Infinity;
      for (let k = 0; k < RADAR_GREYS; k++) {
        const d = Math.abs(v - centers[k]);
        if (d < bestD) { bestD = d; best = k; }
      }
      sum[best] += v * hist[v];
      cnt[best] += hist[v];
    }
    let moved = 0;
    for (let k = 0; k < RADAR_GREYS; k++) {
      if (!cnt[k]) continue;
      const nc = sum[k] / cnt[k];
      moved += Math.abs(nc - centers[k]);
      centers[k] = nc;
    }
    if (moved < 0.01) break;
  }
  centers.sort((a, b) => a - b);

  const levels = centers.map((v) => Math.round(v));
  console.log(`  grises (k-means sobre el histograma): ${levels.join(', ')}`);

  // El rango real de la imagen es angosto y oscuro (aca ~35..90 de 255), asi que
  // en una TFT chica los niveles quedan casi indistinguibles. Se estiran para
  // mostrar, manteniendo el orden y sin pasarse de RADAR_STRETCH_MAX: el mapa
  // tiene que leerse, pero sin competir con los blips ni con la estela.
  const srcLo = levels[0], srcHi = levels[levels.length - 1];
  const display = levels.map((v) =>
    srcHi === srcLo
      ? RADAR_STRETCH_MIN
      : Math.round(
          RADAR_STRETCH_MIN +
            ((v - srcLo) / (srcHi - srcLo)) * (RADAR_STRETCH_MAX - RADAR_STRETCH_MIN)
        )
  );
  console.log(`  grises estirados para la pantalla:   ${display.join(', ')}`);

  // --- Empaquetado 4 bpp, igual layout que TFT_eSprite ---
  // indice 0 = negro (fuera del circulo); 1..RADAR_GREYS = niveles del mapa
  const stride = size / 2;
  const bin = Buffer.alloc(stride * size);

  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const dx = x - c + 0.5, dy = y - c + 0.5;
      let idx = 0;
      if (dx * dx + dy * dy <= RADAR_MASK_R * RADAR_MASK_R) {
        const v = lum[y * size + x];
        let best = 0, bestD = Infinity;
        for (let l = 0; l < levels.length; l++) {
          const d = Math.abs(v - levels[l]);
          if (d < bestD) { bestD = d; best = l; }
        }
        idx = best + 1; // 0 queda reservado para el negro de afuera
      }

      const o = (x + y * size) >> 1;
      if ((x & 1) === 0) bin[o] = (bin[o] & 0x0f) | (idx << 4);
      else               bin[o] = (bin[o] & 0xf0) | idx;
    }
  }

  const outPath = join(ROOT, 'data', 'radar.bin');
  await writeFile(outPath, bin);
  console.log(`  escrito data/radar.bin (${bin.length.toLocaleString()} bytes, 4 bpp)`);
  console.log(`  preview tools/previewradar.png`);

  return { zoom: RADAR_ZOOM, originPx, originPy, scale, levels: display, bytes: bin.length };
}

// --- Header generado para el firmware --------------------------------------
function renderHeader(results, radar) {
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

// Centro con el que se genero este mapa, leido de include/config.h. Tiene que
// seguir coincidiendo con HOME_LAT/HOME_LON: RadarScreen.cpp lo verifica con un
// static_assert. Sin esto, mover la casa en config.h y no volver a correr el
// generador compilaba igual y dejaba los aviones sobre calles que no son.
static constexpr double MAP_ORIGIN_LAT = ${HOME_LAT};
static constexpr double MAP_ORIGIN_LON = ${HOME_LON};

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

// ---------------------------------------------------------------------------
//  Mapa de fondo del radar
// ---------------------------------------------------------------------------
// Va aparte de MAP_ASSETS porque tiene otro formato: 4 bpp indexado y ya
// empaquetado como lo espera TFT_eSprite, para poder meterlo al sprite con un
// memcpy. Ademas viene sin etiquetas de ciudades y recortado en circulo.

static const int RADAR_DISC_SIZE = ${RADAR_DISC_SIZE};
static const int RADAR_RING_MAX  = ${RADAR_RING_MAX};

// Alcance real del anillo exterior. Tiene que coincidir con RADAR_RANGE_KM de
// config.h: RadarScreen.cpp lo verifica con un static_assert.
static const int RADAR_MAP_RANGE_KM = ${RADAR_RANGE_KM};

// (200 * 200) / 2 = 20.000 bytes
static const uint32_t RADAR_MAP_BYTES =
    (uint32_t)RADAR_DISC_SIZE * RADAR_DISC_SIZE / 2;

static const MapAsset RADAR_MAP =
    { "/radar.bin", ${radar.zoom}, ${radar.originPx.toFixed(4)}, ${radar.originPy.toFixed(4)}, ${radar.scale.toFixed(8)}f, ${RADAR_RANGE_KM} };

// Los ${RADAR_GREYS} grises del mapa, en RGB565. Salen de los cuantiles del
// histograma real de la imagen, no de una escala fija: el basemap oscuro usa un
// rango angosto y repartir niveles parejos entre negro y blanco desperdiciaria
// casi todos. Ocupan los indices 1..${RADAR_GREYS} de la paleta del disco.
static const int RADAR_MAP_GREY_COUNT = ${RADAR_GREYS};
static const uint16_t RADAR_MAP_GREYS[RADAR_MAP_GREY_COUNT] = {
${radar.levels
  .map((v) => {
    const rgb565 = ((v >> 3) << 11) | ((v >> 2) << 5) | (v >> 3);
    return `  0x${rgb565.toString(16).padStart(4, '0').toUpperCase()}, // luminancia ${v}`;
  })
  .join('\n')}
};
`;
}

// --- Main ------------------------------------------------------------------
async function main() {
  console.log('Generando el mapa de desk-radar');

  const home = await readHomeFromConfig();
  HOME_LAT = home.lat;
  HOME_LON = home.lon;
  console.log(`  centro: ${HOME_LAT}, ${HOME_LON}  (leido de include/config.h)`);
  console.log(`  viewport: ${VIEW_W}x${VIEW_H} px`);

  await mkdir(join(ROOT, 'data'), { recursive: true });

  const results = [];
  for (const mode of MODES) {
    results.push(await buildMode(mode));
  }

  const radar = await buildRadarMap();

  const headerPath = join(ROOT, 'src', 'MapAssets.h');
  await writeFile(headerPath, renderHeader(results, radar));
  console.log(`\nescrito src/MapAssets.h`);

  const total = results.length * VIEW_W * VIEW_H * 2 + radar.bytes;
  console.log(`\nTotal en flash: ${total.toLocaleString()} bytes`);
  console.log('\nAhora:  pio run -t uploadfs   (sube los mapas)');
  console.log('        pio run -t upload     (sube el firmware)');
}

main().catch((err) => {
  console.error('\nFallo la generacion del mapa:');
  console.error(err.message);
  process.exit(1);
});
