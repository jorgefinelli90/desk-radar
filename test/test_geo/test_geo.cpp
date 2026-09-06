// Tests de las dos unidades geometricas del proyecto: GeoUtils (haversine,
// rumbo, bounding box) y GeoMap (Web Mercator contra el mapa ya generado).
//
// Son las dos piezas donde un error NO se nota mirando la pantalla: un avion
// dibujado 3 km corrido no tira ninguna excepcion, simplemente aparece sobre la
// calle equivocada. tools/verify-map.mjs valida el GENERADOR del mapa; esto
// valida lo que despues hace el FIRMWARE con esos mismos numeros.
//
// Correr con:  pio test -e esp32dev
//
// Los valores esperados salen de dos lados distintos a proposito:
//   - constantes con referencia externa (1 grado de latitud, antipodas), y
//   - una implementacion independiente en Node de la misma formula, para las
//     distancias a los aeropuertos reales del proyecto.
//
// Se usan asserts de float y no de double porque Unity solo trae los de double
// si se lo compila con UNITY_INCLUDE_DOUBLE; con las 7 cifras significativas de
// un float sobra para las tolerancias de abajo.

#include <Arduino.h>
#include <unity.h>

#include "config.h"
#include "utils/GeoUtils.h"
#include "utils/GeoMap.h"
#include "utils/DateUtils.h"
#include "MapAssets.h"

void setUp(void) {}
void tearDown(void) {}

// --- GeoUtils: distancia ----------------------------------------------------

void test_distancia_de_un_punto_a_si_mismo_es_cero(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f,
      (float)GeoUtils::distanceKm(HOME_LAT, HOME_LON, HOME_LAT, HOME_LON));
}

// 2*pi*R/360 con R = 6371 km
void test_un_grado_de_latitud_son_111_19_km(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 111.1949f,
      (float)GeoUtils::distanceKm(0.0, 0.0, 1.0, 0.0));
}

void test_un_grado_de_longitud_en_el_ecuador_mide_lo_mismo(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 111.1949f,
      (float)GeoUtils::distanceKm(0.0, 0.0, 0.0, 1.0));
}

// Media vuelta al planeta: pi*R. Ejercita el atan2 con el argumento en el
// extremo, que es donde una haversine mal escrita se rompe.
void test_antipodas(void) {
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 20015.087f,
      (float)GeoUtils::distanceKm(0.0, 0.0, 0.0, 180.0));
}

void test_la_distancia_es_simetrica(void) {
  double ida    = GeoUtils::distanceKm(HOME_LAT, HOME_LON, -34.8222, -58.5358);
  double vuelta = GeoUtils::distanceKm(-34.8222, -58.5358, HOME_LAT, HOME_LON);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, (float)ida, (float)vuelta);
}

// Los tres aeropuertos del proyecto, medidos desde la casa de config.h.
void test_distancias_a_los_aeropuertos_reales(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.2621f,
      (float)GeoUtils::distanceKm(HOME_LAT, HOME_LON, -34.6098, -58.6122)); // Palomar
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 26.7786f,
      (float)GeoUtils::distanceKm(HOME_LAT, HOME_LON, -34.8222, -58.5358)); // Ezeiza
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 16.3929f,
      (float)GeoUtils::distanceKm(HOME_LAT, HOME_LON, -34.5592, -58.4156)); // Aeroparque
}

// --- GeoUtils: rumbo --------------------------------------------------------

void test_rumbos_cardinales(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f,   (float)GeoUtils::bearingDeg(0, 0,  1,  0));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f,  (float)GeoUtils::bearingDeg(0, 0,  0,  1));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 180.0f, (float)GeoUtils::bearingDeg(0, 0, -1,  0));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 270.0f, (float)GeoUtils::bearingDeg(0, 0,  0, -1));
}

// RadarScreen::updatePings compara el rumbo contra el angulo del barrido dando
// por sentado el rango [0,360). Un valor negativo dejaria a ese avion sin
// destello para siempre, y no habria ningun otro sintoma.
void test_el_rumbo_siempre_cae_en_0_360(void) {
  const double destinos[][2] = {
    { -34.6098, -58.6122 }, { -34.8222, -58.5358 }, { -34.5592, -58.4156 },
    { -34.0,    -59.5    }, { -35.5,    -57.5    }, { -34.5858006, -59.0 },
  };
  for (unsigned i = 0; i < sizeof(destinos) / sizeof(destinos[0]); i++) {
    double b = GeoUtils::bearingDeg(HOME_LAT, HOME_LON, destinos[i][0], destinos[i][1]);
    TEST_ASSERT_TRUE_MESSAGE(b >= 0.0 && b < 360.0, "rumbo fuera de [0,360)");
  }
}

void test_rumbos_a_los_aeropuertos_reales(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 215.102f,
      (float)GeoUtils::bearingDeg(HOME_LAT, HOME_LON, -34.6098, -58.6122)); // Palomar, al SO
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 169.014f,
      (float)GeoUtils::bearingDeg(HOME_LAT, HOME_LON, -34.8222, -58.5358)); // Ezeiza, al S
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 79.655f,
      (float)GeoUtils::bearingDeg(HOME_LAT, HOME_LON, -34.5592, -58.4156)); // Aeroparque, al E
}

// --- GeoUtils: cardinal ------------------------------------------------------

void test_cardinal_de_los_8_rumbos_exactos(void) {
  TEST_ASSERT_EQUAL_STRING("N",  GeoUtils::cardinal(0));
  TEST_ASSERT_EQUAL_STRING("NE", GeoUtils::cardinal(45));
  TEST_ASSERT_EQUAL_STRING("E",  GeoUtils::cardinal(90));
  TEST_ASSERT_EQUAL_STRING("SE", GeoUtils::cardinal(135));
  TEST_ASSERT_EQUAL_STRING("S",  GeoUtils::cardinal(180));
  TEST_ASSERT_EQUAL_STRING("SO", GeoUtils::cardinal(225));
  TEST_ASSERT_EQUAL_STRING("O",  GeoUtils::cardinal(270));
  TEST_ASSERT_EQUAL_STRING("NO", GeoUtils::cardinal(315));
}

// El corte entre NO y N cae en 337.5: justo antes tiene que seguir dando NO, y
// 360 (una vuelta completa) tiene que volver a dar N como 0.
void test_cardinal_en_los_bordes_de_cada_sector(void) {
  TEST_ASSERT_EQUAL_STRING("N",  GeoUtils::cardinal(22.49));
  TEST_ASSERT_EQUAL_STRING("NE", GeoUtils::cardinal(22.51));
  TEST_ASSERT_EQUAL_STRING("NO", GeoUtils::cardinal(337.49));
  TEST_ASSERT_EQUAL_STRING("N",  GeoUtils::cardinal(337.51));
  TEST_ASSERT_EQUAL_STRING("N",  GeoUtils::cardinal(360.0));
}

// bearingDeg nunca da negativo (ver test_el_rumbo_siempre_cae_en_0_360), pero
// cardinal es una funcion de proposito general: un rumbo negativo chico (el
// caso realista si algun dia se le suma un offset a mano) tiene que dar el
// mismo resultado que su equivalente positivo, no un indice fuera de rango.
void test_cardinal_con_grados_negativos(void) {
  TEST_ASSERT_EQUAL_STRING(GeoUtils::cardinal(350.0), GeoUtils::cardinal(-10.0));
}

// --- GeoUtils: bounding box -------------------------------------------------

void test_el_bbox_contiene_su_centro(void) {
  GeoUtils::BBox b = GeoUtils::boundingBox(HOME_LAT, HOME_LON, HOME_FETCH_RADIUS_KM);
  TEST_ASSERT_TRUE(b.lamin < HOME_LAT && HOME_LAT < b.lamax);
  TEST_ASSERT_TRUE(b.lomin < HOME_LON && HOME_LON < b.lomax);
}

// El recuadro que se le manda a OpenSky tiene que cubrir de verdad el radio
// pedido: si quedara corto, los aviones del borde no llegarian nunca y el radar
// se veria vacio justo en el anillo exterior.
void test_el_bbox_cubre_el_radio_pedido(void) {
  const double R = HOME_FETCH_RADIUS_KM;

  // Un metro de margen. El recuadro es una aproximacion plana y su error de
  // segundo orden es de centimetros; eso no es lo que este test vigila. Lo que
  // vigila es que no quede corto por medir con un radio terrestre distinto al
  // que usa distanceKm, que es lo que pasaba con el 111.32 fijo: 50 m de
  // deficit sobre estos mismos 45 km.
  const double margen = 0.001; // km

  GeoUtils::BBox b = GeoUtils::boundingBox(HOME_LAT, HOME_LON, R);

  TEST_ASSERT_TRUE(GeoUtils::distanceKm(HOME_LAT, HOME_LON, b.lamax, HOME_LON) >= R - margen);
  TEST_ASSERT_TRUE(GeoUtils::distanceKm(HOME_LAT, HOME_LON, b.lamin, HOME_LON) >= R - margen);
  TEST_ASSERT_TRUE(GeoUtils::distanceKm(HOME_LAT, HOME_LON, HOME_LAT, b.lomax) >= R - margen);
  TEST_ASSERT_TRUE(GeoUtils::distanceKm(HOME_LAT, HOME_LON, HOME_LAT, b.lomin) >= R - margen);
}

// Fuera del ecuador un grado de longitud mide menos que uno de latitud, asi que
// el recuadro tiene que ser mas ancho en grados. Sin el cos(lat) de la cuenta
// quedaria angosto y se perderian aviones al este y al oeste.
void test_el_bbox_se_ensancha_en_longitud_por_la_latitud(void) {
  GeoUtils::BBox b = GeoUtils::boundingBox(HOME_LAT, HOME_LON, HOME_FETCH_RADIUS_KM);
  TEST_ASSERT_TRUE((b.lomax - b.lomin) > (b.lamax - b.lamin));
}

// --- GeoMap: Web Mercator ---------------------------------------------------

void test_los_bordes_del_mundo(void) {
  const int z = 10;
  const double ancho = 256.0 * 1024.0; // 256 * 2^10
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f,             (float)GeoMap::lonToWorldPx(-180.0, z));
  TEST_ASSERT_FLOAT_WITHIN(0.5f,  (float)ancho,     (float)GeoMap::lonToWorldPx( 180.0, z));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, (float)(ancho/2), (float)GeoMap::lonToWorldPx(   0.0, z));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, (float)(ancho/2), (float)GeoMap::latToWorldPy(   0.0, z));
}

void test_mas_al_sur_es_mas_abajo(void) {
  TEST_ASSERT_TRUE(GeoMap::latToWorldPy(-34.0, 10) < GeoMap::latToWorldPy(-35.0, 10));
}

// El clamp de +-0.9999 esta para que un dato corrupto de OpenSky no meta un
// infinito en el logaritmo y termine dibujando en cualquier lado.
void test_las_latitudes_polares_no_dan_infinito(void) {
  double norte = GeoMap::latToWorldPy( 90.0, 10);
  double sur   = GeoMap::latToWorldPy(-90.0, 10);
  TEST_ASSERT_TRUE(isfinite(norte));
  TEST_ASSERT_TRUE(isfinite(sur));
  TEST_ASSERT_TRUE(norte < sur);
}

// El test que ata el firmware al mapa: los .bin son un recorte centrado en la
// casa, asi que proyectar HOME_LAT/HOME_LON tiene que caer en el centro exacto
// del viewport. Si esto falla, MapAssets.h y la imagen no son del mismo lugar
// y entonces TODOS los aviones estan corridos.
void test_la_casa_cae_en_el_centro_de_cada_mapa(void) {
  for (int i = 0; i < MAP_ASSET_COUNT; i++) {
    float x = GeoMap::screenX(HOME_LON, MAP_ASSETS[i]);
    float y = GeoMap::screenY(HOME_LAT, MAP_ASSETS[i]);
    TEST_ASSERT_TRUE_MESSAGE(GeoMap::inView(x, y), "la casa quedo fuera del mapa");
    TEST_ASSERT_FLOAT_WITHIN(1.0f, MAP_VIEW_W / 2.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, MAP_VIEW_H / 2.0f, y);
  }
}

// Lo mismo para el disco del radar, que tiene su propio recorte y su propia
// escala.
void test_la_casa_cae_en_el_centro_del_disco_del_radar(void) {
  float x = GeoMap::screenX(HOME_LON, RADAR_MAP);
  float y = GeoMap::screenY(HOME_LAT, RADAR_MAP);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, RADAR_DISC_SIZE / 2.0f, x);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, RADAR_DISC_SIZE / 2.0f, y);
}

void test_un_punto_lejano_queda_fuera_del_viewport(void) {
  float x = GeoMap::screenX(-64.1888, MAP_ASSETS[0]); // Cordoba
  float y = GeoMap::screenY(-31.4201, MAP_ASSETS[0]);
  TEST_ASSERT_FALSE(GeoMap::inView(x, y));
}

// --- DateUtils::dayOfWeek ----------------------------------------------------
// Le pone nombre a los dias del pronostico extendido del clima. Las cuatro
// fechas de referencia son hechos verificables por fuera del proyecto, no
// resultados de la propia formula: 1900-01-01 y 2024-01-01 fueron lunes,
// 2000-01-01 (el Y2K) fue sabado, y el 29 de febrero de 2024 (bisiesto) fue
// jueves.

void test_dayOfWeek_1900_01_01_es_lunes(void) {
  TEST_ASSERT_EQUAL_INT(1, DateUtils::dayOfWeek(1900, 1, 1));
}

void test_dayOfWeek_2000_01_01_es_sabado(void) {
  TEST_ASSERT_EQUAL_INT(6, DateUtils::dayOfWeek(2000, 1, 1));
}

void test_dayOfWeek_2024_01_01_es_lunes(void) {
  TEST_ASSERT_EQUAL_INT(1, DateUtils::dayOfWeek(2024, 1, 1));
}

// El caso importante para probar el ajuste de anio en meses 1-2 (ver el "if
// (month < 3)" de la formula): un 29 de febrero solo existe en anio bisiesto,
// y es justo el mes que dispara ese ajuste.
void test_dayOfWeek_bisiesto_29_febrero_2024_es_jueves(void) {
  TEST_ASSERT_EQUAL_INT(4, DateUtils::dayOfWeek(2024, 2, 29));
}

// Siete dias seguidos tienen que dar los siete valores en orden, dando la
// vuelta de sabado (6) a domingo (0).
void test_dayOfWeek_una_semana_completa_en_orden(void) {
  // 2024-01-01 es lunes (indice 1); del 31 de diciembre de 2023 (domingo) al
  // 6 de enero de 2024 (sabado) es una semana calendario completa.
  TEST_ASSERT_EQUAL_INT(0, DateUtils::dayOfWeek(2023, 12, 31)); // Dom
  TEST_ASSERT_EQUAL_INT(1, DateUtils::dayOfWeek(2024, 1, 1));   // Lun
  TEST_ASSERT_EQUAL_INT(2, DateUtils::dayOfWeek(2024, 1, 2));   // Mar
  TEST_ASSERT_EQUAL_INT(3, DateUtils::dayOfWeek(2024, 1, 3));   // Mie
  TEST_ASSERT_EQUAL_INT(4, DateUtils::dayOfWeek(2024, 1, 4));   // Jue
  TEST_ASSERT_EQUAL_INT(5, DateUtils::dayOfWeek(2024, 1, 5));   // Vie
  TEST_ASSERT_EQUAL_INT(6, DateUtils::dayOfWeek(2024, 1, 6));   // Sab
}

void setup() {
  delay(2000); // que el monitor serie alcance a engancharse
  UNITY_BEGIN();

  RUN_TEST(test_distancia_de_un_punto_a_si_mismo_es_cero);
  RUN_TEST(test_un_grado_de_latitud_son_111_19_km);
  RUN_TEST(test_un_grado_de_longitud_en_el_ecuador_mide_lo_mismo);
  RUN_TEST(test_antipodas);
  RUN_TEST(test_la_distancia_es_simetrica);
  RUN_TEST(test_distancias_a_los_aeropuertos_reales);

  RUN_TEST(test_rumbos_cardinales);
  RUN_TEST(test_el_rumbo_siempre_cae_en_0_360);
  RUN_TEST(test_rumbos_a_los_aeropuertos_reales);

  RUN_TEST(test_cardinal_de_los_8_rumbos_exactos);
  RUN_TEST(test_cardinal_en_los_bordes_de_cada_sector);
  RUN_TEST(test_cardinal_con_grados_negativos);

  RUN_TEST(test_el_bbox_contiene_su_centro);
  RUN_TEST(test_el_bbox_cubre_el_radio_pedido);
  RUN_TEST(test_el_bbox_se_ensancha_en_longitud_por_la_latitud);

  RUN_TEST(test_los_bordes_del_mundo);
  RUN_TEST(test_mas_al_sur_es_mas_abajo);
  RUN_TEST(test_las_latitudes_polares_no_dan_infinito);
  RUN_TEST(test_la_casa_cae_en_el_centro_de_cada_mapa);
  RUN_TEST(test_la_casa_cae_en_el_centro_del_disco_del_radar);
  RUN_TEST(test_un_punto_lejano_queda_fuera_del_viewport);

  RUN_TEST(test_dayOfWeek_1900_01_01_es_lunes);
  RUN_TEST(test_dayOfWeek_2000_01_01_es_sabado);
  RUN_TEST(test_dayOfWeek_2024_01_01_es_lunes);
  RUN_TEST(test_dayOfWeek_bisiesto_29_febrero_2024_es_jueves);
  RUN_TEST(test_dayOfWeek_una_semana_completa_en_orden);

  UNITY_END();
}

void loop() {}
