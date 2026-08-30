#pragma once
#include <vector>
#include "DisplayManager.h"
#include "OpenSkyClient.h"
#include "AircraftBlip.h"

class RadarScreen {
  public:
    explicit RadarScreen(DisplayManager& display) : _display(display) {}

    // Dibuja el radar completo con la lista de aviones ya calculada
    // (distanceKm y bearingDeg deben estar llenos).
    void render(std::vector<AircraftState>& aircraft, bool fastMode);

    // true si algún avión está dentro de RADAR_NEAR_KM
    static bool hasNearbyTraffic(const std::vector<AircraftState>& aircraft);

    // Busca un avión en las coordenadas tocadas. Devuelve nullptr si no hay.
    const AircraftState* hitTest(uint16_t x, uint16_t y) const;

  private:
    DisplayManager& _display;
    std::vector<AircraftBlip> _blips; // se rellena en cada render()
};
