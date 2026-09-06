#pragma once
#include <stdint.h>   // uint32_t: sin esto el header solo compila si alguien
                      // incluyó Arduino.h antes que a config.h

// ============================================================
//  CONFIGURACIÓN GENERAL - Flight Radar de escritorio
// ============================================================

// --- Orientación de la pantalla ---
// Rotación de TFT_eSPI: 0 = vertical (conector abajo), 2 = vertical girada 180
// (conector arriba). 1 y 3 son los dos horizontales. Todas las pantallas del
// proyecto están pensadas para vertical (240x320), así que usá 0 o 2.
// OJO: la calibración del touch depende de la rotación. TouchManager guarda con
// qué rotación calibró y vuelve a correr el wizard solo si cambiás este valor.
static const uint8_t SCREEN_ROTATION = 2;

// --- Sensibilidad del touch ---
// Presión (valor Z del XPT2046) a partir de la cual TFT_eSPI considera que
// hay un toque. El default de la librería en getTouch() es 600, pero el wizard
// de calibración valida los toques con Z_THRESHOLD/2 = 175: calibrás con un
// toque suave y después, para usarlo, te exige 3 veces más presión. De ahí la
// sensación de "toco y no pasa nada". 350 es el propio Z_THRESHOLD de la
// librería y deja pasar el toque normal de un dedo.
// Si te registra toques fantasma, subilo; si tenés que apretar fuerte, bajalo.
static const uint16_t TOUCH_PRESSURE = 350;

// Ventana anti-rebote entre toques aceptados. Tiene que cubrir el temblor del
// dedo al soltar, no la velocidad a la que navegás: con 250 ms un segundo
// toque intencional (ej. avanzar de aeropuerto) se comía si ibas rápido.
static const uint32_t TOUCH_DEBOUNCE_MS = 120;

// Poner en 1 para forzar el wizard de calibración en el próximo arranque,
// sin tener que cambiar SCREEN_ROTATION. Útil si te quedó torcida (tocás un
// botón y responde el de al lado). Acordate de volverlo a 0 después.
#define FORCE_TOUCH_CALIBRATION 0

// --- Version del firmware ---
// Se muestra en el panel web. Sirve para confirmar de un vistazo que una
// actualizacion por WiFi entro de verdad: si el numero cambio, el dispositivo
// esta corriendo el binario nuevo.
static const char* FIRMWARE_VERSION = "1.3.0";

// --- Ubicación de referencia (tu casa) ---
// constexpr y no const, por el mismo motivo que RADAR_RANGE_KM: RadarScreen.cpp
// los compara contra el centro del mapa generado en un static_assert, y para eso
// tienen que ser expresión constante. tools/build-map.mjs LEE estos dos valores
// de acá, así que este archivo es la única fuente de verdad de dónde está la
// casa: si los cambiás, volvé a correr el generador.
static constexpr double HOME_LAT = -34.5858006;
static constexpr double HOME_LON = -58.5917033;

// --- Radar: alcance y refresco adaptativo ---
// constexpr y no const: RadarScreen.cpp lo compara con el alcance del mapa
// generado en un static_assert, y para eso tiene que ser expresión constante.
static constexpr double RADAR_RANGE_KM   = 20.0;  // radio máximo mostrado en el radar (debe coincidir con RADAR_MAP_RANGE_KM de MapAssets.h)
static const double RADAR_NEAR_KM        = 10.0;  // distancia bajo la cual se considera "tráfico cercano"
static const uint32_t REFRESH_NORMAL_MS  = 30000; // refresco normal (30s)
static const uint32_t REFRESH_FAST_MS    = 5000;  // refresco cuando hay tráfico cercano (5s)

// --- Bounding box compartido por Radar y Mapa ---
// Radar y Mapa miran la misma zona, así que hacen UN SOLO fetch a OpenSky y
// comparten el vector de aviones: cambiar de una pantalla a la otra no dispara
// una request nueva.
//
// El que manda es el Mapa en modo lejano (80 km de ancho x 87,3 km de alto,
// ver src/MapAssets.h): 45 km de radio lo cubre entero, y de paso le sobra al
// radar, que ahora llega a 20 km. Si tocás el viewport del mapa en
// tools/build-map.mjs, revisá que este radio siga alcanzando.
static const double HOME_FETCH_RADIUS_KM = 45.0;

// --- Aeropuertos fijos para el modo "Aeropuertos" ---
struct AirportDef {
  const char* icao;
  const char* name;
  double lat;
  double lon;
  double boxRadiusKm; // radio del bounding box alrededor del aeropuerto
};

static const AirportDef AIRPORTS[] = {
  { "SAEZ", "Ezeiza",           -34.8222, -58.5358, 25.0 },
  { "SABE", "Aeroparque",       -34.5592, -58.4156, 20.0 },
  { "SADP", "El Palomar",       -34.6098, -58.6122, 20.0 },
};
static const int AIRPORT_COUNT = sizeof(AIRPORTS) / sizeof(AIRPORTS[0]);

// Altitud máxima (metros) para considerar un avión "operando cerca" del aeropuerto
static const double AIRPORT_MAX_ALT_M = 3000.0;

// --- OpenSky API ---
static const char* OPENSKY_TOKEN_URL  = "https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token";
static const char* OPENSKY_STATES_URL = "https://opensky-network.org/api/states/all";

// --- Noticias (GNews.io) ---
// Free tier: 100 requests/día, sin tarjeta de crédito, hasta 10 artículos por
// request. Los titulares del plan gratuito llegan con ~12h de demora.
static const char* GNEWS_URL      = "https://gnews.io/api/v4/top-headlines";
static const char* NEWS_LANG      = "es";
static const char* NEWS_COUNTRY   = "ar";
static const char* NEWS_CATEGORY  = "general";
static const int   NEWS_MAX_ITEMS = 5;

// --- Clima (Open-Meteo) ---
// No necesita API key ni registro para uso no comercial. Se consulta sobre
// HOME_LAT/HOME_LON y devuelve la hora local gracias a timezone=auto.
static const char* OPENMETEO_URL = "https://api.open-meteo.com/v1/forecast";

// --- Cache de noticias y clima ---
// Son datos que cambian lento: los guardamos en RAM y no volvemos a pegarle a
// la API mientras el cache siga vigente. Así el free tier de GNews (100
// requests/día) alcanza de sobra aunque dejes la pantalla puesta.
static const uint32_t NEWS_CACHE_MS    = 20UL * 60UL * 1000UL; // 20 min
static const uint32_t WEATHER_CACHE_MS = 15UL * 60UL * 1000UL; // 15 min

// Si una request falla, esperamos esto antes de reintentar (no martillar la API)
static const uint32_t API_RETRY_MS = 60UL * 1000UL;

// --- Hora local y NTP ---
// Argentina es UTC-3 todo el año (no hay horario de verano), así que alcanza un
// offset fijo y no hace falta arrastrar la base de datos de zonas horarias.
// Lo usan dos cosas: el reloj de la barra de estado y la conversión de la hora
// de publicación de los titulares, que GNews devuelve en UTC.
static const int TZ_OFFSET_H = -3;

// Servidores NTP. El segundo es el respaldo por si el pool no resuelve.
// La sincronización no bloquea: SNTP corre en segundo plano y la hora aparece
// sola unos segundos después de que haya IP.
static const char* NTP_SERVER_1 = "pool.ntp.org";
static const char* NTP_SERVER_2 = "time.google.com";

// A partir de acá los datos de OpenSky se consideran viejos y la barra de
// estado lo avisa en ámbar. Tiene que ser bastante mayor que REFRESH_NORMAL_MS
// (30 s) para no encenderse en cada ciclo normal, pero lo bastante corto como
// para notarse si la API dejó de responder.
static const uint32_t STALE_DATA_MS = 90UL * 1000UL;

// --- Animación del barrido del radar ---
// El barrido gira solo, independiente del ciclo de fetch de OpenSky: usa la
// última posición conocida de cada avión. Bajar SWEEP_PERIOD_MS acelera la
// vuelta; subir RADAR_FRAME_MS baja los FPS (y libera bus SPI para el touch).
static const uint32_t RADAR_SWEEP_PERIOD_MS = 3500; // una vuelta completa
static const uint32_t RADAR_FRAME_MS        = 45;   // ~22 fps (ver nota abajo)
static const int      RADAR_TRAIL_DEG       = 48;   // largo de la estela
static const int      RADAR_TRAIL_STEPS     = 3;    // bandas de brillo (la paleta de 16 solo deja 3: el resto son grises del mapa)
static const uint32_t RADAR_PING_MS         = 700;  // destello al ser detectado

// Pausa del loop mientras el radar está en pantalla. El loop general usa 30 ms,
// pero acá la vuelta ya cuesta ~25 ms sola (≈5 ms del getTouch de TFT_eSPI, que
// hace 5 muestreos con delay(1) cada uno, + ~20 ms de volcar el sprite), así que
// con 30 ms encima los frames caerían cada ~80 ms (12 fps) y el barrido se vería
// a saltos. Con 5 ms el frame queda gobernado por RADAR_FRAME_MS y el touch se
// consulta más seguido, no menos.
static const uint32_t RADAR_LOOP_DELAY_MS = 5;

// Poner en 1 para que el radar informe por serie cuánto tarda cada frame y a
// cuántos fps está yendo. Útil si querés retocar SWEEP_PERIOD_MS / FRAME_MS.
#define RADAR_DEBUG_TIMING 0

// --- WiFi ---
// Cuánto espera el arranque a que entre la red antes de seguir igual. Es lo
// ÚNICO que bloquea esperando WiFi en todo el firmware: durante setup() no hay
// ninguna pantalla útil para mostrar, así que ahí sí conviene esperar. En el
// loop no se espera nunca (ver wifiTick() en main.cpp).
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;

// Cada cuánto se vuelve a empujar un WiFi.begin() mientras la red está caída.
// setAutoReconnect() ya reintenta solo, pero si el router estuvo apagado un
// rato largo el driver deja de insistir y hay que darle un empujón. begin() no
// bloquea: arranca el intento y vuelve enseguida.
static const uint32_t WIFI_RETRY_MS = 15000;

// --- Configuración por web / portal cautivo ---
// Si no hay credenciales WiFi en NVS, el ESP32 arranca como Access Point con
// este nombre y sirve un portal cautivo para configurarlo desde el celular.
// Dejar SETUP_AP_PASSWORD en "" para que el AP quede abierto (ojo: WPA2 pide
// mínimo 8 caracteres, una clave más corta hace fallar el softAP en silencio).
#define SETUP_AP_SSID     "DeskRadar-Setup"
#define SETUP_AP_PASSWORD "radar1234"

// Hostname de mDNS: el dispositivo queda en http://desk-radar.local/
#define DEVICE_HOSTNAME   "desk-radar"

// Usuario del panel web. La contraseña es el PIN que se carga desde el propio
// panel (campo "PIN del panel web"): mientras esté vacío, el panel queda
// abierto a toda la red y la página de estado lo avisa.
static const char* WEB_AUTH_USER = "admin";

// --- Banner de mensajes ---
// Tiempos de la animación del banner que aparece cuando llega un mensaje por
// POST /api/message.
static const uint32_t BANNER_SLIDE_MS = 280;   // entrada y salida
static const uint32_t BANNER_HOLD_MS  = 5500;  // cuánto queda quieto
static const uint32_t BANNER_FRAME_MS = 25;    // ~40 fps mientras se desliza

// Poner en 1 para entrar al portal de configuración aunque ya haya WiFi
// guardado. Sirve para probar o retocar el portal sin borrar NVS (o sea, sin
// perder la calibración del touch, que vive en la misma partición).
// Acordate de volverlo a 0: con esto en 1 el dispositivo nunca conecta.
#define FORCE_SETUP_PORTAL 0
