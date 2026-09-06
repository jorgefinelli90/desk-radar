#include "screens/WeatherForecastScreen.h"
#include "screens/WeatherScreen.h"   // WeatherScreen::tempColor: misma paleta
#include "utils/TextUtils.h"
#include <math.h>

// Sin íconos graficos a proposito: los que dibuja WeatherScreen (drawSun,
// drawCloud, etc.) tienen las proporciones ajustadas a mano para el icono
// grande de la pantalla de "ahora", y encogerlos a lo que entraria en una fila
// de esta lista los deja amontonados e ilegibles. El texto de la condicion
// (igual que ya usa la pantalla de "ahora") dice lo mismo sin ese problema.
void WeatherForecastScreen::render(const WeatherNow& now) {
  TFT_eSPI& tft = _display.tft();
  tft.fillScreen(TFT_BLACK);

  _display.showStatusBar("< VOLVER  PRONOSTICO", "");

  if (now.dailyCount == 0) {
    // Puede pasar si Open-Meteo cambia el formato del bloque "daily" o si el
    // ultimo refresh fue de antes de esta funcion: no es un error nuevo, el
    // clima de "ahora" sigue andando igual.
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_SILVER, TFT_BLACK);
    tft.drawString("Sin pronostico extendido", tft.width() / 2, tft.height() / 2, 2);
    return;
  }

  const int top = 22;
  const int rowH = 54; // 5 dias * 54 = 270px, entra comodo en los 320 de alto
  int y = top;

  for (int i = 0; i < now.dailyCount; i++) {
    const WeatherNow::Day& d = now.daily[i];

    tft.drawFastHLine(10, y, tft.width() - 20, TFT_DARKGREEN);

    // Dia, arriba a la izquierda
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(d.label, 14, y + 6, 2);

    // Temperaturas, arriba a la derecha. Coloreadas por el maximo del dia,
    // la misma paleta que usa el numero grande de la pantalla de "ahora".
    char temps[16];
    snprintf(temps, sizeof(temps), "%d/%dC",
             (int)lround(d.tempMaxC), (int)lround(d.tempMinC));
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(WeatherScreen::tempColor(d.tempMaxC), TFT_BLACK);
    tft.drawString(temps, tft.width() - 14, y + 6, 2);

    // Condicion, renglon de abajo. Se achica con "..." si no entra: hay
    // descripciones largas ("Tormenta con granizo") que casi llenan el ancho.
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_SILVER, TFT_BLACK);
    tft.drawString(TextUtils::fitToWidth(tft, d.description, tft.width() - 28, 1),
                   14, y + 30, 1);

    y += rowH;
  }

  tft.drawFastHLine(10, y, tft.width() - 20, TFT_DARKGREEN);

  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Toca para volver", tft.width() / 2, tft.height() - 5, 1);
}
