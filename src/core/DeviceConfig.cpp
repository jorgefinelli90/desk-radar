#include "core/DeviceConfig.h"
#include <Preferences.h>

// secrets.h es opcional: está en .gitignore, así que en un clon limpio no
// existe. Si está, se usa UNA SOLA VEZ para precargar NVS en el primer boot.
// El dispositivo final se configura entero desde el navegador.
#if defined(__has_include)
  #if __has_include("secrets.h")
    #include "secrets.h"
    #define DESKRADAR_HAS_SECRETS_H 1
  #endif
#endif

DeviceConfig deviceConfig;

static const char* NVS_NAMESPACE = "deskradar";

// ---------------------------------------------------------------------------
//  Tabla de campos
//
//  AGREGAR UNA API KEY NUEVA = AGREGAR UNA FILA ACA. Con eso aparece sola en el
//  portal cautivo, en /config y en el guardado a NVS. No hay que tocar HTML.
//
//  La clave NVS no puede pasar de 15 caracteres.
// ---------------------------------------------------------------------------
static const DeviceConfig::Field CONFIG_FIELDS[] = {
  { "wifi_ssid", "ssid",  "Red WiFi",              "El nombre de tu red de 2,4 GHz", false, true  },
  { "wifi_pass", "pass",  "Contrasena WiFi",       "",                               true,  true  },
  { "os_id",     "osid",  "OpenSky Client ID",     "opensky-network.org -> My OpenSky -> API Client", false, false },
  { "os_secret", "ossec", "OpenSky Client Secret", "",                               true,  false },
  { "gnews_key", "gnews", "GNews API key",         "gnews.io/register (gratis, sin tarjeta)", true, false },
  { "web_pin",   "pin",   "PIN del panel web",     "Usuario: admin. Vacio = panel abierto a toda la red. Escribi off para quitarlo.", true, false },
};
static const int CONFIG_FIELD_COUNT =
    sizeof(CONFIG_FIELDS) / sizeof(CONFIG_FIELDS[0]);

int DeviceConfig::fieldCount() { return CONFIG_FIELD_COUNT; }
const DeviceConfig::Field& DeviceConfig::field(int i) { return CONFIG_FIELDS[i]; }

int DeviceConfig::indexOf(const char* name) {
  for (int i = 0; i < CONFIG_FIELD_COUNT; i++) {
    if (strcmp(CONFIG_FIELDS[i].name, name) == 0) return i;
  }
  return -1;
}

static const String EMPTY_STRING = "";

const String& DeviceConfig::get(const char* name) const {
  int i = indexOf(name);
  return (i < 0) ? EMPTY_STRING : _values[i];
}

void DeviceConfig::set(const char* name, const String& value) {
  int i = indexOf(name);
  if (i >= 0) _values[i] = value;
}

bool DeviceConfig::hasWifi() const {
  return get("ssid").length() > 0;
}

void DeviceConfig::begin() {
  static_assert(CONFIG_FIELD_COUNT <= MAX_FIELDS,
                "Subi MAX_FIELDS en DeviceConfig.h: no entran todos los campos");

  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true); // solo lectura
  for (int i = 0; i < CONFIG_FIELD_COUNT; i++) {
    _values[i] = prefs.getString(CONFIG_FIELDS[i].key, "");
  }
  // Marca de que la precarga desde secrets.h ya se hizo alguna vez.
  bool alreadySeeded = prefs.getBool("seeded", false);
  prefs.end();

#ifdef DESKRADAR_HAS_SECRETS_H
  // Precarga de conveniencia para desarrollo, UNA SOLA VEZ en la vida del
  // dispositivo.
  //
  // El marcador "seeded" no es un detalle: sin él, "Reiniciar config WiFi"
  // quedaria roto en cualquier equipo que tenga secrets.h. clearAccess() deja el
  // SSID vacio, y en el proximo arranque la precarga lo volveria a llenar, asi
  // que el reset se desharia solo y en silencio.
  bool seeded = false;
  if (alreadySeeded) {
    Serial.println("[Config] secrets.h presente, pero la precarga ya se hizo antes");
  }
  auto seed = [&](const char* name, const char* value) {
    if (alreadySeeded) return;
    if (!value || !value[0]) return;
    // Los placeholders del .example no son credenciales
    if (strncmp(value, "PONE_ACA", 8) == 0 || strncmp(value, "TU_", 3) == 0) return;
    if (get(name).length() == 0) { set(name, value); seeded = true; }
  };
  #ifdef WIFI_SSID
    seed("ssid", WIFI_SSID);
  #endif
  #ifdef WIFI_PASSWORD
    seed("pass", WIFI_PASSWORD);
  #endif
  #ifdef OPENSKY_CLIENT_ID
    seed("osid", OPENSKY_CLIENT_ID);
  #endif
  #ifdef OPENSKY_CLIENT_SECRET
    seed("ossec", OPENSKY_CLIENT_SECRET);
  #endif
  #ifdef GNEWS_API_KEY
    seed("gnews", GNEWS_API_KEY);
  #endif

  if (seeded) {
    Serial.println("[Config] NVS vacio: precargado desde secrets.h (solo desarrollo)");
    save();
  }
  if (!alreadySeeded) {
    // Se marca aunque no se haya precargado nada: la oportunidad de sembrar era
    // esta y no se repite.
    Preferences mark;
    mark.begin(NVS_NAMESPACE, false);
    mark.putBool("seeded", true);
    mark.end();
  }
#endif

  Serial.printf("[Config] %d campos cargados de NVS, WiFi %s\n",
                CONFIG_FIELD_COUNT, hasWifi() ? "configurado" : "SIN configurar");
}

void DeviceConfig::save() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  for (int i = 0; i < CONFIG_FIELD_COUNT; i++) {
    prefs.putString(CONFIG_FIELDS[i].key, _values[i]);
  }
  prefs.end();
  Serial.println("[Config] Guardado en NVS");
}

void DeviceConfig::clearAccess() {
  set("ssid", "");
  set("pass", "");
  set("pin", "");  // ver el comentario del header: unica via de recuperacion
  save();
  Serial.println("[Config] WiFi y PIN del panel borrados: al reiniciar arranca en modo AP");
}
