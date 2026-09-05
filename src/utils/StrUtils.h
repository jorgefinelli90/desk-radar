#pragma once
#include <string.h>
#include <stddef.h>

// Helpers de cadenas C, sin dependencias.
//
// Va aparte de TextUtils a propósito: aquel arrastra <TFT_eSPI.h> porque mide
// texto contra una fuente, y OpenSkyClient no tiene por qué depender de la
// pantalla para parsear una respuesta HTTP.
namespace StrUtils {

// Copia un texto a un buffer de largo fijo, recortando los espacios de los
// bordes y sin pasarse nunca del tamaño: lo que no entra se corta.
//
// La usa OpenSkyClient para llenar los campos de AircraftState, que son char
// fijos y no String (ver el comentario del struct). El recorte no es cosmético:
// OpenSky manda el callsign rellenado a 8 caracteres con espacios ("AAL123  "),
// y sin sacarlos las etiquetas quedan descentradas y textWidth() mide de más.
inline void copyTrimmed(const char* src, char* dst, size_t cap) {
  if (!dst || cap == 0) return;
  dst[0] = '\0';
  if (!src) return;

  while (*src == ' ') src++;

  size_t n = strlen(src);
  while (n > 0 && src[n - 1] == ' ') n--;
  if (n >= cap) n = cap - 1;

  memcpy(dst, src, n);
  dst[n] = '\0';
}

} // namespace StrUtils
