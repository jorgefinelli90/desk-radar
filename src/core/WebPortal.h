#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "core/DeviceConfig.h"
#include "core/DisplayManager.h"

// Portal de configuración y dashboard local.
//
// Dos modos, un solo servidor:
//   - SETUP: sin WiFi guardado. Levanta un Access Point y un portal cautivo
//     (DNS que resuelve todo a la IP del AP), para configurar desde el celular.
//   - DASHBOARD: ya conectado a la red real. Sirve el estado, la config y el
//     envío de mensajes en http://desk-radar.local/
//
// Se usa el WebServer sincrónico del core y no ESPAsyncWebServer a propósito:
// WebServer::handleClient() es una máquina de estados, no bloquea (si no hay
// cliente vuelve enseguida; si el cliente todavía no mandó datos, lo guarda y
// retorna). Eso alcanza para el loop de 30 ms, evita dos dependencias más y
// -sobre todo- hace que los handlers corran en el MISMO hilo que el loop, así
// que pasar el mensaje a la pantalla no necesita ni cola ni mutex.
class WebPortal {
  public:
    WebPortal(DisplayManager& display, DeviceConfig& cfg)
      : _display(display), _cfg(cfg) {}

    // Modo AP + portal cautivo. No vuelve: el ESP32 se reinicia cuando el
    // usuario guarda la configuración. Se llama desde setup(), no del loop.
    void runSetupPortal();

    // Modo dashboard. Arranca mDNS y el servidor. Devuelve false si mDNS falló
    // (el servidor igual queda andando, se accede por IP).
    bool startDashboard();

    // Llamar en cada vuelta del loop. No bloquea.
    void tick();

    // Sonda opcional para saber si la tarea de red esta ocupada. Se usa antes
    // de guardar la configuracion: OpenSkyClient relee las credenciales de NVS
    // en cada pedido de token, asi que reescribirlas en mitad de un fetch le
    // cambiaria el String abajo de los pies. Un lambda sin captura convierte
    // solo a este tipo.
    typedef bool (*BusyProbe)();
    void setNetBusyProbe(BusyProbe probe) { _busyProbe = probe; }

    // true si startDashboard() ya corrio. Lo mira wifiTick() en main: el
    // dashboard se levanta cuando aparece la IP, que puede ser al arrancar o
    // un rato despues, y no se puede levantar dos veces.
    bool dashboardUp() const { return _dashboardUp; }

    // Devuelve true (y llena out) si llegó un mensaje nuevo por POST.
    bool takeMessage(String& out);

    // Lo llama main cuando un fetch a OpenSky sale bien, para mostrarlo en el
    // dashboard.
    void noteFetchOk() { _lastFetchOkMs = millis(); }

    // Antiguedad del ultimo fetch bueno. El dato vive aca porque el dashboard
    // ya lo mostraba; desde UX-4 la barra de estado de la pantalla lo muestra
    // tambien, en vez del viejo indicador RAPIDO/NORMAL.
    bool     hasFetchOk() const { return _lastFetchOkMs != 0; }
    uint32_t fetchAgeMs() const { return millis() - _lastFetchOkMs; }

    const String& hostname() const { return _hostname; }

  private:
    DisplayManager& _display;
    DeviceConfig&   _cfg;
    WebServer       _server{80};
    DNSServer       _dns;

    BusyProbe _busyProbe = nullptr;

    bool     _apMode = false;
    bool     _dashboardUp = false;
    String   _hostname;
    uint32_t _lastFetchOkMs = 0;

    // Buzón de un solo mensaje. No hace falta protección: el handler corre en
    // el hilo del loop (ver comentario de arriba).
    String _pendingMessage;
    bool   _hasMessage = false;

    void registerRoutes();
    void registerCaptiveDetection();

    // Puerta de entrada de todos los handlers del dashboard. Devuelve true si la
    // request puede seguir; si devuelve false ya mando el 401 y el handler tiene
    // que salir sin escribir nada mas.
    bool requireAuth();

    void handleRoot();
    void handleConfigForm();
    void handleConfigSave();
    void handleMessageForm();
    void handleMessagePost();

    // --- Actualizacion por WiFi (OTA) ---
    // El POST tiene dos handlers: uno que corre por cada trozo del .bin que va
    // llegando (handleUpdateUpload) y otro al final, cuando ya se recibio todo
    // (handleUpdateDone). Es como WebServer maneja las subidas de archivos.
    void handleUpdateForm();
    void handleUpdateUpload();
    void handleUpdateDone();

    // La autenticacion del OTA no puede contestar en medio del upload: cuando
    // se rechaza se anota aca y el 401 lo manda el handler final.
    bool   _otaDenied = false;
    String _otaError;

    String pageShell(const String& title, const String& body);
    String configFormHtml(const char* action, const char* submitLabel);
    String uptimeText() const;

    void showApScreen(const String& ssid, const String& ip);
};
