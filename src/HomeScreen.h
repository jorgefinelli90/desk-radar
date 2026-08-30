#pragma once
#include "DisplayManager.h"
#include "UiRect.h"

enum class HomeChoice { None, Radar, Airports, Map };

class HomeScreen {
  public:
    explicit HomeScreen(DisplayManager& display) : _display(display) {}

    void render();
    HomeChoice hitTest(uint16_t x, uint16_t y) const;

  private:
    DisplayManager& _display;
    UiRect _radarBtn;
    UiRect _airportBtn;
    UiRect _mapBtn;
};
