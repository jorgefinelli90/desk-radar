#pragma once
#include "core/DisplayManager.h"
#include "services/OpenSkyClient.h"

class DetailScreen {
  public:
    explicit DetailScreen(DisplayManager& display) : _display(display) {}

    // route puede llegar sin valid (todavia no respondio, o OpenSky no
    // encontro vuelos): en ese caso no se dibuja la linea de ruta.
    void render(const AircraftState& a, const RouteInfo& route);

  private:
    DisplayManager& _display;

    // Convierte un rumbo en grados a un texto cardinal (N, NE, E, ...)
    static const char* cardinal(double deg);
};
