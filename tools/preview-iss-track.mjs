// ---------------------------------------------------------------------------
//  Previsualiza la traza de la orbita de la ISS en ASCII
//
//  El primo de verify-map.mjs, para la pantalla ISS: reproduce exactamente lo
//  que hace el firmware -mismo endpoint, misma ventana de tiempo, misma
//  proyeccion equirectangular- y lo dibuja en la terminal. Sirve para
//  contestar "la curva que veo en la pantalla, ¿es real o hay un bug en la
//  proyeccion?" sin tener que compilar, flashear y sacarle una foto a la
//  pantalla.
//
//  Lo que hay que ver: 'o' (recorrida) y '*' (por venir) tienen que formar UNA
//  sola curva continua que pase por '@' (la posicion actual), sin saltos ni
//  vueltas para atras. La longitud siempre avanza hacia el este: la ISS orbita
//  en sentido directo, nunca retrocede. Si la traza se ve partida o doblando
//  para atras, ahi si hay algo mal.
//
//  Uso:
//      node tools/preview-iss-track.mjs
//
//  Los parametros de abajo tienen que coincidir con ISS_TRACK_HALF y
//  ISS_TRACK_STEP_S de include/config.h.
// ---------------------------------------------------------------------------

const HALF = 12;   // ISS_TRACK_HALF
const STEP = 100;  // ISS_TRACK_STEP_S (segundos)

// Grilla de la terminal. Mismo aspecto 2:1 que el mapa del firmware (220x110),
// mas chica para que entre en una consola.
const W = 110;
const H = 46;

// Tienen que coincidir con include/config.h (solo para dibujar la cruz de casa)
const HOME_LAT = -34.5858006;
const HOME_LON = -58.5917033;

const ISS_URL = "https://api.wheretheiss.at/v1/satellites/25544";

async function main() {
  const now = Math.floor(Date.now() / 1000);

  const pos = await (await fetch(ISS_URL)).json();

  const ts = [];
  for (let i = -HALF; i <= HALF; i++) {
    if (i === 0) continue; // el "ahora" lo da el fetch de posicion, igual que en el firmware
    ts.push(now + i * STEP);
  }
  const trk = await (await fetch(`${ISS_URL}/positions?timestamps=${ts.join(",")}`)).json();

  const past = trk.slice(0, HALF);
  const future = trk.slice(HALF);

  // Misma proyeccion que ISSScreen::worldX/worldY
  const px = (lon) => Math.round(((lon + 180) / 360) * W);
  const py = (lat) => Math.round(((90 - lat) / 180) * H);

  const grid = Array.from({ length: H + 1 }, () => Array(W + 1).fill("."));
  const put = (lat, lon, ch) => {
    const x = px(lon), y = py(lat);
    if (x >= 0 && x <= W && y >= 0 && y <= H) grid[y][x] = ch;
  };

  for (const p of past) put(p.latitude, p.longitude, "o");
  for (const p of future) put(p.latitude, p.longitude, "*");
  put(HOME_LAT, HOME_LON, "+");
  put(pos.latitude, pos.longitude, "@");

  const f = (n) => n.toFixed(1).padStart(7);
  console.log(`ventana: +-${(HALF * STEP) / 60} min, ${HALF} puntos por lado cada ${STEP}s\n`);
  console.log(`recorrida  lon ${f(past[0].longitude)} -> ${f(past[HALF - 1].longitude)}   lat ${f(past[0].latitude)} -> ${f(past[HALF - 1].latitude)}`);
  console.log(`ahora      lon ${f(pos.longitude)}              lat ${f(pos.latitude)}`);
  console.log(`por venir  lon ${f(future[0].longitude)} -> ${f(future[HALF - 1].longitude)}   lat ${f(future[0].latitude)} -> ${f(future[HALF - 1].latitude)}`);
  console.log("\no = recorrida    * = por venir    @ = ISS ahora    + = casa\n");

  for (const row of grid) console.log(row.join(""));
}

main().catch((e) => {
  console.error("Error:", e.message);
  process.exit(1);
});
