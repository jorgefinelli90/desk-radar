// ---------------------------------------------------------------------------
//  Detecta tu ubicacion por IP y la escribe en include/config.h
//
//  Pensado para el primer setup: en vez de buscar tus coordenadas a mano
//  (Google Maps -> click derecho -> copiar), este script le pregunta a un
//  servicio de geolocalizacion por IP donde estas y completa HOME_LAT/HOME_LON
//  por vos.
//
//  OJO: la geolocalizacion por IP es aproximada -normalmente acierta la
//  ciudad, no la direccion exacta- porque depende de donde tu proveedor de
//  internet registro esa IP, que puede estar a varios km de tu casa real (mas
//  todavia con datos moviles o VPN). Para el radar alcanza de sobra (un error
//  de pocos km no cambia que aviones ves), pero si te importa la precision
//  -por ejemplo para que el mapa quede perfectamente centrado en tu techo-
//  conviene corregir a mano despues con las coordenadas exactas de Google
//  Maps.
//
//  Uso:
//      node tools/detect-location.mjs             # solo muestra lo que detecto
//      node tools/detect-location.mjs --write      # ademas lo escribe en config.h
//
//  Despues de escribir, falta correr build-map.mjs para regenerar el mapa
//  local con el nuevo centro (ver el mensaje final).
// ---------------------------------------------------------------------------

import { readFileSync, writeFileSync } from "fs";
import { fileURLToPath } from "url";
import { dirname, join } from "path";

const ROOT = dirname(dirname(fileURLToPath(import.meta.url)));
const CONFIG_PATH = join(ROOT, "include", "config.h");

const WRITE = process.argv.includes("--write");

async function main() {
  console.log("Detectando ubicacion por IP...");
  const res = await fetch("https://ipinfo.io/json");
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  const info = await res.json();

  if (!info.loc) {
    throw new Error(
      "La respuesta no trajo 'loc'. Puede ser un limite de uso del servicio " +
        "gratuito (ipinfo.io); probá de nuevo en un rato, o cargá las " +
        "coordenadas a mano en include/config.h."
    );
  }

  const [latStr, lonStr] = info.loc.split(",");
  const lat = Number(latStr);
  const lon = Number(lonStr);

  const lugar = [info.city, info.region, info.country].filter(Boolean).join(", ");
  console.log(`\nDetectado: ${lugar || "(sin nombre)"}`);
  console.log(`  HOME_LAT = ${lat}`);
  console.log(`  HOME_LON = ${lon}`);
  console.log(
    "\nEsto es aproximado (geolocalizacion por IP, no GPS): puede estar a\n" +
      "varios km de tu direccion real. Para mas precision, buscate en Google\n" +
      "Maps, click derecho sobre tu casa y copiá las coordenadas de ahi."
  );

  if (!WRITE) {
    console.log("\nNo se modifico nada (corré con --write para aplicarlo a config.h).");
    return;
  }

  let config = readFileSync(CONFIG_PATH, "utf8");

  const replace = (name, value) => {
    const re = new RegExp(`(static constexpr double ${name} = )[^;]+;`);
    if (!re.test(config)) throw new Error(`no encontré ${name} en include/config.h`);
    config = config.replace(re, `$1${value};`);
  };
  replace("HOME_LAT", lat);
  replace("HOME_LON", lon);

  writeFileSync(CONFIG_PATH, config);
  console.log("\ninclude/config.h actualizado.");
  console.log("Ahora falta regenerar el mapa local con el nuevo centro:");
  console.log("  cd tools && node build-map.mjs && cd .. && pio run -t uploadfs");
}

main().catch((e) => {
  console.error("Error:", e.message);
  process.exit(1);
});
