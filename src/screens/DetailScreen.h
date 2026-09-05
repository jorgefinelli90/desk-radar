#pragma once
#include "DisplayManager.h"
#include "OpenSkyClient.h"

class DetailScreen {
  public:
    explicit DetailScreen(DisplayManager& display) : _display(display) {}

    void render(const AircraftState& a);

  private:
    DisplayManager& _display;

    // Convierte un rumbo en grados a un texto cardinal (N, NE, E, ...)
    static const char* cardinal(double deg);
};
