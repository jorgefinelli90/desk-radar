#pragma once
#include "DisplayManager.h"
#include "NewsClient.h"

class NewsScreen {
  public:
    explicit NewsScreen(DisplayManager& display) : _display(display) {}

    void render(const NewsClient& news);

  private:
    DisplayManager& _display;
};
