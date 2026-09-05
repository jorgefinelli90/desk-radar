#pragma once
#include <vector>
#include "core/DisplayManager.h"
#include "services/OpenSkyClient.h"
#include "config.h"
#include "models/UiRect.h"

class AirportScreen {
  public:
    explicit AirportScreen(DisplayManager& display) : _display(display) {}

    void render(const AirportDef& airport,
                std::vector<AircraftState>& aircraft,
                bool fastMode);

    // Rectángulo del botón "Siguiente >" dibujado en el último render()
    const UiRect& nextButtonRect() const { return _nextBtn; }

  private:
    DisplayManager& _display;
    UiRect _nextBtn;
};
