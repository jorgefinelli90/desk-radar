#pragma once
#include <Arduino.h>
#include <vector>
#include <TFT_eSPI.h>

// Helpers de texto para dibujar contenido que viene de APIs externas.
//
// Las fuentes embebidas de TFT_eSPI (GLCD, FONT2, FONT4) solo traen los
// caracteres ASCII 32-126. Los titulares y descripciones llegan en UTF-8 con
// tildes, eñes y comillas tipográficas, que se dibujarían como basura o como
// huecos. Por eso todo texto externo pasa primero por toAscii().
namespace TextUtils {

// Mapea un codepoint Unicode a su equivalente ASCII más cercano.
// Devuelve 0 si el carácter se debe descartar.
inline char foldCodepoint(uint32_t cp) {
  if (cp >= 32 && cp < 127) return (char)cp;

  // Latin-1 suplementario: vocales acentuadas, ñ, ç
  if (cp >= 0xC0 && cp <= 0xC5) return 'A';
  if (cp == 0xC7)               return 'C';
  if (cp >= 0xC8 && cp <= 0xCB) return 'E';
  if (cp >= 0xCC && cp <= 0xCF) return 'I';
  if (cp == 0xD1)               return 'N';
  if ((cp >= 0xD2 && cp <= 0xD6) || cp == 0xD8) return 'O';
  if (cp >= 0xD9 && cp <= 0xDC) return 'U';
  if (cp == 0xDD)               return 'Y';
  if (cp >= 0xE0 && cp <= 0xE5) return 'a';
  if (cp == 0xE7)               return 'c';
  if (cp >= 0xE8 && cp <= 0xEB) return 'e';
  if (cp >= 0xEC && cp <= 0xEF) return 'i';
  if (cp == 0xF1)               return 'n';
  if ((cp >= 0xF2 && cp <= 0xF6) || cp == 0xF8) return 'o';
  if (cp >= 0xF9 && cp <= 0xFC) return 'u';
  if (cp == 0xFD || cp == 0xFF) return 'y';

  // Signos de puntuación frecuentes en titulares en español
  if (cp == 0xA0) return ' ';   // espacio duro
  if (cp == 0xA1) return '!';   // ¡
  if (cp == 0xBF) return '?';   // ¿
  if (cp == 0xAB || cp == 0xBB) return '"'; // « »
  if (cp == 0x2018 || cp == 0x2019) return '\''; // ' '
  if (cp == 0x201C || cp == 0x201D) return '"';  // " "
  if (cp == 0x2013 || cp == 0x2014) return '-';  // – —

  return 0; // cualquier otra cosa (emoji, símbolos raros) se descarta
}

// Convierte una cadena UTF-8 a ASCII imprimible, plegando acentos.
inline String toAscii(const String& in) {
  String out;
  out.reserve(in.length());

  unsigned int i = 0;
  while (i < in.length()) {
    uint8_t b = (uint8_t)in[i];
    uint32_t cp;
    int len;

    if (b < 0x80)        { cp = b;          len = 1; }
    else if ((b & 0xE0) == 0xC0) { cp = b & 0x1F; len = 2; }
    else if ((b & 0xF0) == 0xE0) { cp = b & 0x0F; len = 3; }
    else if ((b & 0xF8) == 0xF0) { cp = b & 0x07; len = 4; }
    else                 { i++; continue; } // byte de continuación suelto

    if (i + len > in.length()) break; // secuencia truncada al final
    for (int k = 1; k < len; k++) {
      cp = (cp << 6) | ((uint8_t)in[i + k] & 0x3F);
    }
    i += len;

    if (cp == 0x2026) { out += "..."; continue; } // …
    char c = foldCodepoint(cp);
    if (c) out += c;
  }

  out.trim();
  return out;
}

// Recorta el texto para que entre en maxW píxeles, agregando "..." al final.
// Acumula el ancho carácter por carácter: TFT_eSPI no aplica kerning, así que
// la suma es exacta y evita medir la cadena entera N veces.
inline String fitToWidth(TFT_eSPI& tft, const String& text, int maxW, uint8_t font) {
  if (tft.textWidth(text, font) <= maxW) return text;

  int budget = maxW - tft.textWidth("...", font);
  if (budget <= 0) return String("...");

  int w = 0;
  unsigned int cut = 0;
  for (unsigned int i = 0; i < text.length(); i++) {
    char s[2] = { text[i], 0 };
    w += tft.textWidth(s, font);
    if (w > budget) break;
    cut = i + 1;
  }

  String out = text.substring(0, cut);
  out.trim();
  return out + "...";
}

// Parte el texto en hasta maxLines renglones de maxW píxeles, cortando por
// palabra. Si no entra todo, el último renglón termina en "...".
inline std::vector<String> wrapToWidth(TFT_eSPI& tft, const String& text,
                                       int maxW, uint8_t font, int maxLines) {
  std::vector<String> lines;
  String rest = text;
  rest.trim();

  while (rest.length() > 0 && (int)lines.size() < maxLines) {
    if (tft.textWidth(rest, font) <= maxW) {
      lines.push_back(rest);
      break;
    }

    // Último renglón disponible: lo que sobre se recorta con puntos suspensivos
    if ((int)lines.size() == maxLines - 1) {
      lines.push_back(fitToWidth(tft, rest, maxW, font));
      break;
    }

    // Buscar el último espacio que todavía entra en el ancho
    int w = 0;
    int cut = -1;
    for (unsigned int i = 0; i < rest.length(); i++) {
      char s[2] = { rest[i], 0 };
      w += tft.textWidth(s, font);
      if (w > maxW) break;
      if (rest[i] == ' ') cut = (int)i;
    }

    if (cut <= 0) { // palabra sola más larga que el renglón
      lines.push_back(fitToWidth(tft, rest, maxW, font));
      break;
    }

    String line = rest.substring(0, cut);
    line.trim();
    lines.push_back(line);
    rest = rest.substring(cut + 1);
    rest.trim();
  }

  return lines;
}

} // namespace TextUtils
