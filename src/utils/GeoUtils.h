#pragma once
#include <Arduino.h>

namespace GeoUtils {

  constexpr double EARTH_RADIUS_KM = 6371.0;

  inline double toRad(double deg) { return deg * PI / 180.0; }
  inline double toDeg(double rad) { return rad * 180.0 / PI; }

  // Distancia en km entre dos puntos (fórmula de Haversine)
  inline double distanceKm(double lat1, double lon1, double lat2, double lon2) {
    double dLat = toRad(lat2 - lat1);
    double dLon = toRad(lon2 - lon1);
    double a = sin(dLat / 2) * sin(dLat / 2) +
               cos(toRad(lat1)) * cos(toRad(lat2)) *
               sin(dLon / 2) * sin(dLon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return EARTH_RADIUS_KM * c;
  }

  // Rumbo inicial en grados (0-360, 0 = Norte) desde punto 1 hacia punto 2
  inline double bearingDeg(double lat1, double lon1, double lat2, double lon2) {
    double phi1 = toRad(lat1);
    double phi2 = toRad(lat2);
    double dLon = toRad(lon2 - lon1);

    double y = sin(dLon) * cos(phi2);
    double x = cos(phi1) * sin(phi2) - sin(phi1) * cos(phi2) * cos(dLon);
    double theta = atan2(y, x);
    double deg = fmod(toDeg(theta) + 360.0, 360.0);
    return deg;
  }

  // Bounding box aproximado (lamin, lamax, lomin, lomax) alrededor de un
  // punto dado, para un radio en km. Suficiente para bbox chicos (<~100km).
  struct BBox { double lamin, lamax, lomin, lomax; };

  inline BBox boundingBox(double lat, double lon, double radiusKm) {
    double dLat = radiusKm / 111.32; // 1 grado lat ~ 111.32 km
    double dLon = radiusKm / (111.32 * cos(toRad(lat)));
    BBox box;
    box.lamin = lat - dLat;
    box.lamax = lat + dLat;
    box.lomin = lon - dLon;
    box.lomax = lon + dLon;
    return box;
  }
}
