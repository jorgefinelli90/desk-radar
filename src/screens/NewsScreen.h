#pragma once
#include "core/DisplayManager.h"
#include "services/NewsClient.h"

class NewsScreen {
  public:
    explicit NewsScreen(DisplayManager& display) : _display(display) {}

    void render(const NewsClient& news);

  private:
    DisplayManager& _display;
};
