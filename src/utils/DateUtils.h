#pragma once

// Utilidades de fecha, puras y sin tocar time.h: el dia de la semana de una
// fecha calendario no depende de la hora del sistema ni de NTP, asi que no
// hace falta arrastrar esa dependencia (ni sus casos borde de zona horaria)
// para algo que es aritmetica pura. Eso de paso lo hace testeable sin reloj.
namespace DateUtils {

// Dia de semana de una fecha gregoriana (algoritmo de Zeller). 0=Domingo,
// 1=Lunes, ..., 6=Sabado. Lo usa WeatherClient para ponerle nombre a los dias
// del pronostico extendido: Open-Meteo devuelve la fecha ("2026-09-05") pero
// no el dia de la semana.
inline int dayOfWeek(int year, int month, int day) {
  if (month < 3) { month += 12; year -= 1; }
  int K = year % 100;
  int J = year / 100;
  int h = (day + (13 * (month + 1)) / 5 + K + K / 4 + J / 4 + 5 * J) % 7;
  return (h + 6) % 7; // el h crudo de Zeller es "0=Sabado"; esto lo pasa a "0=Domingo"
}

static const char* const WEEKDAY_ABBR[7] = {
  "Dom", "Lun", "Mar", "Mie", "Jue", "Vie", "Sab"
};

} // namespace DateUtils
