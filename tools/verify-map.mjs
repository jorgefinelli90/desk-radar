// ---------------------------------------------------------------------------
//  Verificador de la proyeccion del mapa
//
//  Aplica exactamente la misma formula que src/utils/GeoMap.h sobre puntos conocidos
//  (aeropuertos, tu casa, el Obelisco) y los marca sobre los previews. Si los
//  circulos caen donde corresponde en el mapa, la proyeccion del firmware esta
//  bien; si estan corridos, hay un bug de proyeccion.
//
//  Es la unica forma de validar esto sin flashear la placa.
//
//  Uso:  node verify-map.mjs     (correr despues de build-map.mjs)
//  Sale: check80.png, check40.png
// ---------------------------------------------------------------------------

import { readFile, writeFile } from 'node:fs/promises';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import sharp from 'sharp';

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, '..');

const LANDMARKS = [
  { name: 'CASA',       lat: -34.5858006, lon: -58.5917033, color: '#ffee00' },
  { name: 'Ezeiza',     lat: -34.8222,    lon: -58.5358,    color: '#ff3b3b' },
  { name: 'Aeroparque', lat: -34.5592,    lon: -58.4156,    color: '#ff3b3b' },
  { name: 'Palomar',    lat: -34.6098,    lon: -58.6122,    color: '#ff3b3b' },
  { name: 'Obelisco',   lat: -34.6037,    lon: -58.3816,    color: '#3bd0ff' },
  { name: 'La Plata',   lat: -34.9215,    lon: -57.9545,    color: '#3bd0ff' },
];

// --- Misma proyeccion que src/utils/GeoMap.h -------------------------------
const TILE_SIZE = 256;

function lonToWorldPx(lon, zoom) {
  return ((lon + 180) / 360) * TILE_SIZE * 2 ** zoom;
}

function latToWorldPy(lat, zoom) {
  const s = Math.sin((lat * Math.PI) / 180);
  return (0.5 - Math.log((1 + s) / (1 - s)) / (4 * Math.PI)) * TILE_SIZE * 2 ** zoom;
}

// --- Lee las constantes reales de MapAssets.h ------------------------------
// A proposito NO se recalculan: si el header y el .bin se desincronizaran,
// este verificador tiene que mostrarlo.
async function readAssets() {
  const src = await readFile(join(ROOT, 'src', 'MapAssets.h'), 'utf8');

  const viewW = Number(/MAP_VIEW_W = (\d+)/.exec(src)[1]);
  const viewH = Number(/MAP_VIEW_H = (\d+)/.exec(src)[1]);

  const re = /\{\s*"\/map(\d+)\.bin",\s*(\d+),\s*([-\d.]+),\s*([-\d.]+),\s*([-\d.]+)f,\s*(\d+)\s*\}/g;
  const assets = [];
  let m;
  while ((m = re.exec(src)) !== null) {
    assets.push({
      name: m[1],
      zoom: Number(m[2]),
      originPx: Number(m[3]),
      originPy: Number(m[4]),
      scale: Number(m[5]),
      widthKm: Number(m[6]),
    });
  }
  if (!assets.length) throw new Error('no pude parsear MAP_ASSETS de src/MapAssets.h');

  return { viewW, viewH, assets };
}

async function checkAsset(asset, viewW, viewH) {
  const overlays = [];
  const rows = [];

  for (const lm of LANDMARKS) {
    const x = (lonToWorldPx(lm.lon, asset.zoom) - asset.originPx) * asset.scale;
    const y = (latToWorldPy(lm.lat, asset.zoom) - asset.originPy) * asset.scale;
    const inside = x >= 0 && x < viewW && y >= 0 && y < viewH;

    rows.push(
      `    ${lm.name.padEnd(11)} x=${x.toFixed(1).padStart(7)}  y=${y.toFixed(1).padStart(7)}  ${
        inside ? 'dentro' : 'FUERA del recorte'
      }`
    );

    if (!inside) continue;

    const label = lm.name === 'CASA' ? lm.name : lm.name;
    overlays.push(`
      <circle cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" r="5"
              fill="none" stroke="${lm.color}" stroke-width="2"/>
      <circle cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" r="1.5" fill="${lm.color}"/>
      <text x="${(x + 7).toFixed(1)}" y="${(y + 3).toFixed(1)}"
            font-family="monospace" font-size="9" fill="${lm.color}"
            stroke="#000" stroke-width="2.5" paint-order="stroke">${label}</text>`);
  }

  const svg = `<svg width="${viewW}" height="${viewH}" xmlns="http://www.w3.org/2000/svg">${overlays.join('')}</svg>`;

  const out = join(HERE, `check${asset.name}.png`);
  await sharp(join(HERE, `preview${asset.name}.png`))
    .composite([{ input: Buffer.from(svg) }])
    .png()
    .toFile(out);

  console.log(`\n--- modo ${asset.widthKm} km (zoom ${asset.zoom}) ---`);
  console.log(rows.join('\n'));
  console.log(`    -> tools/check${asset.name}.png`);
}

// El mapa del radar se guarda en 4 bpp indexado con el mismo empaquetado que
// usa TFT_eSprite ((x + y*w)>>1, nibble alto para x par), porque el firmware lo
// mete al sprite con un memcpy. Si ese layout no coincide, el disco sale hecho
// puré. Aca se decodifica el .bin DE VUELTA con la formula de la libreria: si
// checkradar.png se parece al mapa, el empaquetado esta bien.
async function checkRadarMap() {
  const src = await readFile(join(ROOT, 'src', 'MapAssets.h'), 'utf8');
  const size = Number(/RADAR_DISC_SIZE = (\d+)/.exec(src)[1]);
  const greys = [...src.matchAll(/0x[0-9A-F]{4}, \/\/ luminancia (\d+)/g)].map((m) => Number(m[1]));

  const bin = await readFile(join(ROOT, 'data', 'radar.bin'));
  const expected = (size * size) / 2;
  if (bin.length !== expected) {
    throw new Error(`data/radar.bin mide ${bin.length} bytes, se esperaban ${expected}`);
  }

  const rgb = Buffer.alloc(size * size * 3);
  const hist = {};
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const o = (x + y * size) >> 1;
      const idx = (x & 1) === 0 ? (bin[o] >> 4) & 0x0f : bin[o] & 0x0f;
      hist[idx] = (hist[idx] || 0) + 1;
      const v = idx === 0 ? 0 : greys[idx - 1];
      const p = (y * size + x) * 3;
      rgb[p] = rgb[p + 1] = rgb[p + 2] = v;
    }
  }

  await sharp(rgb, { raw: { width: size, height: size, channels: 3 } })
    .png()
    .toFile(join(HERE, 'checkradar.png'));

  // El indice 0 es el negro de afuera del circulo. Su proporcion tiene que dar
  // 1 - pi*r^2/lado^2: si no da, la mascara o el empaquetado estan mal.
  const outside = (100 * (hist[0] || 0)) / (size * size);
  console.log(`\n--- mapa del radar (${size}x${size}, 4 bpp) ---`);
  console.log(`    grises: ${greys.join(', ')}`);
  console.log(
    `    uso de indices: ${Object.entries(hist)
      .map(([k, v]) => `${k}:${((100 * v) / (size * size)).toFixed(1)}%`)
      .join('  ')}`
  );
  console.log(`    fuera del circulo: ${outside.toFixed(1)}%`);
  console.log(`    -> tools/checkradar.png (decodificado del .bin, no del preview)`);
}

async function main() {
  const { viewW, viewH, assets } = await readAssets();
  console.log(`Verificando la proyeccion contra src/MapAssets.h (viewport ${viewW}x${viewH})`);

  for (const a of assets) {
    await checkAsset(a, viewW, viewH);
  }

  await checkRadarMap();

  console.log('\nAbri los check*.png: cada circulo tiene que caer sobre el lugar real.');
  console.log('Si estan corridos en vertical, el sospechoso es la formula de Mercator.');
}

main().catch((e) => {
  console.error('Fallo la verificacion:', e.message);
  process.exit(1);
});
