# Flight Radar de escritorio (ESP32 WROOM/WROVER + TFT 2.4" táctil)

MVP de un radar de vuelos casero: muestra tráfico aéreo cerca de tu casa
(modo Radar), sobre la provincia de Buenos Aires (modo Mapa) o cerca de
aeropuertos elegidos (modo Aeropuertos), usando la API gratuita de
[OpenSky Network](https://opensky-network.org/).

Además tiene una sección **Más info** con los titulares del día en Argentina
([GNews](https://gnews.io/)) y el clima actual de tu ubicación
([Open-Meteo](https://open-meteo.com/)).

## Hardware

- ESP32 WROOM/WROVER (dual-core, placa dev estándar — **no** el LOLIN C3 Mini)
- Pantalla TFT 2.4" ILI9341, 240×320, con touch resistivo XPT2046

## Wiring (confirmado)

| Pin TFT/Touch | Función | GPIO ESP32 |
|---|---|---|
| CS | Chip Select TFT | 15 |
| RESET | Reset TFT | 4 |
| DC (RS) | Data/Command | 2 |
| SDI (MOSI) | Datos TFT | 23 |
| SCK | Clock TFT | 18 |
| LED | Backlight | 21 |
| SDO (MISO) | Datos TFT (lectura) | 19 |
| T_CS | Chip Select touch | 5 |
| T_IRQ | Interrupción touch | 17 (libre, no usado por ahora) |

T_CLK/T_DIN/T_DO comparten el mismo bus SPI que SCK/MOSI/MISO (18/23/19), tal
como está cableado. TFT_eSPI maneja el touch resistivo usando `TOUCH_CS`, así
que no hace falta una librería aparte.

⚠️ **Pines a evitar en este ESP32**: GPIO6-11 (flash interna, no usables como
GPIO), GPIO0/GPIO2/GPIO12 (afectan el modo de arranque si están en el estado
incorrecto al encender — GPIO2 ya está en uso como DC, cuidado si agregás algo
más ahí).

## Setup

1. Instalá [PlatformIO](https://platformio.org/) (extensión de VS Code o CLI).
2. Copiá `include/secrets.h.example` a `include/secrets.h` y completá tu
   WiFi. Las credenciales de OpenSky ya vienen cargadas (client id/secret
   generados en tu cuenta OpenSky → *My OpenSky* → *API Client*). Para la
   pantalla de noticias también hace falta una API key de GNews (ver
   [APIs usadas](#apis-usadas)); el clima no necesita ninguna.
3. Conectá el ESP32 y compilá/subí:
   ```bash
   pio run -t upload
   pio device monitor
   ```

## Cómo funciona

- **Al encender**: si es la primera vez, corre un wizard de calibración
  táctil (te pide tocar 3 cruces). Se guarda en la memoria NVS del ESP32,
  así que solo pasa una vez.
- **Pantalla Home**: cuatro botones grandes — "RADAR", "AEROPUERTOS",
  "MAPA BA" y "MÁS INFO". Tocás el que quieras y entra directo a esa pantalla
  (con fetch inmediato).
- **Modo Radar**: pantalla circular centrada en tu casa
  (`-34.5858006, -58.5917033`), radio 40 km, con barrido animado tipo radar
  de aviación: una línea que gira 360° cada 3,5 s dejando una estela que se
  desvanece detrás. Cuando el barrido pasa por encima de un avión, el blip
  destella y se agranda un instante, como si lo acabara de detectar (es solo
  visual: no dispara ningún fetch). Los anillos están rotulados con su
  distancia real (10/20/30/40 km), hay puntos cardinales N/E/S/O alrededor
  del círculo y una leyenda de color debajo. Refresca los datos cada 30s, y
  pasa a cada 5s automáticamente si detecta un avión a menos de 10 km. La
  animación es independiente del ciclo de fetch: el barrido sigue girando
  sobre la última posición conocida aunque no lleguen datos nuevos.
- **Detalle de avión**: tocá cualquier punto del radar y se abre su ficha
  (callsign, ICAO24, distancia, rumbo cardinal, altitud, velocidad,
  coordenadas). La zona tocable es más grande que el punto dibujado, así que
  no hace falta puntería. La ficha queda congelada mientras la mirás; tocás
  de nuevo en cualquier lado y volvés al radar.
- **Modo Aeropuertos**: lista de tráfico con altitud baja (<3000m) cerca de
  Ezeiza (SAEZ), Aeroparque (SABE) o El Palomar (SADP). Un botón en la parte
  inferior ("Siguiente aeropuerto >") rota entre los 3.
- **Más info**: sub-menú con dos botones táctiles, "NOTICIAS" y "CLIMA".
- **Pantalla Noticias**: los 5 titulares más recientes de Argentina (GNews,
  `country=ar&lang=es`). Cada fila muestra el titular partido en hasta dos
  renglones, más el medio y la hora de publicación en horario argentino. Los
  titulares llegan en UTF-8 con tildes y eñes, que las fuentes embebidas de
  TFT_eSPI no tienen, así que se pliegan a ASCII antes de dibujarlos
  (`TextUtils::toAscii`); lo que no entra a lo ancho se corta con "...".
- **Pantalla Clima**: temperatura, sensación térmica, condición, humedad y
  viento para `HOME_LAT`/`HOME_LON`, con un ícono dibujado a mano con
  primitivas de TFT_eSPI (sol, nubes, lluvia, nieve, niebla, tormenta) según
  el código WMO que devuelve Open-Meteo. No se descarga ninguna imagen.
- **Cache**: noticias y clima se guardan en RAM y no se vuelven a pedir
  mientras el cache siga vigente (20 y 15 minutos respectivamente, en
  `config.h`). Si una request falla, se reintenta recién al minuto en vez de
  martillar la API. Con eso el free tier de GNews (100 requests/día) alcanza
  de sobra aunque dejes la pantalla puesta todo el día.
- **Volver a Home**: en cualquier pantalla, tocá la franja superior (donde
  dice "< HOME") para volver al menú principal.

## Estructura del proyecto

```
flight-radar-esp32/
├── platformio.ini          # config de build, pines TFT, dependencias
├── include/
│   ├── config.h             # ubicación, aeropuertos, radios, tiempos
│   ├── secrets.h.example    # plantilla de credenciales
│   └── secrets.h            # tus credenciales reales (gitignored)
├── src/
│   ├── main.cpp                # loop principal, navegación táctil, refresco adaptativo
│   ├── OpenSkyClient.{h,cpp}   # OAuth2 + fetch de /states/all
│   ├── GeoUtils.h              # haversine, bearing, bounding box
│   ├── DisplayManager.{h,cpp}  # init TFT + status bar
│   ├── TouchManager.{h,cpp}    # calibración táctil persistente + detección de tap
│   ├── HomeScreen.{h,cpp}      # menú principal con los cuatro botones
│   ├── RadarScreen.{h,cpp}     # radar circular + hit-test de aviones
│   ├── MapScreen.{h,cpp}       # mapa de la provincia de Buenos Aires
│   ├── DetailScreen.{h,cpp}    # ficha de un avión tocado en el radar
│   ├── AirportScreen.{h,cpp}   # dibujo de la lista por aeropuerto
│   ├── InfoMenuScreen.{h,cpp}  # sub-menú "MÁS INFO" (Noticias / Clima)
│   ├── NewsClient.{h,cpp}      # fetch de titulares en GNews + cache
│   ├── NewsScreen.{h,cpp}      # lista de titulares
│   ├── WeatherClient.{h,cpp}   # fetch de clima en Open-Meteo + cache
│   ├── WeatherScreen.{h,cpp}   # clima actual + íconos dibujados a mano
│   ├── TextUtils.h             # UTF-8 → ASCII, recorte y wrap por ancho
│   └── UiRect.h                # helper de hit-testing para zonas táctiles
└── README.md
```

## APIs usadas

| Pantalla | API | ¿API key? | Free tier |
|---|---|---|---|
| Radar / Mapa / Aeropuertos | [OpenSky Network](https://opensky-network.org/) | OAuth2 client id + secret | gratis para uso personal |
| Noticias | [GNews.io](https://gnews.io/) | sí (`GNEWS_API_KEY`) | 100 requests/día, **sin tarjeta de crédito** |
| Clima | [Open-Meteo](https://open-meteo.com/) | **no hace falta** | libre para uso no comercial |

### Cómo conseguir la API key de GNews (gratis)

1. Entrá a <https://gnews.io/register> y creá una cuenta con tu mail. **No
   pide tarjeta de crédito.**
2. Confirmá el mail y entrá al dashboard: la API key aparece ahí directamente.
3. Pegala en `include/secrets.h`:
   ```c
   #define GNEWS_API_KEY "tu_api_key_aca"
   ```

Detalles del plan gratuito que conviene tener en cuenta:

- 100 requests por día (el contador se resetea a las 00:00 UTC) y máximo 1
  request por segundo. Con el cache de 20 minutos gastás ~72 por día como
  mucho.
- Los titulares del free tier llegan con **unas 12 horas de demora** respecto
  a la publicación real. Para una pantalla de escritorio no molesta, pero si
  querés noticias al minuto hay que pasar a un plan pago.
- El plan gratuito es solo para proyectos no comerciales.

Si se agota el cupo diario la pantalla muestra "Cupo diario agotado" en vez de
quedarse en blanco, y reintenta al minuto siguiente.

### Por qué Open-Meteo y no OpenWeatherMap

OpenWeatherMap hoy empuja a *One Call 3.0*, que exige dejar una tarjeta de
crédito registrada aunque no llegues a pasarte del tramo gratuito.
[Open-Meteo](https://open-meteo.com/) no pide nada: ni tarjeta, ni registro,
ni API key para uso no comercial — le pegás directo a
`https://api.open-meteo.com/v1/forecast` con la lat/lon y listo. Para este
proyecto eso significa un secreto menos que manejar.

Devuelve además la condición como
[código WMO](https://open-meteo.com/en/docs) (0 = despejado, 3 = nublado,
61-65 = lluvia, 95-99 = tormenta, etc.), que es lo que
`WeatherClient::describe()` traduce a ícono + texto en español. Con
`timezone=auto` la hora de la medición ya viene en horario local, así que la
pantalla la muestra sin necesidad de NTP ni RTC.

## Notas de implementación

### Radar: por qué un sprite parcial a 4 bits

El barrido gira continuamente (una vuelta cada `RADAR_SWEEP_PERIOD_MS`, 3,5 s
por defecto) con una estela de 7 bandas de brillo decreciente. Para eso hay que
repintar el disco entero ~22 veces por segundo, y hacerlo directo sobre el TFT
parpadea. Las opciones y por qué se eligió la que se eligió:

| Enfoque | RAM | Veredicto |
|---|---|---|
| Sprite de pantalla completa, 16 bpp | 240×320×2 = **150 KB** | descartado, no entra |
| Sprite del disco, 16 bpp | 200×200×2 = **80 KB** | descartado: convive mal con el handshake TLS de OpenSky (~45 KB de pico) |
| **Sprite del disco, 4 bpp + paleta** | (200×200)/2 = **20 KB** | **elegido** |
| Redibujo directo de la porción que cambia | 0 | fallback |

El sprite cubre **solo el disco** (200×200 px pegado en 20,18), no la pantalla
entera: la barra de estado, la leyenda y el panel inferior son estáticos y se
dibujan una sola vez, así que meterlos en el buffer sería pagar RAM y bus SPI
por píxeles que no cambian.

Los 4 bpp alcanzan de sobra porque el radar usa 16 colores contados: fondo,
anillos, cruz, 7 verdes de estela, borde de ataque, blip normal, blip en
alerta, destello, casa y etiquetas. Ojo con una particularidad de TFT_eSPI: en
sprites de 4 bpp **el "color" que reciben las primitivas es el índice de la
paleta** (`color & 0x0F`), no un RGB565 — de ahí el `enum` de índices en
`RadarScreen.h` y el array `RADAR_PALETTE`.

Si `createSprite()` fallara por falta de RAM, `RadarScreen` no se rompe: cae
solo a `drawDiscDirect()`, que anima con una sola línea sin estela borrando la
anterior y re-estampando los anillos que tapaba. Se ve peor, pero funciona.

### Radar: por qué el loop usa 5 ms en vez de 30

`tick()` avanza el barrido y vuelve enseguida si todavía no toca frame, así que
nunca bloquea el polling del touch. Pero el pacing es más sutil de lo que
parece:

- `TFT_eSPI::getTouch()` llama 5 veces a `validTouch()`, y cada una hace un
  `delay(1)` **antes** de comprobar la presión. O sea que leer el touch cuesta
  ~5 ms por vuelta aunque nadie toque la pantalla.
- Volcar el sprite cuesta ~20 ms (200×200 px × 16 bits a 40 MHz ≈ 16 ms de SPI,
  más la conversión de paleta a RGB565).

Con el `delay(30)` general, la vuelta del loop daba ~55 ms y los frames caían
cada ~80 ms (12 fps, 8° de salto por frame): se veía a los tirones. Por eso el
radar usa `RADAR_LOOP_DELAY_MS = 5`: el ritmo lo marca `RADAR_FRAME_MS` (45 ms
→ ~22 fps, ~4,6° por frame) y el touch se consulta **más** seguido, no menos.

El avance angular se calcula con el `dt` real, no con un incremento fijo, así
que la vuelta sigue tardando 3,5 s aunque un frame se retrase por un fetch. El
salto se topa en 500 ms para que al volver de una request lenta el barrido no
pegue un tirón.

Para medir esto en tu placa, poné `RADAR_DEBUG_TIMING` en 1 en `config.h`: cada
2 s salen por serie los fps reales, el costo del frame y los grados por frame.

### ⚠️ Leer el body con `getString()`, nunca con `getStream()`

Las tres APIs (OpenSky, GNews y Open-Meteo) responden con
`Transfer-Encoding: chunked`. En el `HTTPClient` del core de ESP32,
`getStream()` devuelve el socket TCP **crudo**:

```cpp
WiFiClient& HTTPClient::getStream(void) {
    if (connected()) return *_client;   // sin desarmar el chunked
    ...
}
```

Los headers de chunk (el tamaño en hexa + `\r\n`) viajan mezclados dentro del
cuerpo, así que `deserializeJson(doc, http.getStream())` recibe algo como
`16c5\r\n{"latitude":...` y falla en el primer byte con `InvalidInput`. El
único que interpreta el chunked es `writeToStream()`, que es lo que usa
`getString()`.

Esto ya nos costó una tarde: el token de OpenSky funcionaba (usa `getString()`)
pero `/states/all` no devolvía ningún avión, y las pantallas de noticias y
clima mostraban "respuesta ilegible". **Si agregás otra API, leé el body con
`getString()` y parseá desde el `String`.**

Los payloads de este proyecto son chicos (Open-Meteo ~400 B, OpenSky ~1-30 KB,
GNews ~6 KB), así que tener el cuerpo entero en RAM no es problema. Si algún
día hiciera falta parsear en streaming, la alternativa es `http.useHTTP10(true)`
(HTTP/1.0 no admite chunked), pero entonces el fin del cuerpo depende del
cierre de conexión en vez de un terminador explícito.

Cuando el parseo falla, el motivo concreto de ArduinoJson (`InvalidInput`,
`IncompleteInput`, `NoMemory`) se muestra en pantalla y los primeros 120 bytes
del payload salen por el monitor serie.

## Próximos pasos (fuera del MVP)

- Traer datos de ruta (origen/destino) del avión seleccionado con el endpoint
  `/flights/aircraft` de OpenSky, para enriquecer la ficha de detalle.
- Usar T_IRQ (GPIO17) por interrupción en vez de polling, para ahorrar CPU.
- Usar el NeoPixel para alertar visualmente cuando hay tráfico muy cerca.
- Cachear también los resultados de OpenSky, para no gastar cupo si dos modos
  piden zonas solapadas (Noticias y Clima ya cachean).
- Pronóstico extendido en la pantalla de Clima: Open-Meteo devuelve los
  próximos días en la misma request, solo falta dibujarlos.
- Manejo de errores de red más robusto (reintentos con backoff).
