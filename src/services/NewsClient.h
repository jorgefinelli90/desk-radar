#pragma once
#include <Arduino.h>
#include <vector>

struct NewsItem {
  String title;   // ya pasado a ASCII, listo para dibujar
  String source;  // nombre del medio
  String time;    // "HH:MM" hora local de Argentina, o "" si no vino
  String summary; // el "description" de GNews: una o dos frases de resumen
};

// Trae los titulares más recientes de Argentina desde GNews.io y los mantiene
// cacheados en RAM. El cache evita quemar el cupo del free tier (100 req/día)
// si la pantalla queda puesta un rato largo.
class NewsClient {
  public:
    // La API key se lee de NVS en cada refresh (ver DeviceConfig), no del
    // compilador: se puede cambiar desde el navegador sin recompilar.
    NewsClient() = default;

    // true si conviene volver a pedir: nunca pedimos, el cache venció, o el
    // último intento falló y ya pasó el tiempo de reintento.
    bool shouldRefresh() const;

    // Pega a la API y actualiza el cache. Devuelve false si falló.
    bool refresh();

    const std::vector<NewsItem>& items() const { return _items; }
    bool hasData() const { return _ok; }
    const String& lastError() const { return _lastError; }

  private:
    std::vector<NewsItem> _items;

    bool     _ok = false;            // el último refresh trajo datos buenos
    bool     _everTried = false;
    uint32_t _lastOkMs = 0;
    uint32_t _lastTryMs = 0;
    String   _lastError;

    // "2026-08-30T14:23:00Z" (UTC) -> "11:23" (hora de Argentina)
    static String localTimeFromIso(const String& iso);
};
