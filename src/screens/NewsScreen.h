#pragma once
#include "core/DisplayManager.h"
#include "services/NewsClient.h"
#include "models/UiRect.h"

class NewsScreen {
  public:
    explicit NewsScreen(DisplayManager& display) : _display(display) {}

    void render(const NewsClient& news);

    // Índice del titular tocado, o -1 si el toque cayó afuera. Las zonas las
    // deja el último render(), que es el que sabe cuántas filas entraron en
    // pantalla y a qué altura quedó cada una.
    int hitTest(uint16_t x, uint16_t y) const;

  private:
    DisplayManager& _display;

    // Tope de filas con zona tocable. En 320 px de alto y 58 por fila entran
    // cinco, que es también el máximo que pide NewsClient.
    static const int MAX_ROWS = 8;

    UiRect _rows[MAX_ROWS];
    int    _rowCount = 0;
};
