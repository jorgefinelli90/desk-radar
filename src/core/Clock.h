#pragma once
#include <Arduino.h>

// Reloj del dispositivo, sincronizado por NTP.
//
// Hasta ahora el firmware no sabía qué hora era: los titulares se fechaban
// restando 3 a mano sobre el string que devolvía GNews, y el clima mostraba la
// hora que venía en la respuesta de Open-Meteo. Un aparato que vive encendido
// en un escritorio y no tiene hora se siente incompleto, y además sin hora no
// se pueden validar certificados TLS: una cadena no se puede verificar sin
// saber qué día es.
//
// La sincronización no bloquea. configTime() deja andando el cliente SNTP del
// core y la hora aparece sola unos segundos después; mientras tanto hasTime()
// devuelve false y quien la use simplemente no dibuja nada.
class Clock {
  public:
    // Llamar cuando ya hay IP. Es idempotente: llamarla de nuevo en cada
    // reconexión no reinicia nada.
    void begin();

    // true cuando el sistema ya tiene una hora creíble (ver el .cpp).
    bool hasTime() const;

    // "HH:MM" en hora local, o "" si todavía no sincronizó.
    String hhmm() const;

  private:
    bool _started = false;

    // mutable porque hasTime() es un getter const pero necesita recordar que ya
    // sincronizó: sin el cache, cada consulta vuelve a pedir la hora al sistema.
    mutable bool _synced = false;
};
