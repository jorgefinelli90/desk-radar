#pragma once
#include <Arduino.h>

// Configuración del dispositivo persistida en NVS.
//
// Reemplaza a include/secrets.h como fuente de verdad: antes las credenciales
// se leían en tiempo de COMPILACIÓN, ahora en tiempo de EJECUCIÓN, así que se
// pueden cambiar desde el navegador sin recompilar ni reflashear.
//
// Para agregar una API key nueva alcanza con sumar una fila a CONFIG_FIELDS en
// DeviceConfig.cpp: el formulario del portal cautivo, el de /config y el
// guardado en NVS se arman todos a partir de esa tabla.
class DeviceConfig {
  public:
    // Un campo configurable.
    struct Field {
      const char* key;    // clave NVS. OJO: máximo 15 caracteres, lo impone NVS.
      const char* name;   // atributo name= del <input>
      const char* label;  // etiqueta visible en el formulario
      const char* hint;   // ayuda debajo del campo (puede ser "")
      bool        secret; // password: nunca se devuelve el valor al navegador
      bool        wifi;   // si cambia, hay que reconectarse a la red
    };

    // Carga todo de NVS. Si no hay nada guardado y existe include/secrets.h con
    // valores reales, los precarga (comodidad para desarrollar localmente).
    void begin();

    const String& get(const char* name) const;
    void set(const char* name, const String& value);

    // Escribe a NVS lo que haya en memoria.
    void save();

    bool hasWifi() const;

    // Borra SSID, contraseña y PIN del panel, y deja el resto (las API keys
    // sobreviven). Al reiniciar arranca en modo AP.
    //
    // El PIN va acá y no en un botón aparte porque esta es la única vía de
    // recuperación si te lo olvidás: exige estar parado frente al aparato y
    // confirmar con dos toques, que es prueba suficiente de que el dispositivo
    // es tuyo. Sin esto, un PIN olvidado dejaba el panel inaccesible para
    // siempre salvo reflasheando la NVS.
    void clearAccess();

    static int fieldCount();
    static const Field& field(int i);

  private:
    static const int MAX_FIELDS = 8;
    String _values[MAX_FIELDS];

    static int indexOf(const char* name);
};

// Instancia global: la usan main, el portal web y OpenSkyClient.
extern DeviceConfig deviceConfig;
