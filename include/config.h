#pragma once

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
