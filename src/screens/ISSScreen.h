#pragma once
#include "core/DisplayManager.h"
#include "services/ISSClient.h"

class ISSScreen {
  public:
    explicit ISSScreen(DisplayManager& display) : _display(display) {}

    void render(const ISSClient& iss);

  private:
    DisplayManager& _display;
};
