#pragma once
#include "models/UiRect.h"
#include "services/OpenSkyClient.h"

// Un avión dibujado en pantalla, con su zona tocable y una copia de sus datos.
// Guardamos copia (y no puntero) porque la lista de aviones se reemplaza entera
// en cada fetch, y el detalle tiene que seguir siendo válido después.
// Lo usan RadarScreen y MapScreen para el hit-test táctil.
struct AircraftBlip {
  UiRect hitBox;
  AircraftState aircraft;
};
