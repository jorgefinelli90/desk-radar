#include "WebPortal.h"
#include "config.h"
#include <WiFi.h>
#include <ESPmDNS.h>

static const byte DNS_PORT = 53;

// --- Estilo compartido -----------------------------------------------------
// Un solo shell para el portal cautivo y el dashboard: el formulario de
// configuración es literalmente el mismo en los dos, y así no hay dos diseños
// que mantener.
static const char* PAGE_CSS =
  "*{box-sizing:border-box}"
  "body{margin:0;padding:16px;background:#12151a;color:#e6e9ef;"
  "font:15px/1.5 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}"
  ".w{max-width:520px;margin:0 auto}"
  "h1{font-size:20px;margin:0 0 4px}"
  ".sub{color:#8b94a3;font-size:13px;margin-bottom:20px}"
  "nav{display:flex;gap:8px;margin-bottom:20px;flex-wrap:wrap}"
  "nav a{padding:7px 12px;border-radius:8px;background:#1c212b;color:#c8d0dc;"
  "text-decoration:none;font-size:13px}"
  "nav a:hover{background:#273040}"
  ".card{background:#1a1f28;border:1px solid #262d3a;border-radius:12px;"
  "padding:16px;margin-bottom:14px}"
  "label{display:block;font-size:13px;color:#c8d0dc;margin:14px 0 5px}"
  "label:first-child{margin-top:0}"
  "input,textarea{width:100%;padding:10px 12px;border-radius:8px;"
  "border:1px solid #303849;background:#0f1319;color:#e6e9ef;font-size:15px;"
  "font-family:inherit}"
  "input:focus,textarea:focus{outline:none;border-color:#4a9eff}"
  ".hint{font-size:12px;color:#6f7889;margin-top:4px}"
  "button{width:100%;margin-top:18px;padding:12px;border:0;border-radius:8px;"
  "background:#2f7ddb;color:#fff;font-size:15px;font-weight:600;cursor:pointer}"
  "button:hover{background:#3b8ceb}"
  "button.danger{background:#c0392b}"
  "table{width:100%;border-collapse:collapse;font-size:14px}"
  "td{padding:7px 0;border-bottom:1px solid #232a36}"
  "td:first-child{color:#8b94a3;width:45%}"
  "tr:last-child td{border-bottom:0}"
  ".ok{color:#4ade80}.bad{color:#f87171}"
  "#toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%) translateY(80px);"
  "background:#1f8a4c;color:#fff;padding:11px 20px;border-radius:8px;"
  "opacity:0;transition:.25s;font-size:14px}"
  "#toast.on{opacity:1;transform:translateX(-50%) translateY(0)}";

// Escapa lo que va dentro de un atributo HTML. Sin esto, una contraseña con
// comillas rompe el formulario y encima abre la puerta a inyectar markup.
static String esc(const String& s) {
  String out;
  out.reserve(s.length() + 8);
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '&':  out += "&amp;";  break;
      case '<':  out += "&lt;";   break;
      case '>':  out += "&gt;";   break;
      case '"':  out += "&quot;"; break;
      case '\'': out += "&#39;";  break;
      default:   out += c;
    }
  }
  return out;
}

String WebPortal::pageShell(const String& title, const String& body) {
  String h;
  h.reserve(body.length() + 1600);
  h += "<!DOCTYPE html><html lang=\"es\"><head><meta charset=\"utf-8\">";
  h += "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
  h += "<title>desk-radar</title><style>";
  h += PAGE_CSS;
  h += "</style></head><body><div class=\"w\">";
  h += "<h1>desk-radar</h1><div class=\"sub\">" + title + "</div>";
  if (!_apMode) {
    h += "<nav><a href=\"/\">Estado</a><a href=\"/config\">Configuracion</a>"
         "<a href=\"/message\">Mensaje</a></nav>";
  }
  h += body;
  h += "</div></body></html>";
  return h;
}

// El formulario sale entero de la tabla de campos de DeviceConfig: agregar una
// API key nueva no requiere tocar este HTML.
String WebPortal::configFormHtml(const char* action, const char* submitLabel) {
  String h = "<form method=\"POST\" action=\"";
  h += action;
  h += "\"><div class=\"card\">";

  for (int i = 0; i < DeviceConfig::fieldCount(); i++) {
    const DeviceConfig::Field& f = DeviceConfig::field(i);
    const String& val = _cfg.get(f.name);

    h += "<label>";
    h += f.label;
    h += "</label><input name=\"";
    h += f.name;
    h += "\"";

    if (f.secret) {
      // No mandamos el secreto al navegador. Si ya hay uno guardado, se avisa
      // con el placeholder y dejarlo vacío significa "no lo cambies".
      h += " type=\"password\" placeholder=\"";
      h += val.length() ? "(guardado - dejar vacio para no cambiar)" : "(vacio)";
      h += "\"";
    } else {
      h += " value=\"" + esc(val) + "\"";
    }
    h += " autocomplete=\"off\">";

    if (f.hint && f.hint[0]) {
      h += "<div class=\"hint\">";
      h += f.hint;
      h += "</div>";
    }
  }

  h += "</div><button type=\"submit\">";
  h += submitLabel;
  h += "</button></form>";
  return h;
}

String WebPortal::uptimeText() const {
  uint32_t s = millis() / 1000;
  char buf[32];
  if (s < 3600) snprintf(buf, sizeof(buf), "%u min %u s", (unsigned)(s / 60), (unsigned)(s % 60));
  else          snprintf(buf, sizeof(buf), "%u h %u min", (unsigned)(s / 3600), (unsigned)((s % 3600) / 60));
  return String(buf);
}

// --- Rutas -----------------------------------------------------------------
void WebPortal::handleRoot() {
  if (_apMode) {
    // En modo AP la raíz ES el formulario: el celular abre el portal cautivo y
    // tiene que caer directo en lo que hay que completar.
    String body =
      "<div class=\"card\"><b>Primera configuracion</b><div class=\"hint\" "
      "style=\"margin-top:6px\">Cargá los datos de tu red WiFi y las claves de "
      "las APIs. El dispositivo se reinicia y se conecta solo.</div></div>";
    body += configFormHtml("/config", "Guardar y reiniciar");
    _server.send(200, "text/html", pageShell("Configuracion inicial", body));
    return;
  }

  String body = "<div class=\"card\"><table>";
  body += "<tr><td>Red WiFi</td><td>" + esc(WiFi.SSID()) + "</td></tr>";
  body += "<tr><td>IP local</td><td>" + WiFi.localIP().toString() + "</td></tr>";
  body += "<tr><td>Direccion</td><td>http://" + _hostname + ".local/</td></tr>";
  body += "<tr><td>Senal</td><td>" + String(WiFi.RSSI()) + " dBm</td></tr>";
  body += "<tr><td>Encendido hace</td><td>" + uptimeText() + "</td></tr>";

  body += "<tr><td>Ultimo fetch OpenSky</td><td>";
  if (_lastFetchOkMs == 0) {
    body += "<span class=\"bad\">todavia ninguno</span>";
  } else {
    uint32_t age = (millis() - _lastFetchOkMs) / 1000;
    body += "<span class=\"ok\">hace " + String(age) + " s</span>";
  }
  body += "</td></tr>";

  body += "<tr><td>Heap libre</td><td>" + String(ESP.getFreeHeap() / 1024) + " KB</td></tr>";
  body += "</table></div>";

  _server.send(200, "text/html", pageShell("Estado del dispositivo", body));
}

void WebPortal::handleConfigForm() {
  String body =
    "<div class=\"card\"><div class=\"hint\">Los campos de tipo contrasena se "
    "guardan pero no se muestran. Dejalos vacios para no cambiarlos.</div></div>";
  body += configFormHtml("/config", "Guardar");
  _server.send(200, "text/html", pageShell("Configuracion", body));
}

void WebPortal::handleConfigSave() {
  bool wifiChanged = false;

  for (int i = 0; i < DeviceConfig::fieldCount(); i++) {
    const DeviceConfig::Field& f = DeviceConfig::field(i);
    if (!_server.hasArg(f.name)) continue;

    String v = _server.arg(f.name);
    v.trim();

    // Un secreto vacío significa "dejalo como está", no "borralo": el navegador
    // nunca recibió el valor, así que un submit sin tocarlo llega vacío.
    if (f.secret && v.length() == 0) continue;

    if (v != _cfg.get(f.name)) {
      if (f.wifi) wifiChanged = true;
      _cfg.set(f.name, v);
    }
  }

  _cfg.save();

  bool reboot = _apMode || wifiChanged;
  String body = "<div class=\"card\"><b class=\"ok\">Guardado</b><div class=\"hint\" "
                "style=\"margin-top:8px\">";
  if (reboot) {
    body += "Cambiaron los datos de WiFi, asi que el dispositivo se esta "
            "reiniciando para conectarse. Volve a entrar en unos segundos.";
  } else {
    body += "Los cambios ya estan activos. No hace falta reiniciar.";
  }
  body += "</div></div>";
  if (!reboot) body += "<div class=\"card\"><a href=\"/\">Volver al estado</a></div>";

  _server.send(200, "text/html", pageShell("Configuracion", body));

  if (reboot) {
    delay(1200); // que alcance a irse la respuesta antes del reset
    ESP.restart();
  }
}

void WebPortal::handleMessageForm() {
  String body =
    "<form id=\"f\"><div class=\"card\">"
    "<label>Mensaje para la pantalla</label>"
    "<textarea name=\"text\" id=\"t\" rows=\"3\" maxlength=\"160\" "
    "placeholder=\"Escribi algo...\"></textarea>"
    "<div class=\"hint\">Aparece como un banner animado encima de lo que este "
    "mostrando el dispositivo, durante unos segundos.</div>"
    "</div><button type=\"submit\">Enviar mensaje al dispositivo</button></form>"
    "<div id=\"toast\">Enviado</div>"
    "<script>"
    "var f=document.getElementById('f'),t=document.getElementById('t'),"
    "k=document.getElementById('toast');"
    "f.onsubmit=function(e){e.preventDefault();"
    "if(!t.value.trim())return;"
    "fetch('/api/message',{method:'POST',"
    "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
    "body:'text='+encodeURIComponent(t.value)})"
    ".then(function(r){return r.json()})"
    ".then(function(){t.value='';k.textContent='Enviado';k.className='on';"
    "setTimeout(function(){k.className=''},1800)})"
    ".catch(function(){k.textContent='Error al enviar';k.className='on';"
    "setTimeout(function(){k.className=''},2500)});};"
    "</script>";
  _server.send(200, "text/html", pageShell("Enviar un mensaje", body));
}

void WebPortal::handleMessagePost() {
  String text;
  if (_server.hasArg("text"))      text = _server.arg("text");
  else if (_server.hasArg("plain")) text = _server.arg("plain"); // curl --data-binary

  text.trim();
  if (text.length() == 0) {
    _server.send(400, "application/json", "{\"ok\":false,\"error\":\"texto vacio\"}");
    return;
  }
  if (text.length() > 160) text = text.substring(0, 160);

  // Este handler corre en el hilo del loop (lo llama handleClient desde tick),
  // así que asignar acá y leer desde el loop no necesita sincronización.
  _pendingMessage = text;
  _hasMessage = true;

  Serial.printf("[Web] Mensaje recibido: %s\n", text.c_str());
  _server.send(200, "application/json", "{\"ok\":true}");
}

bool WebPortal::takeMessage(String& out) {
  if (!_hasMessage) return false;
  out = _pendingMessage;
  _hasMessage = false;
  return true;
}

void WebPortal::registerRoutes() {
  _server.on("/", HTTP_GET, [this]() { handleRoot(); });
  _server.on("/config", HTTP_GET, [this]() { handleConfigForm(); });
  _server.on("/config", HTTP_POST, [this]() { handleConfigSave(); });
  _server.on("/message", HTTP_GET, [this]() { handleMessageForm(); });
  _server.on("/api/message", HTTP_POST, [this]() { handleMessagePost(); });
}

// Android, iOS y Windows piden URLs conocidas para saber si la red tiene
// internet. Si respondemos con un redirect en vez de lo que esperan, el sistema
// concluye que hay un portal cautivo y abre el navegador solo. Sin esto hay que
// escribir 192.168.4.1 a mano.
void WebPortal::registerCaptiveDetection() {
  auto redirect = [this]() {
    _server.sendHeader("Location", "http://192.168.4.1/", true);
    _server.send(302, "text/plain", "");
  };
  _server.on("/generate_204", redirect);      // Android
  _server.on("/gen_204", redirect);           // Android
  _server.on("/hotspot-detect.html", redirect); // iOS / macOS
  _server.on("/library/test/success.html", redirect); // iOS
  _server.on("/connecttest.txt", redirect);   // Windows
  _server.on("/ncsi.txt", redirect);          // Windows
  _server.on("/fwlink", redirect);            // Windows
  _server.onNotFound(redirect);
}

// --- Modo AP ---------------------------------------------------------------
void WebPortal::showApScreen(const String& ssid, const String& ip) {
  TFT_eSPI& tft = _display.tft();
  tft.fillScreen(TFT_BLACK);

  tft.fillRect(0, 0, tft.width(), 30, TFT_NAVY);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString("CONFIGURACION", tft.width() / 2, 15, 2);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_SILVER, TFT_BLACK);
  tft.drawString("Conectate con el celular", tft.width() / 2, 56, 2);
  tft.drawString("a esta red WiFi:", tft.width() / 2, 76, 2);

  tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  tft.drawString(ssid, tft.width() / 2, 108, 4);

  if (strlen(SETUP_AP_PASSWORD) > 0) {
    tft.setTextColor(TFT_SILVER, TFT_BLACK);
    tft.drawString("Clave:", tft.width() / 2, 140, 2);
    tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    tft.drawString(SETUP_AP_PASSWORD, tft.width() / 2, 162, 4);
  }

  tft.drawFastHLine(20, 190, tft.width() - 40, TFT_DARKGREEN);

  tft.setTextColor(TFT_SILVER, TFT_BLACK);
  tft.drawString("Se abre solo el navegador.", tft.width() / 2, 210, 2);
  tft.drawString("Si no, entra a:", tft.width() / 2, 232, 2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(ip, tft.width() / 2, 258, 4);

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Esperando configuracion...", tft.width() / 2, tft.height() - 8, 1);
}

void WebPortal::runSetupPortal() {
  _apMode = true;

  WiFi.mode(WIFI_AP);
  const char* pass = strlen(SETUP_AP_PASSWORD) > 0 ? SETUP_AP_PASSWORD : nullptr;
  WiFi.softAP(SETUP_AP_SSID, pass);
  delay(300); // la IP del AP tarda un instante en quedar lista

  IPAddress ip = WiFi.softAPIP();
  Serial.printf("[Web] Modo AP: SSID '%s', IP %s\n", SETUP_AP_SSID, ip.toString().c_str());

  // DNS comodín: cualquier dominio resuelve al ESP32. Eso es lo que hace que
  // sea un portal cautivo de verdad y no una IP que hay que escribir a mano.
  _dns.setErrorReplyCode(DNSReplyCode::NoError);
  _dns.start(DNS_PORT, "*", ip);

  registerRoutes();
  registerCaptiveDetection();
  _server.enableDelay(false);
  _server.begin();

  showApScreen(SETUP_AP_SSID, ip.toString());

  // No vuelve: se sale por el ESP.restart() de handleConfigSave().
  while (true) {
    _dns.processNextRequest();
    _server.handleClient();
    delay(2);
  }
}

// --- Modo dashboard --------------------------------------------------------
bool WebPortal::startDashboard() {
  _apMode = false;
  _hostname = DEVICE_HOSTNAME;

  bool mdnsOk = MDNS.begin(_hostname.c_str());
  if (mdnsOk) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[Web] mDNS activo: http://%s.local/\n", _hostname.c_str());
  } else {
    Serial.println("[Web] mDNS fallo: entra por IP");
  }

  registerRoutes();
  // Sin portal cautivo acá: un 404 tiene que ser un 404, no un redirect.
  _server.onNotFound([this]() {
    _server.send(404, "text/plain", "No existe. Proba /, /config o /message");
  });
  _server.enableDelay(false); // si no, mete un delay(1) en cada vuelta del loop
  _server.begin();
  _dashboardUp = true;

  Serial.printf("[Web] Dashboard en http://%s/\n", WiFi.localIP().toString().c_str());
  return mdnsOk;
}

void WebPortal::tick() {
  if (!_dashboardUp) return;
  _server.handleClient();
}
