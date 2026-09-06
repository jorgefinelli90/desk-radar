// Tests de TextUtils: el plegado de UTF-8 a ASCII y el ajuste de texto al ancho
// de la pantalla.
//
// Todo el texto que entra por red (titulares de GNews, descripciones del clima,
// mensajes del portal) pasa por aca antes de dibujarse, porque las fuentes
// embebidas de TFT_eSPI solo tienen ASCII 32-126. Un error en el decodificador
// de UTF-8 no rompe nada visiblemente: simplemente aparecen titulares con
// agujeros o cortados, que es facil de confundir con un problema de la API.
//
// Correr con:  pio test -e esp32dev
//
// Los literales UTF-8 van con escapes hexadecimales y NO con acentos escritos
// directo: asi el test no depende de con que codificacion se guarde el archivo.
// Van partidos en literales adyacentes ("\xC3\x91" "and") porque \x en C++ se
// come todos los digitos hex que le siguen, y "\x91and" seria un solo escape.

#include <Arduino.h>
#include <unity.h>

#include "utils/TextUtils.h"
#include "utils/StrUtils.h"
#include "services/OpenSkyClient.h"
#include "config.h"

// Sin init(): textWidth() resuelve por tablas de ancho de la fuente y no toca
// el bus SPI, asi que estos tests corren aunque no haya pantalla conectada.
static TFT_eSPI tft;

// Ancho de un caracter en la fuente 1 (GLCD). TFT_eSPI la trata como monoespacio.
static const int CHAR_W = 6;

void setUp(void) {}
void tearDown(void) {}

// --- foldCodepoint ----------------------------------------------------------

void test_el_ascii_imprimible_pasa_intacto(void) {
  TEST_ASSERT_EQUAL_CHAR('a', TextUtils::foldCodepoint('a'));
  TEST_ASSERT_EQUAL_CHAR('Z', TextUtils::foldCodepoint('Z'));
  TEST_ASSERT_EQUAL_CHAR('7', TextUtils::foldCodepoint('7'));
  TEST_ASSERT_EQUAL_CHAR(' ', TextUtils::foldCodepoint(' '));
  TEST_ASSERT_EQUAL_CHAR('~', TextUtils::foldCodepoint(0x7E));
}

void test_las_vocales_acentuadas_pierden_el_acento(void) {
  TEST_ASSERT_EQUAL_CHAR('a', TextUtils::foldCodepoint(0xE1)); // a con tilde
  TEST_ASSERT_EQUAL_CHAR('e', TextUtils::foldCodepoint(0xE9));
  TEST_ASSERT_EQUAL_CHAR('i', TextUtils::foldCodepoint(0xED));
  TEST_ASSERT_EQUAL_CHAR('o', TextUtils::foldCodepoint(0xF3));
  TEST_ASSERT_EQUAL_CHAR('u', TextUtils::foldCodepoint(0xFA));
  TEST_ASSERT_EQUAL_CHAR('u', TextUtils::foldCodepoint(0xFC)); // dieresis
  TEST_ASSERT_EQUAL_CHAR('A', TextUtils::foldCodepoint(0xC1));
  TEST_ASSERT_EQUAL_CHAR('O', TextUtils::foldCodepoint(0xD3));
}

void test_la_enie_y_la_cedilla(void) {
  TEST_ASSERT_EQUAL_CHAR('n', TextUtils::foldCodepoint(0xF1));
  TEST_ASSERT_EQUAL_CHAR('N', TextUtils::foldCodepoint(0xD1));
  TEST_ASSERT_EQUAL_CHAR('c', TextUtils::foldCodepoint(0xE7));
}

void test_la_puntuacion_del_espaniol_y_las_comillas(void) {
  TEST_ASSERT_EQUAL_CHAR('!',  TextUtils::foldCodepoint(0xA1));   // signo de apertura
  TEST_ASSERT_EQUAL_CHAR('?',  TextUtils::foldCodepoint(0xBF));
  TEST_ASSERT_EQUAL_CHAR('"',  TextUtils::foldCodepoint(0xAB));   // comillas latinas
  TEST_ASSERT_EQUAL_CHAR('"',  TextUtils::foldCodepoint(0xBB));
  TEST_ASSERT_EQUAL_CHAR('\'', TextUtils::foldCodepoint(0x2019)); // apostrofo tipografico
  TEST_ASSERT_EQUAL_CHAR('"',  TextUtils::foldCodepoint(0x201C));
  TEST_ASSERT_EQUAL_CHAR('-',  TextUtils::foldCodepoint(0x2014)); // raya
  TEST_ASSERT_EQUAL_CHAR(' ',  TextUtils::foldCodepoint(0xA0));   // espacio duro
}

// Todo lo que no sepamos dibujar se descarta, no se reemplaza por basura.
void test_lo_que_no_es_dibujable_se_descarta(void) {
  TEST_ASSERT_EQUAL_CHAR(0, TextUtils::foldCodepoint(0x1F600)); // emoji
  TEST_ASSERT_EQUAL_CHAR(0, TextUtils::foldCodepoint(0x4E2D));  // ideograma
  TEST_ASSERT_EQUAL_CHAR(0, TextUtils::foldCodepoint(0x09));    // tab
}

// --- toAscii ----------------------------------------------------------------

void test_una_cadena_ascii_no_se_toca(void) {
  TEST_ASSERT_EQUAL_STRING("Vuelo AR1234 a 9500 m",
                           TextUtils::toAscii("Vuelo AR1234 a 9500 m").c_str());
}

void test_plegado_de_dos_bytes(void) {
  // "Nandu" con enie y u acentuada
  TEST_ASSERT_EQUAL_STRING("Nandu",
      TextUtils::toAscii(String("\xC3\x91" "and" "\xC3\xBA")).c_str());
}

void test_signos_de_apertura_y_comillas_latinas(void) {
  // "¿Que?" y "«Hola»"
  TEST_ASSERT_EQUAL_STRING("?Que?",
      TextUtils::toAscii(String("\xC2\xBF" "Que?")).c_str());
  TEST_ASSERT_EQUAL_STRING("\"Hola\"",
      TextUtils::toAscii(String("\xC2\xAB" "Hola" "\xC2\xBB")).c_str());
}

// Los puntos suspensivos son el unico caso que crece: un codepoint pasa a ser
// tres caracteres.
void test_los_puntos_suspensivos_se_expanden(void) {
  TEST_ASSERT_EQUAL_STRING("Sigue...",
      TextUtils::toAscii(String("Sigue" "\xE2\x80\xA6")).c_str());
}

void test_los_emojis_desaparecen_sin_dejar_rastro(void) {
  TEST_ASSERT_EQUAL_STRING("Hola mundo",
      TextUtils::toAscii(String("Hola " "\xF0\x9F\x98\x80" "mundo")).c_str());
}

// Una respuesta cortada a mitad de camino no puede hacer que el decodificador
// lea mas alla del final de la cadena.
void test_una_secuencia_truncada_no_se_va_de_rango(void) {
  TEST_ASSERT_EQUAL_STRING("ab", TextUtils::toAscii(String("ab\xC3")).c_str());
  TEST_ASSERT_EQUAL_STRING("ab", TextUtils::toAscii(String("ab\xE2\x80")).c_str());
}

void test_un_byte_de_continuacion_suelto_se_ignora(void) {
  TEST_ASSERT_EQUAL_STRING("abc", TextUtils::toAscii(String("\x80" "abc")).c_str());
}

void test_la_cadena_vacia_no_rompe_nada(void) {
  TEST_ASSERT_EQUAL_STRING("", TextUtils::toAscii(String("")).c_str());
}

void test_se_recortan_los_espacios_de_los_bordes(void) {
  TEST_ASSERT_EQUAL_STRING("hola", TextUtils::toAscii(String("  hola  ")).c_str());
}

// --- fitToWidth -------------------------------------------------------------

void test_si_entra_no_se_toca(void) {
  String s = TextUtils::fitToWidth(tft, "corto", 20 * CHAR_W, 1);
  TEST_ASSERT_EQUAL_STRING("corto", s.c_str());
}

void test_si_no_entra_se_corta_con_puntos(void) {
  const int maxW = 10 * CHAR_W;
  String s = TextUtils::fitToWidth(tft, "ABCDEFGHIJKLMNOP", maxW, 1);

  TEST_ASSERT_TRUE_MESSAGE(s.endsWith("..."), "el recorte tiene que terminar en ...");
  TEST_ASSERT_TRUE_MESSAGE(tft.textWidth(s, 1) <= maxW, "el recorte se paso del ancho");
}

// --- wrapToWidth ------------------------------------------------------------

void test_un_texto_corto_ocupa_un_solo_renglon(void) {
  std::vector<String> l = TextUtils::wrapToWidth(tft, "hola", 20 * CHAR_W, 1, 2);
  TEST_ASSERT_EQUAL_INT(1, (int)l.size());
  TEST_ASSERT_EQUAL_STRING("hola", l[0].c_str());
}

// Con 10 caracteres de ancho, "uno dos tres cuatro" tiene que cortar por
// espacio y no por la mitad de una palabra.
void test_el_corte_es_por_palabra(void) {
  std::vector<String> l = TextUtils::wrapToWidth(tft, "uno dos tres cuatro",
                                                 10 * CHAR_W, 1, 3);
  TEST_ASSERT_EQUAL_INT(3, (int)l.size());
  TEST_ASSERT_EQUAL_STRING("uno dos", l[0].c_str());
  TEST_ASSERT_EQUAL_STRING("tres",    l[1].c_str());
  TEST_ASSERT_EQUAL_STRING("cuatro",  l[2].c_str());
}

void test_ningun_renglon_se_pasa_del_ancho(void) {
  const int maxW = 12 * CHAR_W;
  std::vector<String> l = TextUtils::wrapToWidth(tft,
      "Cayo el avion de carga en Ezeiza por el temporal", maxW, 1, 4);
  TEST_ASSERT_TRUE(l.size() > 1);
  for (unsigned i = 0; i < l.size(); i++) {
    TEST_ASSERT_TRUE_MESSAGE(tft.textWidth(l[i], 1) <= maxW, "un renglon se paso");
  }
}

// Un titular sin espacios (una URL, por ejemplo) no puede colgar el bucle ni
// devolver renglones vacios para siempre.
void test_una_palabra_mas_larga_que_el_renglon(void) {
  std::vector<String> l = TextUtils::wrapToWidth(tft,
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 8 * CHAR_W, 1, 3);
  TEST_ASSERT_EQUAL_INT(1, (int)l.size());
  TEST_ASSERT_TRUE(l[0].endsWith("..."));
}

// El tope de renglones es lo que evita que un titular largo se coma la pantalla.
void test_se_respeta_el_tope_de_renglones(void) {
  std::vector<String> l = TextUtils::wrapToWidth(tft,
      "uno dos tres cuatro cinco seis siete ocho nueve diez",
      10 * CHAR_W, 1, 2);
  TEST_ASSERT_EQUAL_INT(2, (int)l.size());
  TEST_ASSERT_TRUE_MESSAGE(l[1].endsWith("..."),
      "lo que no entra tiene que quedar marcado con ...");
}

// --- StrUtils::copyTrimmed --------------------------------------------------
// Es lo que llena los campos de AircraftState, que desde ARQ-3 son char fijos y
// no String. Un desborde acá no da una excepción: pisa el campo de al lado del
// struct y aparece como datos corruptos en cualquier otra parte.

void test_copia_normal_sin_espacios(void) {
  char dst[9];
  StrUtils::copyTrimmed("AAL123", dst, sizeof(dst));
  TEST_ASSERT_EQUAL_STRING("AAL123", dst);
}

// El caso real: OpenSky rellena el callsign a 8 caracteres con espacios.
void test_se_recorta_el_relleno_de_opensky(void) {
  char dst[9];
  StrUtils::copyTrimmed("AAL123  ", dst, sizeof(dst));
  TEST_ASSERT_EQUAL_STRING("AAL123", dst);

  StrUtils::copyTrimmed("  ARG1234", dst, sizeof(dst));
  TEST_ASSERT_EQUAL_STRING("ARG1234", dst);

  StrUtils::copyTrimmed("   LAN99  ", dst, sizeof(dst));
  TEST_ASSERT_EQUAL_STRING("LAN99", dst);
}

// Un callsign de 8 caracteres tiene que entrar entero en char[9].
void test_el_callsign_mas_largo_entra_justo(void) {
  char dst[9];
  StrUtils::copyTrimmed("ABCDEFGH", dst, sizeof(dst));
  TEST_ASSERT_EQUAL_STRING("ABCDEFGH", dst);
  TEST_ASSERT_EQUAL_INT(8, (int)strlen(dst));
}

// Lo que no entra se corta, nunca se desborda. El centinela de atrás detecta
// una escritura fuera de rango.
void test_lo_que_no_entra_se_corta(void) {
  struct { char dst[7]; char centinela[4]; } b;
  memcpy(b.centinela, "\xAA\xAA\xAA\xAA", 4);

  StrUtils::copyTrimmed("ABCDEFGHIJKLMNOP", b.dst, sizeof(b.dst));

  TEST_ASSERT_EQUAL_STRING("ABCDEF", b.dst);
  TEST_ASSERT_EQUAL_INT(6, (int)strlen(b.dst));
  TEST_ASSERT_EQUAL_MEMORY("\xAA\xAA\xAA\xAA", b.centinela, 4);
}

void test_entrada_vacia_nula_o_toda_espacios(void) {
  char dst[9];
  StrUtils::copyTrimmed("", dst, sizeof(dst));
  TEST_ASSERT_EQUAL_STRING("", dst);

  StrUtils::copyTrimmed(nullptr, dst, sizeof(dst));
  TEST_ASSERT_EQUAL_STRING("", dst);

  StrUtils::copyTrimmed("     ", dst, sizeof(dst));
  TEST_ASSERT_EQUAL_STRING("", dst);
}

// --- AircraftState::label ---------------------------------------------------
// Cuatro pantallas mostraban "callsign, y si no ICAO24" con la misma cuenta a
// mano; ahora la hace el struct.

void test_la_etiqueta_prefiere_el_callsign(void) {
  AircraftState a;
  StrUtils::copyTrimmed("E49406", a.icao24, sizeof(a.icao24));
  StrUtils::copyTrimmed("ARG1234", a.callsign, sizeof(a.callsign));
  TEST_ASSERT_EQUAL_STRING("ARG1234", a.label());
}

// Muchos aviones vienen sin callsign; ahí tiene que caer al ICAO24, que siempre
// está, en vez de dibujar un hueco.
void test_sin_callsign_la_etiqueta_es_el_icao(void) {
  AircraftState a;
  StrUtils::copyTrimmed("E49406", a.icao24, sizeof(a.icao24));
  StrUtils::copyTrimmed("   ", a.callsign, sizeof(a.callsign)); // solo relleno
  TEST_ASSERT_EQUAL_STRING("E49406", a.label());
}

// Un AircraftState recién declarado se lee sin explotar: los char van
// inicializados a cero en el struct justamente para esto.
void test_un_avion_recien_declarado_no_tiene_basura(void) {
  AircraftState a;
  TEST_ASSERT_EQUAL_STRING("", a.label());
}

// --- OpenSkyClient::nextBackoffMs -------------------------------------------
// El backoff del 429 (SEC-3). Se factorizo en una funcion estatica pura
// justamente para poder testear la aritmetica sin depender de millis() ni de
// una request real.

void test_backoff_arranca_en_el_piso_configurado(void) {
  TEST_ASSERT_EQUAL_UINT32(OPENSKY_BACKOFF_START_MS,
                           OpenSkyClient::nextBackoffMs(0));
}

void test_backoff_se_duplica_en_cada_429(void) {
  uint32_t b = OpenSkyClient::nextBackoffMs(0);
  TEST_ASSERT_EQUAL_UINT32(OPENSKY_BACKOFF_START_MS, b);

  b = OpenSkyClient::nextBackoffMs(b);
  TEST_ASSERT_EQUAL_UINT32(OPENSKY_BACKOFF_START_MS * 2, b);

  b = OpenSkyClient::nextBackoffMs(b);
  TEST_ASSERT_EQUAL_UINT32(OPENSKY_BACKOFF_START_MS * 4, b);
}

// El caso que importa: sin este techo, una racha larga de 429 duplicaria hasta
// desbordar el uint32_t, o en el mejor caso dejaria al radar esperando horas.
void test_backoff_no_pasa_el_techo(void) {
  TEST_ASSERT_EQUAL_UINT32(OPENSKY_BACKOFF_MAX_MS,
                           OpenSkyClient::nextBackoffMs(OPENSKY_BACKOFF_MAX_MS));

  // Un valor que al duplicarse se pasaria del techo tiene que quedar
  // exactamente en el techo, no en el doble.
  uint32_t cerca_del_techo = OPENSKY_BACKOFF_MAX_MS - 1000;
  TEST_ASSERT_EQUAL_UINT32(OPENSKY_BACKOFF_MAX_MS,
                           OpenSkyClient::nextBackoffMs(cerca_del_techo));
}

void setup() {
  delay(2000);
  UNITY_BEGIN();

  RUN_TEST(test_el_ascii_imprimible_pasa_intacto);
  RUN_TEST(test_las_vocales_acentuadas_pierden_el_acento);
  RUN_TEST(test_la_enie_y_la_cedilla);
  RUN_TEST(test_la_puntuacion_del_espaniol_y_las_comillas);
  RUN_TEST(test_lo_que_no_es_dibujable_se_descarta);

  RUN_TEST(test_una_cadena_ascii_no_se_toca);
  RUN_TEST(test_plegado_de_dos_bytes);
  RUN_TEST(test_signos_de_apertura_y_comillas_latinas);
  RUN_TEST(test_los_puntos_suspensivos_se_expanden);
  RUN_TEST(test_los_emojis_desaparecen_sin_dejar_rastro);
  RUN_TEST(test_una_secuencia_truncada_no_se_va_de_rango);
  RUN_TEST(test_un_byte_de_continuacion_suelto_se_ignora);
  RUN_TEST(test_la_cadena_vacia_no_rompe_nada);
  RUN_TEST(test_se_recortan_los_espacios_de_los_bordes);

  RUN_TEST(test_si_entra_no_se_toca);
  RUN_TEST(test_si_no_entra_se_corta_con_puntos);

  RUN_TEST(test_un_texto_corto_ocupa_un_solo_renglon);
  RUN_TEST(test_el_corte_es_por_palabra);
  RUN_TEST(test_ningun_renglon_se_pasa_del_ancho);
  RUN_TEST(test_una_palabra_mas_larga_que_el_renglon);
  RUN_TEST(test_se_respeta_el_tope_de_renglones);

  RUN_TEST(test_copia_normal_sin_espacios);
  RUN_TEST(test_se_recorta_el_relleno_de_opensky);
  RUN_TEST(test_el_callsign_mas_largo_entra_justo);
  RUN_TEST(test_lo_que_no_entra_se_corta);
  RUN_TEST(test_entrada_vacia_nula_o_toda_espacios);

  RUN_TEST(test_la_etiqueta_prefiere_el_callsign);
  RUN_TEST(test_sin_callsign_la_etiqueta_es_el_icao);
  RUN_TEST(test_un_avion_recien_declarado_no_tiene_basura);

  RUN_TEST(test_backoff_arranca_en_el_piso_configurado);
  RUN_TEST(test_backoff_se_duplica_en_cada_429);
  RUN_TEST(test_backoff_no_pasa_el_techo);

  UNITY_END();
}

void loop() {}
