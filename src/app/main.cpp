#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include <string.h>

#include "config.h"
#include "core/DeviceConfig.h"  // credenciales en NVS (reemplaza a secrets.h)
#include "core/WebPortal.h"
#include "core/Clock.h"
#include "core/DataLock.h"
#include "core/NetTask.h"
#include "core/Banner.h"
#include "screens/SettingsScreen.h"
#include "utils/GeoUtils.h"
#include "services/OpenSkyClient.h"
#include "core/DisplayManager.h"
#include "core/TouchManager.h"
#include "screens/HomeScreen.h"
#include "screens/RadarScreen.h"
#include "screens/AirportScreen.h"
#include "screens/MapScreen.h"
#include "screens/DetailScreen.h"
#include "screens/InfoMenuScreen.h"
#include "services/NewsClient.h"
#include "screens/NewsScreen.h"
#include "screens/NewsDetailScreen.h"
#include "services/WeatherClient.h"
#include "screens/WeatherScreen.h"

enum class Mode { Home, Radar, Airports, Map, Detail, InfoMenu, News, NewsDetail,
                  Weather, Settings };

DisplayManager display;
Clock          deviceClock;   // NTP; la barra de estado lo consulta via display
TouchManager   touch(display.tft());
OpenSkyClient  opensky;      // credenciales desde NVS, ya no del compilador
NewsClient     newsClient;   // idem
WeatherClient  weatherClient;
WebPortal      webPortal(display, deviceConfig, opensky);
// Toda la red vive en el core 0: el loop pide y sigue dibujando (ver NetTask.h)
NetTask        netTask(opensky, newsClient, weatherClient);
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
NewsDetailScreen newsDetailScreen(display);
WeatherScreen  weatherScreen(display);

Mode currentMode = Mode::Home;
Mode detailReturnMode = Mode::Radar; // a qué pantalla volver al salir del Detail
int  currentAirportIdx = 0;

// Copia del avión que se está viendo en modo Detail
AircraftState selectedAircraft;

// Ruta (origen/destino) de ese avión, si OpenSky la tiene. Llega asincronica
// via NetTask; se pide al entrar a Detail y se dibuja cuando llega (ver
// MODE_OPS y netTick()).
RouteInfo selectedRoute;

// Idem para la noticia abierta. Copia y no indice: la tarea de red puede
// republicar los titulares mientras estas leyendo uno, y el que estaba en la
// posicion 3 pasaria a ser otro.
NewsItem selectedNews;

std::vector<AircraftState> aircraft;
uint32_t lastFetch = 0;
bool fastMode = false;

// --- WiFi -------------------------------------------------------------------
// La reconexión la maneja el stack (setAutoReconnect) y el loop NUNCA se queda
// esperando la red. Antes había un solo connectWiFi() que bloqueaba hasta 20 s
// y el loop lo llamaba cada vez que se vencía el intervalo de refresco sin
// conexión: con el router caído, el aparato se congelaba 20 s de cada 30 y en
// ese rato no corría ni el touch, ni el barrido del radar, ni el servidor web.
bool wifiOnline = false;          // último estado que ya reportamos por serie
uint32_t lastWifiTryMs = 0;

// Lo setea el evento y lo consume wifiTick(). volatile porque lo escriben dos
// tareas distintas.
volatile bool wifiEventGotIp = false;

// OJO: esto corre en la tarea del event loop de Arduino, NO en el loop
// principal. Acá solo se toca una bandera: levantar el servidor web o dibujar
// en la pantalla desde este contexto (otro stack, otra prioridad) es pedir
// problemas. De eso se encarga wifiTick(), que sí corre en el loop.
void onWiFiEvent(WiFiEvent_t event) {
  if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) wifiEventGotIp = true;
}

// Arranca el intento de conexión y vuelve enseguida. No espera nada.
void startWiFi() {
  if (!deviceConfig.hasWifi()) return; // sin config no hay a qué conectarse

  WiFi.persistent(false); // las credenciales ya viven en NVS via DeviceConfig:
                          // sin esto, cada begin() reescribe flash al pedo
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.begin(deviceConfig.get("ssid").c_str(), deviceConfig.get("pass").c_str());
  lastWifiTryMs = millis();
}

// Único lugar del firmware que espera la red, y solo se llama desde setup():
// hasta que no hay datos no hay ninguna pantalla útil que mostrar.
bool waitForWiFi(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(100);
  }
  return WiFi.status() == WL_CONNECTED;
}

// Se llama en cada vuelta del loop. No bloquea nunca.
void wifiTick() {
  if (wifiEventGotIp) {
    wifiEventGotIp = false;
    Serial.printf("[WiFi] Conectado a %s, IP %s\n",
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    // Recien ahora tiene sentido pedir la hora. Es idempotente, asi que no pasa
    // nada si la red se cae y vuelve varias veces.
    deviceClock.begin();
    // El dashboard se levanta acá y no en setup(): si al arrancar no había red,
    // antes no se levantaba nunca, aunque la red volviera un minuto después.
    if (!webPortal.dashboardUp()) webPortal.startDashboard();
  }

  bool online = (WiFi.status() == WL_CONNECTED);
  if (wifiOnline && !online) {
    Serial.println("[WiFi] Se cayo la red. Se reintenta solo, en segundo plano.");
  }
  wifiOnline = online;

  if (online || !deviceConfig.hasWifi()) return;
  if (millis() - lastWifiTryMs < WIFI_RETRY_MS) return;

  lastWifiTryMs = millis();
  WiFi.begin(deviceConfig.get("ssid").c_str(), deviceConfig.get("pass").c_str());
}

// Calcula distancia y bearing de cada avión respecto al punto de referencia dado
void enrichWithGeo(std::vector<AircraftState>& list, double refLat, double refLon) {
  for (auto& a : list) {
    a.distanceKm = GeoUtils::distanceKm(refLat, refLon, a.lat, a.lon);
    a.bearingDeg = GeoUtils::bearingDeg(refLat, refLon, a.lat, a.lon);
  }
}

void renderCurrentMode(); // definida mas abajo; netTick() la necesita antes

// Le PIDE a la tarea de red lo que necesita la pantalla actual y vuelve
// enseguida. Antes esta funcion hacia el fetch acá mismo y se llevaba puestos
// varios segundos del loop; ahora el trabajo pasa al core 0 y el resultado se
// recoge en netTick().
void fetchForCurrentMode() {
  if (WiFi.status() != WL_CONNECTED) {
    // Sin red no hay nada que pedir, pero el intento se marca igual: si no, el
    // loop volvería a entrar acá en cada vuelta y repintaría la pantalla entera
    // cada 30 ms hasta que volviera el WiFi.
    lastFetch = millis();
    return;
  }

  if (currentMode == Mode::Radar || currentMode == Mode::Map) {
    // Mismo recuadro para las dos: miran la misma zona (ver HOME_FETCH_RADIUS_KM)
    netTask.request(NetJob::Aircraft,
                    GeoUtils::boundingBox(HOME_LAT, HOME_LON, HOME_FETCH_RADIUS_KM));
  } else if (currentMode == Mode::Airports) {
    const AirportDef& ap = AIRPORTS[currentAirportIdx];
    netTask.request(NetJob::Aircraft,
                    GeoUtils::boundingBox(ap.lat, ap.lon, ap.boxRadiusKm));
  } else if (currentMode == Mode::News) {
    // Cada cliente decide solo si le toca pedir o si el cache sigue vigente
    if (newsClient.shouldRefresh()) netTask.request(NetJob::News);
  } else if (currentMode == Mode::Weather) {
    if (weatherClient.shouldRefresh()) netTask.request(NetJob::Weather);
  }

  lastFetch = millis();
}

// Recoge lo que la tarea de red haya dejado listo. No bloquea nunca: si no hay
// nada, las dos comprobaciones son la lectura de un bool.
void netTick() {
  std::vector<AircraftState> frescos;
  if (netTask.takeAircraft(frescos)) {
    aircraft.swap(frescos);
    // El enriquecido geométrico se hace acá y no en la tarea a propósito: es
    // cuenta pura sobre datos que ya son nuestros, y así la tarea suelta el
    // candado lo antes posible.
    enrichWithGeo(aircraft, HOME_LAT, HOME_LON); // siempre relativo a casa
    webPortal.noteFetchOk();

    // Solo si la pantalla de arriba muestra aviones. Los datos pueden llegar
    // cuando ya te fuiste a Noticias, y repintar por eso seria trabajo al pedo.
    if (currentMode == Mode::Radar || currentMode == Mode::Map ||
        currentMode == Mode::Airports) {
      renderCurrentMode();
    }
  }

  // Noticias o clima nuevos: los datos ya están publicados, solo falta dibujar.
  if (netTask.takeRefreshed()) {
    if (currentMode == Mode::News || currentMode == Mode::Weather) {
      renderCurrentMode();
    }
  }

  // Ruta de un avión: puede llegar cuando el usuario ya volvió al radar o pasó
  // a mirar otro avión. Se descarta si no es la que se pidió para el que se
  // está mirando ahora.
  RouteInfo route;
  if (netTask.takeRoute(route)) {
    if (strcmp(route.icao24, selectedAircraft.icao24) == 0) {
      selectedRoute = route;
      if (currentMode == Mode::Detail) renderCurrentMode();
    }
  }
}

// --- Frescura de los datos de OpenSky ---------------------------------------
// "5s", "45s", "2m", "1h".
static String formatAge(uint32_t ms) {
  uint32_t s = ms / 1000;
  char buf[8];
  if (s < 60)        snprintf(buf, sizeof(buf), "%us", (unsigned)s);
  else if (s < 3600) snprintf(buf, sizeof(buf), "%um", (unsigned)(s / 60));
  else               snprintf(buf, sizeof(buf), "%uh", (unsigned)(s / 3600));
  return String(buf);
}

// Lo que va a la derecha de la barra de estado, en lugar del viejo indicador
// RAPIDO/NORMAL. Saber que el radar sigue barriendo sobre datos de hace tres
// minutos importa mas que saber cada cuanto piensa refrescar: el barrido gira
// igual aunque no llegue nada, asi que sin esto una API caida se ve exactamente
// igual que todo funcionando. El modo rapido no se perdio, ahora lo dice el
// color.
static String dataStatusText() {
  if (!webPortal.hasFetchOk()) return String("sin datos");
  return String("hace ") + formatAge(webPortal.fetchAgeMs());
}

// Version corta para el Mapa, que ademas muestra el conteo de aviones y no
// tiene lugar para el "hace".
static String dataAgeShort() {
  if (!webPortal.hasFetchOk()) return String("--");
  return formatAge(webPortal.fetchAgeMs());
}

// Verde: refrescando rapido porque hay trafico cerca. Ambar: el ultimo fetch
// bueno quedo viejo (la API dejo de responder, o no hay red) y lo que se ve en
// pantalla ya no es de ahora. Plata: al dia, a ritmo normal.
static uint16_t dataStatusColor() {
  if (!webPortal.hasFetchOk())                 return TFT_ORANGE;
  if (webPortal.fetchAgeMs() >= STALE_DATA_MS) return TFT_ORANGE;
  return fastMode ? TFT_GREEN : TFT_SILVER;
}

// ---------------------------------------------------------------------------
//  Tabla de pantallas
//
//  Antes cada modo estaba repartido en cuatro cadenas de if/else distintas
//  -fetch, render, enterMode y el loop-, asi que agregar una pantalla obligaba
//  a tocar las cuatro, y olvidarse de una no daba error de compilacion: daba
//  una pantalla que no refrescaba, o que no respondia al toque.
//
//  Ahora cada modo es UNA fila de MODE_OPS. Agregar una pantalla es agregar una
//  fila; si falta un campo, no compila.
//
//  Los lambdas van sin captura a proposito: asi convierten a puntero de funcion
//  y la tabla queda en flash en vez de en RAM. Acceden a los objetos globales de
//  arriba, que es donde ya vivian.
// ---------------------------------------------------------------------------

// Abre la ficha de un avion tocado en el radar o el mapa (definida abajo: la
// tabla la necesita antes).
static Mode openDetailFrom(const AircraftState* hit);

// De que fuente se alimenta la pantalla. Decide el ciclo de refresco.
enum class DataNeed : uint8_t { None, Aircraft, News, Weather };

struct ModeOps {
  Mode     mode;
  DataNeed needs;
  bool     statusBarBack;  // tocar la barra de arriba vuelve a Home
  uint32_t loopDelayMs;

  void (*onEnter)();
  void (*onExit)();
  void (*render)();
  void (*tick)();

  // Devuelve a que modo ir. Para quedarse donde estamos, devuelve el mismo:
  // las acciones que no son navegacion (cambiar el zoom, rotar de aeropuerto)
  // se resuelven adentro y redibujan solas.
  Mode (*handleTap)(uint16_t x, uint16_t y);
};

static const ModeOps MODE_OPS[] = {
  { Mode::Home, DataNeed::None, false, 30,
    []{}, []{},
    []{ homeScreen.render(); },
    []{},
    [](uint16_t x, uint16_t y) {
      switch (homeScreen.hitTest(x, y)) {
        case HomeChoice::Radar:    return Mode::Radar;
        case HomeChoice::Airports: currentAirportIdx = 0; return Mode::Airports;
        case HomeChoice::Map:      return Mode::Map;
        case HomeChoice::Info:     return Mode::InfoMenu;
        case HomeChoice::Settings: return Mode::Settings;
        default:                   return Mode::Home;
      }
    } },

  { Mode::Radar, DataNeed::Aircraft, true, RADAR_LOOP_DELAY_MS,
    []{ radarScreen.onEnter(); },
    []{ radarScreen.onExit(); },   // suelta el sprite y el mapa: 40 KB (ARQ-7)
    []{ radarScreen.render(aircraft, dataStatusText(), dataStatusColor()); },
    []{ radarScreen.tick(); },
    [](uint16_t x, uint16_t y) { return openDetailFrom(radarScreen.hitTest(x, y)); } },

  { Mode::Map, DataNeed::Aircraft, true, 30,
    []{ mapScreen.onEnter(); }, []{},
    []{ mapScreen.render(aircraft, dataAgeShort(), dataStatusColor()); },
    []{},
    [](uint16_t x, uint16_t y) {
      // El boton de abajo alterna 80 km <-> 40 km. No pide datos nuevos: los dos
      // zooms son recortes de la misma zona que ya esta en RAM.
      if (mapScreen.zoomButtonRect().contains(x, y)) {
        mapScreen.toggleZoom();
        renderCurrentMode();
        return Mode::Map;
      }
      return openDetailFrom(mapScreen.hitTest(x, y));
    } },

  { Mode::Airports, DataNeed::Aircraft, true, 30,
    []{}, []{},
    []{ airportScreen.render(AIRPORTS[currentAirportIdx], aircraft,
                             dataStatusText(), dataStatusColor()); },
    []{},
    [](uint16_t x, uint16_t y) {
      if (airportScreen.nextButtonRect().contains(x, y)) {
        currentAirportIdx = (currentAirportIdx + 1) % AIRPORT_COUNT;
        lastFetch = 0;
        aircraft.clear(); // otro aeropuerto: los que hay son de otra zona
        fetchForCurrentMode();
        renderCurrentMode();
        return Mode::Airports;
      }
      // UX-2: era la unica de las tres pantallas de aviones donde tocar la
      // fila no hacia nada. Mismo patron que Radar y Mapa.
      return openDetailFrom(airportScreen.hitTest(x, y));
    } },

  // La ficha queda congelada para leerla tranquilo: no refresca sola (needs
  // None) y cualquier toque vuelve al origen, incluida la barra de arriba, por
  // eso statusBarBack va en false.
  { Mode::Detail, DataNeed::None, false, 30,
    []{
      // Ruta: enriquecido opcional, se pide al entrar. Se resetea antes de
      // pedir para no mostrar un instante la ruta del avion anterior mientras
      // llega la nueva.
      selectedRoute = RouteInfo{};
      netTask.requestRoute(selectedAircraft.icao24);
    },
    []{},
    []{ detailScreen.render(selectedAircraft, selectedRoute); },
    []{},
    [](uint16_t, uint16_t) { return detailReturnMode; } },

  { Mode::InfoMenu, DataNeed::None, true, 30,
    []{}, []{},
    []{ infoMenuScreen.render(); },
    []{},
    [](uint16_t x, uint16_t y) {
      switch (infoMenuScreen.hitTest(x, y)) {
        case InfoChoice::News:    return Mode::News;
        case InfoChoice::Weather: return Mode::Weather;
        default:                  return Mode::InfoMenu;
      }
    } },

  { Mode::News, DataNeed::News, true, 30,
    []{}, []{},
    []{
      // Bajo candado: la tarea de red puede estar publicando titulares nuevos
      // en el otro nucleo justo mientras los dibujamos. Es el dibujo de una
      // pantalla, milisegundos, no un fetch.
      DataLock::Guard g;
      newsScreen.render(newsClient);
    },
    []{},
    [](uint16_t x, uint16_t y) {
      // Bajo candado: se consulta la lista publicada y se copia el item.
      DataLock::Guard g;
      const int idx = newsScreen.hitTest(x, y);
      if (idx < 0 || idx >= (int)newsClient.items().size()) return Mode::News;
      selectedNews = newsClient.items()[idx];
      return Mode::NewsDetail;
    } },

  // La noticia abierta: congelada como la ficha del avion, y cualquier toque
  // vuelve a la lista.
  { Mode::NewsDetail, DataNeed::None, false, 30,
    []{}, []{},
    []{ newsDetailScreen.render(selectedNews); },
    []{},
    [](uint16_t, uint16_t) { return Mode::News; } },

  { Mode::Weather, DataNeed::Weather, true, 30,
    []{}, []{},
    []{ DataLock::Guard g; weatherScreen.render(weatherClient); },
    []{},
    [](uint16_t, uint16_t) { return Mode::Weather; } },

  { Mode::Settings, DataNeed::None, true, 30,
    []{ settingsScreen.onEnter(); }, []{},
    []{ settingsScreen.render(WiFi.SSID(), WiFi.localIP().toString(),
                              webPortal.hostname()); },
    []{ settingsScreen.tick(); },  // vence la confirmacion si el usuario se fue
    [](uint16_t x, uint16_t y) {
      SettingsAction action = settingsScreen.handleTap(x, y);
      if (action == SettingsAction::ResetWifi) {
        display.showMessage("Borrando WiFi...");
        deviceConfig.clearAccess();
        delay(700);
        ESP.restart();
      } else if (action == SettingsAction::Recalibrate) {
        touch.recalibrate();  // bloquea hasta que toques las 3 cruces
        renderCurrentMode();  // el wizard piso la pantalla entera
      }
      return Mode::Settings;
    } },
};

static const int MODE_COUNT = sizeof(MODE_OPS) / sizeof(MODE_OPS[0]);

// Si algun dia se agrega un modo al enum y no a la tabla, esto lo caza al
// compilar en vez de dejar una pantalla muda.
static_assert(MODE_COUNT == (int)Mode::Settings + 1,
              "Falta (o sobra) una fila en MODE_OPS: tiene que haber una por "
              "cada valor de Mode.");

static const ModeOps& opsFor(Mode m) {
  for (int i = 0; i < MODE_COUNT; i++) {
    if (MODE_OPS[i].mode == m) return MODE_OPS[i];
  }
  return MODE_OPS[0]; // inalcanzable: el static_assert garantiza la cobertura
}

void renderCurrentMode() {
  opsFor(currentMode).render();
}

// Abre la ficha de un avion tocado en el radar o el mapa. Devuelve el modo al
// que hay que ir: Detail si el toque acerto, o el actual si no.
static Mode openDetailFrom(const AircraftState* hit) {
  if (!hit) return currentMode;
  selectedAircraft = *hit;      // copia: sobrevive al proximo fetch
  detailReturnMode = currentMode;
  return Mode::Detail;
}

// true si las dos pantallas se alimentan del mismo fetch
static bool sharesAircraftData(Mode a, Mode b) {
  // El Detail es una ficha congelada sobre un avion que ya teniamos: entrar y
  // salir no cambia la zona que estamos mirando, asi que no tiene que descartar
  // la lista ni disparar un fetch nuevo.
  if (a == Mode::Detail || b == Mode::Detail) return true;

  auto usesHomeBox = [](Mode m) { return m == Mode::Radar || m == Mode::Map; };
  return usesHomeBox(a) && usesHomeBox(b);
}

void enterMode(Mode m) {
  Mode previous = currentMode;
  if (previous != m) opsFor(previous).onExit();

  currentMode = m;

  // Radar y Mapa comparten el recuadro: pasar de uno al otro reusa los aviones
  // que ya tenemos en vez de gastar otra llamada a OpenSky y hacerte esperar.
  const bool reusesData = sharesAircraftData(previous, m);
  if (!reusesData) {
    lastFetch = 0; // fuerza un fetch inmediato al entrar

    // Los aviones que tenemos en RAM son de otra zona (o de otra pantalla):
    // dibujarlos aca seria mentir, sobre todo en Aeropuertos, donde la lista no
    // filtra por area y quedarian los de casa como si estuvieran en Ezeiza.
    //
    // Esto NO es el out.clear() que causaba el bug de OpenSkyClient: alla la
    // lista se vaciaba por un error de red, aca se descarta porque cambio el
    // area que estamos mirando, que es un motivo legitimo.
    aircraft.clear();
  }

  opsFor(m).onEnter();

  if (!reusesData) {
    // Antes aca iba un cartel de "Buscando aviones...", porque el fetch se
    // llevaba puesto el loop varios segundos y sin el la pantalla anterior
    // quedaba congelada y el toque parecia no haber entrado. Con la red en el
    // core 0 la pantalla nueva se dibuja al instante y los datos entran cuando
    // llegan; que todavia no esten lo dice la barra de estado ("sin datos").
    fetchForCurrentMode();
  }

  renderCurrentMode();
}

// Ciclo de refresco de la pantalla activa, segun de que se alimente.
static void refreshDataFor(const ModeOps& ops) {
  if (ops.needs == DataNeed::Aircraft) {
    // Refresco adaptativo: mas rapido si hay trafico cerca de casa.
    fastMode = (currentMode == Mode::Radar || currentMode == Mode::Map) &&
               RadarScreen::hasNearbyTraffic(aircraft);
    const uint32_t interval = fastMode ? REFRESH_FAST_MS : REFRESH_NORMAL_MS;

    if (millis() - lastFetch >= interval) {
      // Sin red, fetchForCurrentMode() vuelve enseguida y reprograma el intento
      // para dentro de un intervalo. De reconectar se ocupa wifiTick().
      fetchForCurrentMode();
      renderCurrentMode();
    }
  } else if (ops.needs == DataNeed::News || ops.needs == DataNeed::Weather) {
    // Noticias y clima no siguen el refresco adaptativo: cada cliente tiene su
    // propio cache y su propio reintento (API_RETRY_MS). Se pide y nada mas; el
    // redibujo lo dispara netTick() cuando los datos llegan.
    const bool should = (ops.needs == DataNeed::News) ? newsClient.shouldRefresh()
                                                      : weatherClient.shouldRefresh();
    if (should && WiFi.status() == WL_CONNECTED && !netTask.busy()) {
      fetchForCurrentMode();
    }
  }
}
// UNIT_TEST la define PlatformIO solo al compilar para "pio test": el test
// runner pone su propio setup()/loop() (el que arranca UNITY_BEGIN), y sin este
// guard esta pareja chocaba con la de ahi por "multiple definition". El resto
// de este archivo -los objetos globales, la tabla MODE_OPS, los helpers- se
// sigue compilando igual: no declaran setup()/loop() y no generan conflicto.
#ifndef UNIT_TEST
void setup() {
  Serial.begin(115200);
  display.begin();
  display.setClock(&deviceClock); // la barra de estado muestra la hora

  DataLock::begin();  // antes que la tarea de red: protege lo que se comparte
  netTask.begin();

  // Guardar la configuracion mientras hay un fetch en curso le cambiaria las
  // credenciales a OpenSkyClient abajo de los pies, porque las relee de NVS en
  // cada pedido de token. El handler espera a que la red termine.
  webPortal.setNetBusyProbe([]() { return netTask.busy(); });
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

  WiFi.onEvent(onWiFiEvent);
  if (deviceConfig.hasWifi()) {
    display.showMessage("Conectando a " + deviceConfig.get("ssid") + "...");
    startWiFi();
    if (!waitForWiFi(WIFI_CONNECT_TIMEOUT_MS)) {
      // Nos quedamos en modo cliente igual: la red puede volver sola y
      // wifiTick() reintenta en segundo plano. Entrar al portal por un corte de
      // WiFi sería peor: perderíamos la configuración buena por un problema
      // temporal.
      display.showMessage("Sin WiFi. Reintentando...");
      Serial.println("[Setup] No conecto ahora. Reintenta solo; para reconfigurar,"
                     " usa AJUSTES en la pantalla.");
    }
  }
  // El dashboard NO se levanta acá: lo hace wifiTick() en cuanto aparece la IP,
  // sea ahora o dentro de un rato. Así el portal también aparece si la red se
  // hizo esperar más que WIFI_CONNECT_TIMEOUT_MS.

  renderCurrentMode(); // Home
}

void loop() {
  // Resultados de la tarea de red, si los hay. No bloquea.
  netTick();

  // Estado de la red: reconecta si hace falta y levanta el dashboard la primera
  // vez que hay IP. Va antes de todos los "return" de abajo para que siga
  // corriendo aunque el banner esté activo o estemos en una pantalla sin red.
  wifiTick();

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

  // Termino la animacion: hay que repintar la pantalla que quedo abajo.
  if (banner.takeRepaintRequest()) {
    opsFor(currentMode).onEnter();  // el radar reinicia el barrido, el mapa el fondo
    renderCurrentMode();
  }

  // Reloj de la barra: repinta solo cuando cambia el minuto, asi que sale casi
  // siempre por el primer if. Va despues del banner (que tapa la barra) y no se
  // llama en Home, que es la unica pantalla sin barra de estado.
  if (currentMode != Mode::Home) display.tickStatusClock();

  // --- Router -------------------------------------------------------------
  // Todo lo que antes eran siete ramas de if/else sale de la fila del modo
  // activo. Ver MODE_OPS.
  const ModeOps* ops = &opsFor(currentMode);

  uint16_t tx = 0, ty = 0;
  if (touch.getTap(tx, ty)) {
    // La barra de arriba es "volver" en todas las pantallas menos Home, que no
    // la tiene, y Detail, donde cualquier toque ya vuelve al origen.
    if (ops->statusBarBack && ty < DisplayManager::STATUS_BAR_HEIGHT) {
      enterMode(Mode::Home);
    } else {
      const Mode next = ops->handleTap(tx, ty);
      if (next != currentMode) enterMode(next);
    }

    // enterMode() puede haber cambiado de pantalla: lo que queda de la vuelta
    // le toca a la que quedo activa, no a la que recibio el toque.
    ops = &opsFor(currentMode);
  }

  ops->tick();          // animacion (el barrido del radar) o vencimientos
  refreshDataFor(*ops); // ciclo de refresco segun de que se alimente

  delay(ops->loopDelayMs);
}
#endif  // UNIT_TEST
