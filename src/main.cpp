#include <Arduino.h>
#include <WiFi.h>
#include <vector>

#include "config.h"
#include "DeviceConfig.h"  // credenciales en NVS (reemplaza a secrets.h)
#include "WebPortal.h"
#include "Banner.h"
#include "SettingsScreen.h"
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

enum class Mode { Home, Radar, Airports, Map, Detail, InfoMenu, News, Weather, Settings };

DisplayManager display;
TouchManager   touch(display.tft());
OpenSkyClient  opensky;      // credenciales desde NVS, ya no del compilador
NewsClient     newsClient;   // idem
WeatherClient  weatherClient;
WebPortal      webPortal(display, deviceConfig);
Banner         banner(display);
SettingsScreen settingsScreen(display);
// mapTiles va antes que las pantallas que lo usan: guardan una referencia, y el
// orden de declaración es el orden de construcción dentro de la misma unidad.
MapTiles       mapTiles(display);
HomeScreen     homeScreen(display);
RadarScreen    radarScreen(display, mapTiles);
AirportScreen  airportScreen(display);
MapScreen      mapScreen(display, mapTiles);
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
  String ssid = deviceConfig.get("ssid");
  String pass = deviceConfig.get("pass");
  if (ssid.length() == 0) return; // sin config no hay a qué conectarse

  display.showMessage("Conectando a " + ssid + "...");
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_HOSTNAME);
  WiFi.begin(ssid.c_str(), pass.c_str());

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

  if (currentMode == Mode::Radar || currentMode == Mode::Map) {
    // Mismo recuadro para las dos: miran la misma zona (ver HOME_FETCH_RADIUS_KM)
    auto box = GeoUtils::boundingBox(HOME_LAT, HOME_LON, HOME_FETCH_RADIUS_KM);
    if (opensky.fetchStates(box, aircraft)) {
      enrichWithGeo(aircraft, HOME_LAT, HOME_LON);
      webPortal.noteFetchOk();
    }
  } else if (currentMode == Mode::Airports) {
    const AirportDef& ap = AIRPORTS[currentAirportIdx];
    auto box = GeoUtils::boundingBox(ap.lat, ap.lon, ap.boxRadiusKm);
    if (opensky.fetchStates(box, aircraft)) {
      enrichWithGeo(aircraft, HOME_LAT, HOME_LON); // distancia mostrada siempre relativa a casa
      webPortal.noteFetchOk();
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
  } else if (currentMode == Mode::Settings) {
    settingsScreen.render(WiFi.SSID(), WiFi.localIP().toString(),
                          webPortal.hostname());
  } else {
    airportScreen.render(AIRPORTS[currentAirportIdx], aircraft, fastMode);
  }
}

void goHome() {
  currentMode = Mode::Home;
  renderCurrentMode();
}

// true si las dos pantallas se alimentan del mismo fetch
static bool sharesAircraftData(Mode a, Mode b) {
  auto usesHomeBox = [](Mode m) { return m == Mode::Radar || m == Mode::Map; };
  return usesHomeBox(a) && usesHomeBox(b);
}

// Cartel a mostrar mientras corre el fetch de entrada, o nullptr si esa
// pantalla no va a hacerte esperar (no usa red, no hay red, o su cache sigue
// vigente y refresh() va a volver enseguida).
static const char* pendingFetchMessage(Mode m) {
  if (WiFi.status() != WL_CONNECTED) return nullptr;
  switch (m) {
    case Mode::Radar:
    case Mode::Map:      return "Buscando aviones...";
    case Mode::Airports: return "Consultando el aeropuerto...";
    case Mode::News:     return newsClient.shouldRefresh() ? "Buscando titulares..." : nullptr;
    case Mode::Weather:  return weatherClient.shouldRefresh() ? "Consultando el clima..." : nullptr;
    default:             return nullptr; // Home, Detail, InfoMenu, Settings: sin red de por medio
  }
}

void enterMode(Mode m) {
  Mode previous = currentMode;
  currentMode = m;

  // Radar y Mapa comparten el recuadro: pasar de uno al otro reusa los aviones
  // que ya tenemos en vez de gastar otra llamada a OpenSky y hacerte esperar.
  const bool reusesData = sharesAircraftData(previous, m);
  if (!reusesData) {
    lastFetch = 0; // fuerza un fetch inmediato al entrar
  }

  if (m == Mode::Radar)         radarScreen.onEnter(); // limpia y reinicia el barrido
  else if (m == Mode::Map)      mapScreen.onEnter();   // fuerza el redibujo del mapa
  else if (m == Mode::Settings) settingsScreen.onEnter();

  if (!reusesData) {
    // El fetch bloquea varios segundos. Sin este cartel la pantalla anterior
    // queda congelada tal cual estaba y el toque parece no haber entrado.
    const char* waiting = pendingFetchMessage(m);
    if (waiting) display.showMessage(waiting);
    fetchForCurrentMode();
  }

  renderCurrentMode();
}

void setup() {
  Serial.begin(115200);
  display.begin();
  touch.begin();      // corre el wizard de calibración la primera vez
  deviceConfig.begin(); // credenciales desde NVS
  mapTiles.begin();   // monta LittleFS con los mapas pre-renderizados

  // Sin WiFi configurado no hay nada que mostrar: se levanta el Access Point y
  // el portal cautivo, y no se vuelve de acá (el ESP32 se reinicia cuando el
  // usuario guarda la configuración desde el navegador).
  if (FORCE_SETUP_PORTAL || !deviceConfig.hasWifi()) {
    Serial.println(FORCE_SETUP_PORTAL
                     ? "[Setup] FORCE_SETUP_PORTAL=1: arranco el portal a proposito"
                     : "[Setup] Sin WiFi configurado: arranco el portal");
    webPortal.runSetupPortal();
  }

  connectWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    webPortal.startDashboard();
  } else {
    // Nos quedamos en modo cliente igual: la red puede volver sola y el loop
    // reintenta. Entrar al portal por un corte de WiFi sería peor: perderíamos
    // la configuración buena por un problema temporal.
    Serial.println("[Setup] No conecto ahora. Reintenta solo; para reconfigurar,"
                   " usa AJUSTES en la pantalla.");
  }

  renderCurrentMode(); // Home
}

void loop() {
  // Servidor web. handleClient() es una máquina de estados: si no hay cliente
  // vuelve enseguida, así que no le roba latencia al touch.
  webPortal.tick();

  // ¿Llegó un mensaje por POST /api/message? El handler corre en este mismo
  // hilo (lo llama webPortal.tick()), así que no hace falta cola ni mutex.
  String incoming;
  if (webPortal.takeMessage(incoming)) {
    banner.show(incoming);
  }

  // El banner se dibuja encima de lo que haya. Mientras está activo se sigue
  // atendiendo el touch, pero no se procesan toques: son casi siempre el
  // reflejo de querer sacarlo de encima, no de navegar.
  if (banner.active()) {
    banner.tick();
    uint16_t bx = 0, by = 0;
    touch.getTap(bx, by); // consumir el tap para que no dispare después
    delay(5);
    return;
  }

  // Terminó la animación: hay que repintar la pantalla que quedó abajo.
  if (banner.takeRepaintRequest()) {
    if (currentMode == Mode::Radar)    radarScreen.onEnter();
    else if (currentMode == Mode::Map) mapScreen.onEnter();
    renderCurrentMode();
  }

  uint16_t tx = 0, ty = 0;
  bool tapped = touch.getTap(tx, ty);

  // Ajustes: muestra red/IP, recalibra el touch y permite borrar la config de
  // WiFi con dos toques.
  if (currentMode == Mode::Settings) {
    if (tapped) {
      if (ty < DisplayManager::STATUS_BAR_HEIGHT) {
        goHome();
      } else {
        SettingsAction action = settingsScreen.handleTap(tx, ty);
        if (action == SettingsAction::ResetWifi) {
          // Segundo toque confirmado: borrar y volver al portal
          display.showMessage("Borrando WiFi...");
          deviceConfig.clearWifi();
          delay(700);
          ESP.restart();
        } else if (action == SettingsAction::Recalibrate) {
          touch.recalibrate();  // bloquea hasta que toques las 3 cruces
          renderCurrentMode();  // el wizard piso la pantalla entera
        }
      }
    }
    settingsScreen.tick(); // vence la confirmación si el usuario se fue
    delay(30);
    return;
  }

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
      } else if (choice == HomeChoice::Settings) {
        enterMode(Mode::Settings);
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
      // Volvemos desde el Detail: la pantalla de origen quedó tapada por la
      // ficha, así que hay que repintarla entera.
      if (currentMode == Mode::Radar)    radarScreen.onEnter();
      else if (currentMode == Mode::Map) mapScreen.onEnter();
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

  // En Mapa: el botón inferior alterna entre 80 km y 40 km. No pide datos
  // nuevos: los dos zooms son recortes de la misma zona ya descargada.
  if (tapped && currentMode == Mode::Map && mapScreen.zoomButtonRect().contains(tx, ty)) {
    mapScreen.toggleZoom();
    renderCurrentMode();
    delay(30);
    return;
  }

  // En Aeropuertos: tocar el botón inferior rota al siguiente aeropuerto
  if (tapped && currentMode == Mode::Airports && airportScreen.nextButtonRect().contains(tx, ty)) {
    currentAirportIdx = (currentAirportIdx + 1) % AIRPORT_COUNT;
    lastFetch = 0;
    // Mismo motivo que en enterMode(): el fetch bloquea y sin cartel el boton
    // parece no haber respondido.
    const char* waiting = pendingFetchMessage(Mode::Airports);
    if (waiting) display.showMessage(waiting);
    fetchForCurrentMode();
    renderCurrentMode();
    delay(30);
    return;
  }

  // Refresco adaptativo: más rápido si hay tráfico cerca de casa. Vale para
  // Radar y Mapa por igual, que ahora miran la misma zona.
  fastMode = (currentMode == Mode::Radar || currentMode == Mode::Map) &&
             RadarScreen::hasNearbyTraffic(aircraft);
  uint32_t interval = fastMode ? REFRESH_FAST_MS : REFRESH_NORMAL_MS;

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
