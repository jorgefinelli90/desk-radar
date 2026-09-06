// ---------------------------------------------------------------------------
//  Generador del mapa mundial en miniatura para la pantalla ISS
//
//  Baja el contorno de tierra de Natural Earth (110m, dominio publico) y lo
//  rasteriza a un bitmap de 1 bit por pixel (tierra/agua), proyeccion
//  equirectangular (x = longitud, y = latitud, lineal). No hace falta ninguna
//  libreria de geometria: point-in-polygon con ray casting alcanza para un
//  contorno de esta resolucion.
//
//  Uso:
//      node tools/build-worldmap.mjs
//
//  Salida:
//      src/models/WorldMapAsset.h   bitmap empaquetado + WORLD_MAP_W/H (GENERADO)
// ---------------------------------------------------------------------------

import { writeFileSync } from "fs";

const SOURCE_URL =
  "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/ne_110m_land.geojson";

// 2:1 (equirectangular). Suficiente para reconocer continentes en una pantalla
// de 240x320; mas resolucion no se nota y pesa mas flash.
const W = 220;
const H = 110;

function pointInRing(x, y, ring) {
  // Ray casting estandar: cuenta cruces del rayo horizontal hacia +x.
  let inside = false;
  for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) {
    const [xi, yi] = ring[i];
    const [xj, yj] = ring[j];
    const intersects =
      yi > y !== yj > y && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi;
    if (intersects) inside = !inside;
  }
  return inside;
}

function pointInPolygon(x, y, rings) {
  // rings[0] es el contorno exterior, el resto son agujeros.
  if (!pointInRing(x, y, rings[0])) return false;
  for (let k = 1; k < rings.length; k++) {
    if (pointInRing(x, y, rings[k])) return false; // cae en un agujero
  }
  return true;
}

async function main() {
  console.log("Bajando contorno de tierra (Natural Earth 110m)...");
  const res = await fetch(SOURCE_URL);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  const geo = await res.json();

  // Junta todos los poligonos (Polygon y MultiPolygon) en una lista plana de
  // "rings-por-poligono", con su bbox para descartar rapido.
  const polys = [];
  for (const f of geo.features) {
    const g = f.geometry;
    const polyList =
      g.type === "Polygon" ? [g.coordinates] : g.coordinates; // MultiPolygon
    for (const rings of polyList) {
      let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
      for (const [x, y] of rings[0]) {
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
      }
      polys.push({ rings, minX, maxX, minY, maxY });
    }
  }
  console.log(`${polys.length} poligonos de tierra cargados.`);

  // Bitmap empaquetado: 1 bit por pixel, filas de ceil(W/8) bytes, MSB primero
  // (mismo orden que usan las fuentes/iconos de TFT_eSPI).
  const rowBytes = Math.ceil(W / 8);
  const bitmap = new Uint8Array(rowBytes * H);

  for (let py = 0; py < H; py++) {
    // Centro del pixel en latitud: fila 0 = polo norte (+90), fila H-1 = polo
    // sur (-90).
    const lat = 90 - ((py + 0.5) / H) * 180;
    for (let px = 0; px < W; px++) {
      const lon = ((px + 0.5) / W) * 360 - 180;

      let land = false;
      for (const p of polys) {
        if (lon < p.minX || lon > p.maxX || lat < p.minY || lat > p.maxY) continue;
        if (pointInPolygon(lon, lat, p.rings)) { land = true; break; }
      }

      if (land) {
        const byteIdx = py * rowBytes + (px >> 3);
        const bitIdx = 7 - (px & 7);
        bitmap[byteIdx] |= 1 << bitIdx;
      }
    }
    if (py % 20 === 0) console.log(`  fila ${py}/${H}`);
  }

  const landBits = bitmap.reduce(
    (acc, b) => acc + b.toString(2).split("1").length - 1,
    0
  );
  console.log(`Listo: ${landBits} de ${W * H} pixeles son tierra.`);

  const hex = Array.from(bitmap)
    .map((b) => "0x" + b.toString(16).padStart(2, "0"))
    .join(",");

  // Se corta en lineas para que no quede una sola linea gigante en el archivo.
  const wrapped = hex.match(/.{1,120}/g).join("\n  ");

  const header = `#pragma once
#include <stdint.h>

// ===========================================================================
//  ARCHIVO GENERADO POR tools/build-worldmap.mjs - NO EDITAR A MANO
//
//  Contorno de tierra a baja resolucion (proyeccion equirectangular), 1 bit
//  por pixel: bit en 1 = tierra, bit en 0 = agua. Se usa para dibujar el
//  mapamundi de fondo en ISSScreen.
//
//  Fuente: Natural Earth 110m Land (dominio publico, naturalearthdata.com).
// ===========================================================================

static const int WORLD_MAP_W = ${W};
static const int WORLD_MAP_H = ${H};
static const int WORLD_MAP_ROW_BYTES = ${rowBytes};

static const uint8_t WORLD_MAP_BITS[${bitmap.length}] = {
  ${wrapped}
};
`;

  writeFileSync(new URL("../src/models/WorldMapAsset.h", import.meta.url), header);
  console.log("Escrito src/models/WorldMapAsset.h");
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
