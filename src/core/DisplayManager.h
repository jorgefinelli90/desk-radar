#pragma once
#include <TFT_eSPI.h>
#include "core/Clock.h"

class DisplayManager {
  public:
    static const int STATUS_BAR_HEIGHT = 16;

    void begin();
    TFT_eSPI& tft() { return _tft; }

    // El reloj de la barra de estado, inyectado una vez desde setup(). La barra
    // la dibuja esta clase, así que es la que tiene que saber la hora.
    void setClock(const Clock* clock) { _clock = clock; }

    void showMessage(const String& msg);

    // Dibuja la barra superior: navegación a la izquierda, estado de la
    // pantalla a la derecha y el reloj pegado al borde.
    //
    // El color del texto de la derecha lo decide el llamador. Antes era un
    // `bool fastMode`, pero desde que ahí va la antigüedad del último fetch
    // hacen falta tres estados y no dos: refresco normal, refresco rápido y
    // dato viejo.
    void showStatusBar(const String& left, const String& right,
                       uint16_t rightColor = TFT_SILVER);

    // Repinta SOLO el reloj, y solo si cambió el minuto; en cualquier otro caso
    // vuelve enseguida, así que se puede llamar en cada vuelta del loop. Sin
    // esto la hora se actualizaría únicamente cuando la pantalla se redibuja
    // entera, y en Detalle o Ajustes se quedaría congelada varios minutos.
    void tickStatusClock();

  private:
    TFT_eSPI _tft = TFT_eSPI();
    const Clock* _clock = nullptr;

    // Último texto de reloj dibujado y en qué x termina, para poder repintar
    // solo esa esquina.
    String _clockDrawn;
    int    _clockRight = 0;

    // Copia de lo último que se pidió dibujar. Hace falta para el caso en que
    // la hora aparece por primera vez: hasta entonces el texto de la derecha
    // estaba ocupando el lugar del reloj, así que hay que rehacer la barra.
    String   _statusLeft;
    String   _statusRight;
    uint16_t _statusColor = TFT_SILVER;

    // Dibuja el reloj y devuelve la x hasta donde llega el espacio libre para
    // el texto de la derecha.
    int drawStatusClock();
};
