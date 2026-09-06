#pragma once
#include "core/DisplayManager.h"
#include "models/UiRect.h"

enum class InfoChoice { None, News, Weather, ISS };

class InfoMenuScreen {
  public:
    explicit InfoMenuScreen(DisplayManager& display) : _display(display) {}

    void render();
    InfoChoice hitTest(uint16_t x, uint16_t y) const;

  private:
    DisplayManager& _display;
    UiRect _newsBtn;
    UiRect _weatherBtn;
    UiRect _issBtn;
};
