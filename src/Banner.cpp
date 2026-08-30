#include "Banner.h"
#include "TextUtils.h"
#include "config.h"

void Banner::show(const String& text) {
  // Los mensajes llegan del navegador en UTF-8; las fuentes de TFT_eSPI solo
  // tienen ASCII 32-126, así que hay que plegarlos igual que los titulares.
  _text = TextUtils::toAscii(text);
  if (_text.length() == 0) return;

  _phase = Phase::SlideIn;
  _y = -HEIGHT;
  _lastY = -HEIGHT;
  _phaseStart = millis();
  _lastFrameMs = 0; // fuerza un frame inmediato
}

bool Banner::takeRepaintRequest() {
  if (!_repaintPending) return false;
  _repaintPending = false;
  return true;
}

void Banner::tick() {
  if (_phase == Phase::Idle) return;

  uint32_t now = millis();
  if (now - _lastFrameMs < BANNER_FRAME_MS) return;
  _lastFrameMs = now;

  uint32_t elapsed = now - _phaseStart;

  switch (_phase) {
    case Phase::SlideIn: {
      float t = (float)elapsed / BANNER_SLIDE_MS;
      if (t >= 1.0f) {
        t = 1.0f;
        _phase = Phase::Hold;
        _phaseStart = now;
      }
      // Ease-out: arranca rápido y frena al final. Un desplazamiento lineal se
      // ve mecánico.
      float e = 1.0f - (1.0f - t) * (1.0f - t);
      _y = (int)(-HEIGHT + e * HEIGHT);
      break;
    }

    case Phase::Hold:
      _y = 0;
      if (elapsed >= BANNER_HOLD_MS) {
        _phase = Phase::SlideOut;
        _phaseStart = now;
      }
      break;

    case Phase::SlideOut: {
      float t = (float)elapsed / BANNER_SLIDE_MS;
      if (t >= 1.0f) {
        // Terminó: pedimos que se repinte la pantalla de abajo. Limpiar la
        // franja acá sería al pedo, el repintado la cubre.
        _phase = Phase::Idle;
        _repaintPending = true;
        return;
      }
      float e = t * t; // ease-in, el espejo del ease-out de la entrada
      _y = (int)(-e * HEIGHT);
      break;
    }

    default:
      return;
  }

  draw();
}

void Banner::draw() {
  TFT_eSPI& tft = _display.tft();

  // Al subir, el banner deja al descubierto una franja abajo que ya no le
  // corresponde. Se pinta de negro: el fondo real vuelve al terminar, con el
  // repintado completo.
  if (_y < _lastY) {
    int exposedTop = _y + HEIGHT;
    int exposedH = _lastY - _y;
    if (exposedTop < tft.height() && exposedH > 0) {
      tft.fillRect(0, exposedTop, tft.width(), exposedH, TFT_BLACK);
    }
  }
  _lastY = _y;

  // TFT_eSPI recorta solo lo que se sale de la pantalla, así que se puede
  // dibujar con _y negativo sin cuentas extra.
  tft.fillRoundRect(4, _y + 3, tft.width() - 8, HEIGHT - 6, 10, 0x0A4A);
  tft.drawRoundRect(4, _y + 3, tft.width() - 8, HEIGHT - 6, 10, TFT_CYAN);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_CYAN, 0x0A4A);
  tft.drawString("MENSAJE", 14, _y + 9, 1);

  // Hasta 2 renglones; lo que no entra se corta con "..."
  std::vector<String> lines =
      TextUtils::wrapToWidth(tft, _text, tft.width() - 28, 2, 2);

  tft.setTextColor(TFT_WHITE, 0x0A4A);
  int ty = _y + 22;
  for (const auto& line : lines) {
    tft.drawString(line, 14, ty, 2);
    ty += 18;
  }
}
