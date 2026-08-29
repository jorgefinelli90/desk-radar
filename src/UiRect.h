#pragma once

struct UiRect {
  int x, y, w, h;

  bool contains(int px, int py) const {
    return px >= x && px < x + w && py >= y && py < y + h;
  }
};
