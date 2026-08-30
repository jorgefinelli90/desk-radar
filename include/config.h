#pragma once
#include <stdint.h>   // uint32_t: sin esto el header solo compila si alguien
                      // incluyó Arduino.h antes que a config.h

// ============================================================
//  CONFIGURACIÓN GENERAL - Flight Radar de escritorio
// ============================================================

// --- Ubicación de referencia (tu casa) ---
static const double HOME_LAT = -34.5858006;
static const double HOME_LON = -58.5917033;

// --- Radar: alcance y refresco adaptativo ---
static const double RADAR_RANGE_KM       = 40.0;  // radio máximo mostrado en el radar
static const double RADAR_NEAR_KM        = 10.0;  // distancia bajo la cual se considera "tráfico cercano"
static const uint32_t REFRESH_NORMAL_MS  = 30000; // refresco normal (30s)
static const uint32_t REFRESH_FAST_MS    = 5000;  // refresco cuando hay tráfico cercano (5s)

// --- Bounding box para /states/all en modo radar ---
// Se calcula en runtime a partir de HOME_LAT/HOME_LON + RADAR_RANGE_KM (ver GeoUtils)

// --- Mapa de la provincia de Buenos Aires (modo "Mapa BA") ---
// Bounding box que cubre toda la provincia (de Patagones al sur hasta el
// límite con Santa Fe/Córdoba al norte, de La Pampa al Atlántico).
static const double MAP_BA_LAT_MIN = -41.10; // sur  (Carmen de Patagones)
static const double MAP_BA_LAT_MAX = -33.20; // norte (límite con Santa Fe)
static const double MAP_BA_LON_MIN = -63.50; // oeste (límite con La Pampa)
static const double MAP_BA_LON_MAX = -56.60; // este  (costa atlántica)
static const uint32_t REFRESH_MAP_MS = 45000; // refresco del mapa (zona grande, se mueve poco a esta escala)

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

// --- Botón físico ---
// GPIO27 elegido por estar libre: no choca con el bus SPI de la pantalla/touch
// (2,4,5,15,18,19,21,23,17) ni con los pines de flash interna (6-11) ni con
// pines de arranque (0,12).
static const int PIN_BUTTON = 27;
static const uint32_t LONG_PRESS_MS = 600;

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

// GNews devuelve publishedAt en UTC. Argentina es UTC-3 todo el año.
static const int NEWS_TZ_OFFSET_H = -3;

// --- Animación del barrido del radar ---
// El barrido gira solo, independiente del ciclo de fetch de OpenSky: usa la
// última posición conocida de cada avión. Bajar SWEEP_PERIOD_MS acelera la
// vuelta; subir RADAR_FRAME_MS baja los FPS (y libera bus SPI para el touch).
static const uint32_t RADAR_SWEEP_PERIOD_MS = 3500; // una vuelta completa
static const uint32_t RADAR_FRAME_MS        = 45;   // ~22 fps (ver nota abajo)
static const int      RADAR_TRAIL_DEG       = 70;   // largo de la estela
static const int      RADAR_TRAIL_STEPS     = 7;    // bandas de brillo del trail
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
