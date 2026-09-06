#pragma once
#include <Arduino.h>
#include <string.h>
#include "config.h"

// Nombre legible para un codigo ICAO de aeropuerto.
//
// Solo conocemos los 3 que ya estan en AIRPORTS (config.h): una base completa
// de aeropuertos son miles de filas, y no vale la pena para lo que esto es -un
// dato de mas en la ficha de un avion, no un buscador de vuelos-. Si el codigo
// no es ninguno de los 3, se muestra tal cual (mejor un ICAO que nada).
inline String airportLabel(const char* icao) {
  if (!icao || !icao[0]) return String("?");

  for (int i = 0; i < AIRPORT_COUNT; i++) {
    if (strcmp(AIRPORTS[i].icao, icao) == 0) return String(AIRPORTS[i].name);
  }
  return String(icao);
}
