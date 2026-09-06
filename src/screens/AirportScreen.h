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
                const String& status, uint16_t statusColor);

    // Rectángulo del botón "Siguiente >" dibujado en el último render()
    const UiRect& nextButtonRect() const { return _nextBtn; }

    // Avion tocado en la lista, o nullptr si el toque no cayo en ninguna fila.
    // Radar y Mapa ya tenian esto; era la unica de las tres pantallas de
    // aviones donde tocar no hacia nada.
    const AircraftState* hitTest(uint16_t x, uint16_t y) const;

  private:
    DisplayManager& _display;
    UiRect _nextBtn;

    // Tope de filas con zona tocable, mismo patron que MAX_ROWS en NewsScreen:
    // en 320 px de alto con filas de 34 px entran menos de 10, asi que sobra
    // margen.
    static const int MAX_ROWS = 10;
    UiRect        _rows[MAX_ROWS];
    AircraftState _rowAircraft[MAX_ROWS]; // copia: aircraft puede cambiar de tamano
    int           _rowCount = 0;
};
