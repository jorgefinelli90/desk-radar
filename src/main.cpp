#include <Arduino.h>
#include <WiFi.h>
#include <vector>

#include "config.h"
#include "secrets.h"       // copiá secrets.h.example -> secrets.h y completá tus datos
#include "GeoUtils.h"
#include "OpenSkyClient.h"
#include "DisplayManager.h"
#include "TouchManager.h"
#include "HomeScreen.h"
#include "RadarScreen.h"
#include "AirportScreen.h"
#include "MapScreen.h"
#include "DetailScreen.h"
#include "InfoMenuScreen.h"
#include "NewsClient.h"
#include "NewsScreen.h"
#include "WeatherClient.h"
#include "WeatherScreen.h"

enum class Mode { Home, Radar, Airports, Map, Detail, InfoMenu, News, Weather };

DisplayManager display;
TouchManager   touch(display.tft());
OpenSkyClient  opensky(OPENSKY_CLIENT_ID, OPENSKY_CLIENT_SECRET);
NewsClient     newsClient(GNEWS_API_KEY);
WeatherClient  weatherClient;
HomeScreen     homeScreen(display);
RadarScreen    radarScreen(display);
AirportScreen  airportScreen(display);
MapScreen      mapScreen(display);
DetailScreen   detailScreen(display);
InfoMenuScreen infoMenuScreen(display);
NewsScreen     newsScreen(display);
WeatherScreen  weatherScreen(display);

Mode currentMode = Mode::Home;
Mode detailReturnMode = Mode::Radar; // a qué pantalla volver al salir del Detail
int  currentAirportIdx = 0;

// Copia del avión que se está viendo en modo Detail
AircraftState selectedAircraft;

std::vector<AircraftState> aircraft;
uint32_t lastFetch = 0;
bool fastMode = false;

void connectWiFi() {
  display.showMessage("Conectando WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(300);
  }

  if (WiFi.status() != WL_CONNECTED) {
    display.showMessage("Sin WiFi. Reintentando...");
  }
}

// Calcula distancia y bearing de cada avión respecto al punto de referencia dado
void enrichWithGeo(std::vector<AircraftState>& list, double refLat, double refLon) {
  for (auto& a : list) {
    a.distanceKm = GeoUtils::distanceKm(refLat, refLon, a.lat, a.lon);
    a.bearingDeg = GeoUtils::bearingDeg(refLat, refLon, a.lat, a.lon);
  }
}

void fetchForCurrentMode() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (currentMode == Mode::Radar) {
    auto box = GeoUtils::boundingBox(HOME_LAT, HOME_LON, RADAR_RANGE_KM);
    if (opensky.fetchStates(box, aircraft)) {
      enrichWithGeo(aircraft, HOME_LAT, HOME_LON);
    }
  } else if (currentMode == Mode::Map) {
    GeoUtils::BBox box { MAP_BA_LAT_MIN, MAP_BA_LAT_MAX, MAP_BA_LON_MIN, MAP_BA_LON_MAX };
    if (opensky.fetchStates(box, aircraft)) {
      enrichWithGeo(aircraft, HOME_LAT, HOME_LON); // distancia a casa: la usa el color y el detalle
    }
  } else if (currentMode == Mode::Airports) {
    const AirportDef& ap = AIRPORTS[currentAirportIdx];
    auto box = GeoUtils::boundingBox(ap.lat, ap.lon, ap.boxRadiusKm);
    if (opensky.fetchStates(box, aircraft)) {
      enrichWithGeo(aircraft, HOME_LAT, HOME_LON); // distancia mostrada siempre relativa a casa
    }
  } else if (currentMode == Mode::News) {
    // Cada cliente decide solo si le toca pedir o si el cache sigue vigente
    if (newsClient.shouldRefresh()) newsClient.refresh();
  } else if (currentMode == Mode::Weather) {
    if (weatherClient.shouldRefresh()) weatherClient.refresh();
  }

  lastFetch = millis();
}

void renderCurrentMode() {
  if (currentMode == Mode::Home) {
    homeScreen.render();
  } else if (currentMode == Mode::Radar) {
    radarScreen.render(aircraft, fastMode);
  } else if (currentMode == Mode::Map) {
    mapScreen.render(aircraft);
  } else if (currentMode == Mode::Detail) {
    detailScreen.render(selectedAircraft);
  } else if (currentMode == Mode::InfoMenu) {
    infoMenuScreen.render();
  } else if (currentMode == Mode::News) {
    newsScreen.render(newsClient);
  } else if (currentMode == Mode::Weather) {
    weatherScreen.render(weatherClient);
  } else {
    airportScreen.render(AIRPORTS[currentAirportIdx], aircraft, fastMode);
  }
}

void goHome() {
  currentMode = Mode::Home;
  renderCurrentMode();
}

void enterMode(Mode m) {
  currentMode = m;
  lastFetch = 0; // fuerza un fetch inmediato al entrar

  // Noticias y clima tardan un par de segundos en responder: avisamos en vez
  // de dejar la pantalla en negro mientras va la request.
  if (m == Mode::News)         display.showMessage("Buscando titulares...");
  else if (m == Mode::Weather) display.showMessage("Consultando el clima...");
  else if (m == Mode::Radar)   radarScreen.onEnter(); // limpia y reinicia el barrido

  fetchForCurrentMode();
  renderCurrentMode();
}

void setup() {
  Serial.begin(115200);
  display.begin();
  touch.begin(); // corre el wizard de calibración la primera vez
  connectWiFi();
  renderCurrentMode(); // Home
}

void loop() {
  uint16_t tx = 0, ty = 0;
  bool tapped = touch.getTap(tx, ty);

  if (currentMode == Mode::Home) {
    if (tapped) {
      HomeChoice choice = homeScreen.hitTest(tx, ty);
      if (choice == HomeChoice::Radar) {
        enterMode(Mode::Radar);
      } else if (choice == HomeChoice::Airports) {
        currentAirportIdx = 0;
        enterMode(Mode::Airports);
      } else if (choice == HomeChoice::Map) {
        enterMode(Mode::Map);
      } else if (choice == HomeChoice::Info) {
        enterMode(Mode::InfoMenu);
      }
    }
    delay(30);
    return;
  }

  // Submenu "MAS INFO": elige entre noticias y clima, o vuelve a Home tocando
  // la barra superior (misma convencion que el resto de las pantallas).
  if (currentMode == Mode::InfoMenu) {
    if (tapped) {
      if (ty < DisplayManager::STATUS_BAR_HEIGHT) {
        goHome();
      } else {
        InfoChoice choice = infoMenuScreen.hitTest(tx, ty);
        if (choice == InfoChoice::News) {
          enterMode(Mode::News);
        } else if (choice == InfoChoice::Weather) {
          enterMode(Mode::Weather);
        }
      }
    }
    delay(30);
    return;
  }

  // En Detail: cualquier toque vuelve a la pantalla de origen (radar o mapa).
  // La ficha queda congelada (no se refresca sola) para leerla con tranquilidad.
  if (currentMode == Mode::Detail) {
    if (tapped) {
      currentMode = detailReturnMode;
      // Volvemos al radar desde otra pantalla: hay que repintar el marco fijo
      if (currentMode == Mode::Radar) radarScreen.onEnter();
      renderCurrentMode();
    }
    delay(30);
    return;
  }

  // En Radar/Aeropuertos/Mapa/Noticias/Clima: tocar la franja superior
  // (barra de estado) vuelve a Home
  if (tapped && ty < DisplayManager::STATUS_BAR_HEIGHT) {
    goHome();
    delay(30);
    return;
  }

  // Noticias y clima no dependen del refresco adaptativo de OpenSky: cada
  // cliente tiene su propio cache y solo redibujamos cuando trajo datos nuevos.
  if (currentMode == Mode::News || currentMode == Mode::Weather) {
    bool shouldFetch = (currentMode == Mode::News) ? newsClient.shouldRefresh()
                                                   : weatherClient.shouldRefresh();
    if (shouldFetch) {
      if (WiFi.status() != WL_CONNECTED) {
        // connectWiFi() bloquea hasta 20s: sin este throttle, con el WiFi
        // caido el loop reintentaria en cada vuelta y la pantalla quedaria
        // congelada. fetchForCurrentMode() no actualiza lastFetch si no hay
        // red, asi que lo movemos nosotros.
        if (millis() - lastFetch >= API_RETRY_MS) {
          lastFetch = millis();
          connectWiFi();
        }
      } else {
        fetchForCurrentMode();
        renderCurrentMode();
      }
    }
    delay(30);
    return;
  }

  // En Radar o Mapa: tocar un avión abre su ficha de detalle
  if (tapped && (currentMode == Mode::Radar || currentMode == Mode::Map)) {
    const AircraftState* hit = (currentMode == Mode::Radar)
                                 ? radarScreen.hitTest(tx, ty)
                                 : mapScreen.hitTest(tx, ty);
    if (hit) {
      selectedAircraft = *hit; // copia: sobrevive al próximo fetch
      detailReturnMode = currentMode;
      currentMode = Mode::Detail;
      renderCurrentMode();
      delay(30);
      return;
    }
  }

  // En Aeropuertos: tocar el botón inferior rota al siguiente aeropuerto
  if (tapped && currentMode == Mode::Airports && airportScreen.nextButtonRect().contains(tx, ty)) {
    currentAirportIdx = (currentAirportIdx + 1) % AIRPORT_COUNT;
    lastFetch = 0;
    fetchForCurrentMode();
    renderCurrentMode();
    delay(30);
    return;
  }

  // Refresco adaptativo: más rápido si hay tráfico cerca de casa (solo en modo Radar).
  // El mapa provincial se mueve poco a esa escala, así que refresca más lento.
  fastMode = (currentMode == Mode::Radar) && RadarScreen::hasNearbyTraffic(aircraft);
  uint32_t interval;
  if (currentMode == Mode::Map)   interval = REFRESH_MAP_MS;
  else if (fastMode)              interval = REFRESH_FAST_MS;
  else                            interval = REFRESH_NORMAL_MS;

  if (millis() - lastFetch >= interval) {
    if (WiFi.status() != WL_CONNECTED) {
      connectWiFi();
    }
    fetchForCurrentMode();
    renderCurrentMode();
  }

  // Barrido del radar: gira solo, sin depender del ciclo de fetch. tick()
  // vuelve enseguida si todavía no toca frame, así que no le roba latencia
  // al polling del touch.
  if (currentMode == Mode::Radar) {
    radarScreen.tick();
    delay(RADAR_LOOP_DELAY_MS);
    return;
  }

  delay(30); // polling suave del touch
}
