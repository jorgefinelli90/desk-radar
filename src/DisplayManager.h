#pragma once
#include <TFT_eSPI.h>

class DisplayManager {
  public:
    static const int STATUS_BAR_HEIGHT = 16;

    void begin();
    TFT_eSPI& tft() { return _tft; }

    void showMessage(const String& msg);
    void showStatusBar(const String& left, const String& right, bool fastMode);

  private:
    TFT_eSPI _tft = TFT_eSPI();
};
