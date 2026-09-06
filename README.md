# Flight Radar de escritorio (ESP32 WROOM/WROVER + TFT 2.4" táctil)

MVP de un radar de vuelos casero: muestra tráfico aéreo cerca de tu casa
(modo Radar), sobre un mapa realista de tu zona (modo Mapa) o cerca de
aeropuertos elegidos (modo Aeropuertos), usando la API gratuita de
[OpenSky Network](https://opensky-network.org/).

Además tiene una sección **Más info** con los titulares del día en Argentina
([GNews](https://gnews.io/)), el clima actual de tu ubicación
([Open-Meteo](https://open-meteo.com/)) y la posición en vivo de la Estación
Espacial Internacional ([wheretheiss.at](https://wheretheiss.at/)).

## Características

- 📡 **Radar circular** en tiempo real sobre un mapa real de tu zona, con
  barrido animado y alerta de tráfico cercano.
- 🗺️ **Mapa realista** con los aviones coloreados por altitud, zoom 80/40 km.
- ✈️ **Ficha de detalle** por avión: distancia, rumbo, altitud, velocidad y
  **ruta estimada** (origen → destino) vía OpenSky.
- 🛬 **Modo Aeropuertos**: tráfico bajo cerca de Ezeiza, Aeroparque o El Palomar.
- 📰🌤️ **Noticias y clima** de tu zona (GNews + Open-Meteo), con **pronóstico
  extendido** de hasta 5 días.
- 🛰️ **Posición de la ISS en vivo**: distancia y rumbo desde tu casa, altura
  orbital, velocidad y si está a la luz del sol o en la sombra de la Tierra.
- 🌐 **Se configura entero desde el navegador**, sin recompilar ni tocar
  código: WiFi y API keys se cargan por un portal cautivo la primera vez.
- 🔄 **Se actualiza por WiFi (OTA)**: subís un `firmware.bin` nuevo desde
  `desk-radar.local/update` y el dispositivo se reinicia solo, sin cable —
  ver [Actualizar por WiFi](#actualizar-por-wifi-ota).
- 🔒 Panel web opcionalmente protegido con PIN, TLS validado contra las cuatro
  APIs, y 61 tests automáticos.

Ver [Configuración inicial](#configuración-inicial) para arrancar.

## Índice

- [Características](#características)
- [Hardware](#hardware)
- [Wiring (confirmado)](#wiring-confirmado)
- [Setup](#setup)
- [Configuración inicial](#configuración-inicial)
  - [Primer arranque: portal cautivo](#1-primer-arranque-portal-cautivo)
  - [Después: desk-radar.local](#2-después-desk-radarlocal)
  - [Mandar un mensaje a la pantalla](#3-mandar-un-mensaje-a-la-pantalla)
  - [PIN del panel web](#pin-del-panel-web)
  - [Resetear la configuración](#4-resetear-la-configuración)
- [Cómo funciona](#cómo-funciona)
- [Estructura actual del proyecto](#estructura-actual-del-proyecto)
- [Mapa pre-renderizado](#mapa-pre-renderizado)
- [APIs usadas](#apis-usadas)
- [Notas de implementación](#notas-de-implementación)
- [Actualizar por WiFi (OTA)](#actualizar-por-wifi-ota)
- [Próximos pasos](#próximos-pasos-fuera-del-mvp)
- [Licencia](#licencia)

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

> **No hace falta editar código para configurar el dispositivo.** WiFi y API
> keys se cargan desde el navegador y se guardan en NVS. Ver
> [Configuración inicial](#configuración-inicial).

1. Instalá [PlatformIO](https://platformio.org/) (extensión de VS Code o CLI).
2. **Poné tu ubicación.** El repo trae la del autor a modo de ejemplo: abrí
   `include/config.h` y cambiá `HOME_LAT`/`HOME_LON` por tu latitud/longitud
   (Google Maps: click derecho sobre tu casa → copiar coordenadas). Es el
   único dato que tenés que tocar antes de compilar.
3. *(Opcional, solo para desarrollar)* Copiá `include/secrets.h.example` a
   `include/secrets.h` y completá tus datos. Si ese archivo existe, sus valores
   se precargan en NVS **una sola vez** en el primer arranque, para no tener que
   pasar por el portal cada vez que reflasheás. El dispositivo final no lo
   necesita para nada.
4. Generá el mapa del modo Mapa, ya centrado en tu ubicación (una sola vez, o
   cada vez que la cambies):
   ```bash
   cd tools && npm install && node build-map.mjs && cd ..
   ```
5. Conectá el ESP32 y subí el mapa y el firmware:
   ```bash
   pio run -t uploadfs   # los mapas (data/*.bin) -> particion spiffs
   pio run -t upload     # el firmware
   pio device monitor
   ```
   El `uploadfs` va una sola vez; después alcanza con `upload` salvo que
   regeneres el mapa.
6. Si la pantalla te queda cabeza abajo (depende de cómo montes el módulo),
   cambiá `SCREEN_ROTATION` en `include/config.h`: `0` es vertical con el
   conector abajo y `2` es la misma vertical girada 180°. Al arrancar con un
   valor distinto al que ya estaba, el wizard de calibración táctil se vuelve
   a correr solo (la calibración vieja no sirve para la orientación nueva).

## Configuración inicial

El dispositivo no tiene ninguna credencial compilada adentro: todo vive en NVS
y se carga desde el navegador.

### 1. Primer arranque: portal cautivo

Si no hay WiFi guardado, el ESP32 levanta su propio Access Point y muestra en la
pantalla a qué red conectarte:

```
        CONFIGURACION
   Conectate con el celular
        a esta red WiFi:

      DeskRadar-Setup
       Clave: radar1234

  Se abre solo el navegador.
        Si no, entra a:
         192.168.4.1
```

Conectás el celular a **DeskRadar-Setup** (clave `radar1234`) y el sistema
operativo abre solo el navegador con el formulario — es un portal cautivo real:
un DNS comodín resuelve cualquier dominio a la IP del ESP32, y las URLs que
Android/iOS/Windows usan para detectar internet (`/generate_204`,
`/hotspot-detect.html`, `/ncsi.txt`, …) devuelven un redirect. Si tu celular no
lo abre solo, entrá a `http://192.168.4.1/`.

El formulario pide:

| Campo | Para qué |
|---|---|
| Red WiFi + Contraseña | la red de casa, 2,4 GHz |
| OpenSky Client ID + Secret | tráfico aéreo ([cómo obtenerlos](#apis-usadas)) |
| GNews API key | titulares (opcional) |

Al guardar, se persiste en NVS y el ESP32 se reinicia y se conecta solo.

### 2. Después: `desk-radar.local`

Ya conectado a tu red, el dispositivo publica mDNS y queda en
**http://desk-radar.local/** (también funciona la IP, que se ve en la pantalla
AJUSTES).

| Ruta | Qué hace |
|---|---|
| `GET /` | estado: red, IP, señal, uptime, último fetch de OpenSky, heap libre |
| `GET /config` | el mismo formulario, para cambiar cualquier cosa sin volver al AP |
| `POST /config` | guarda. Si cambió el WiFi, avisa y reinicia |
| `GET /message` | textarea para mandar un mensaje a la pantalla |
| `POST /api/message` | recibe el texto (`text=...`), responde `{"ok":true}` |

Los campos de tipo contraseña nunca se mandan al navegador: si ya hay uno
guardado se ve el placeholder *(guardado - dejar vacio para no cambiar)*, y
dejarlo vacío no lo borra.

### 3. Mandar un mensaje a la pantalla

Desde el navegador, en `http://desk-radar.local/message`: escribís, apretás
enviar y aparece un toast *Enviado*.

Desde la terminal:

```bash
curl -X POST http://desk-radar.local/api/message \
     -d "text=Hola! Sali a mirar, esta pasando un avion bajo"
```

El mensaje entra en la pantalla como un banner que baja deslizándose, se queda
unos 5,5 segundos y se retira, **encima de la pantalla que estuviera activa**
(Home, Radar, Mapa, la que sea). Al terminar, esa pantalla se repinta limpia.

### PIN del panel web

El panel (`desk-radar.local`) arranca **abierto**: cualquiera en tu red puede
verlo y cambiar la configuración. La página de estado lo avisa en grande
mientras siga así.

Para cerrarlo, cargá un **PIN del panel web** en Configuración. A partir de ahí
el navegador pide usuario y clave:

| | |
|---|---|
| Usuario | `admin` |
| Clave | el PIN que pusiste |

Se usa autenticación **Digest** y no Basic: esto va por HTTP plano, y con Basic
el PIN viajaría en cada request en base64, que es texto legible para cualquiera
que mire la red.

Dos formas de sacarlo:

- Desde el propio panel, escribiendo `off` en el campo del PIN. Hace falta
  porque un campo secreto vacío significa "no lo cambies", así que sin una
  salida explícita, una vez puesto no habría forma de volver atrás.
- Con el botón **Reiniciar configuración** de la pantalla de Ajustes, que borra
  WiFi y PIN. Es la vía si te lo olvidaste: exige estar parado frente al aparato
  y confirmar con dos toques.

### 4. Resetear la configuración

En el dispositivo: **AJUSTES → "Reiniciar config WiFi"**. Pide un segundo toque
para confirmar (un toque accidental no puede dejarte sin red), borra las
credenciales de NVS y reinicia en modo AP.

Para probar el portal sin borrar nada, poné `FORCE_SETUP_PORTAL` en `1` en
`include/config.h`: entra al AP aunque haya WiFi guardado. Acordate de volverlo
a `0`.

## Cómo funciona

- **Al encender**: si es la primera vez, corre un wizard de calibración
  táctil (te pide tocar 3 cruces). Se guarda en la memoria NVS del ESP32,
  así que solo pasa una vez (y se repite si cambiás `SCREEN_ROTATION`).
- **Pantalla Home**: cinco botones grandes — "RADAR", "AEROPUERTOS", "MAPA",
  "MÁS INFO" y "AJUSTES". Tocás el que quieras y entra directo a esa pantalla
  (con fetch inmediato).
- **Pantalla Ajustes**: muestra la red conectada, la IP local y la dirección
  `desk-radar.local`, más un botón para borrar la configuración de WiFi y
  volver al portal (ver [Configuración inicial](#configuración-inicial)).
- **Banner de mensajes**: los mensajes que llegan por `POST /api/message`
  aparecen deslizándose desde arriba sobre cualquier pantalla, sin romperla.
- **Modo Radar**: pantalla circular centrada en tu casa
  (`-34.5858006, -58.5917033`), **radio 20 km sobre un mapa real** recortado en
  círculo, sin nombres de ciudades: el único punto rotulado es **El Palomar
  (SADP)**. Con barrido animado tipo radar
  de aviación: una línea que gira 360° cada 3,5 s dejando una estela que se
  desvanece detrás. Cuando el barrido pasa por encima de un avión, el blip
  destella y se agranda un instante, como si lo acabara de detectar (es solo
  visual: no dispara ningún fetch). Los anillos están rotulados con su
  distancia real (5/10/15/20 km), hay puntos cardinales N/E/S/O alrededor
  del círculo y una leyenda de color debajo. Refresca los datos cada 30s, y
  pasa a cada 5s automáticamente si detecta un avión a menos de 10 km. La
  animación es independiente del ciclo de fetch: el barrido sigue girando
  sobre la última posición conocida aunque no lleguen datos nuevos.
- **Modo Mapa**: mapa oscuro **realista** de tu zona (calles, rutas, nombres de
  localidades) con los aviones encima como siluetas rotadas según su rumbo real
  y coloreadas por altitud, con la misma rampa que usa FlightRadar24: naranja
  abajo → amarillo → verde → cian arriba. Abajo hay una barra de gradiente que
  explica la escala. El callsign se muestra solo para el avión más cercano a
  casa (con diez etiquetas encima el mapa se vuelve ilegible); el resto se
  consulta tocándolos. Un botón abajo alterna entre **80 km y 40 km** de ancho.
  El fondo no se descarga: es una imagen pre-renderizada que vive en la flash
  (ver [Mapa](#mapa-pre-renderizado)).
- **Detalle de avión**: tocá cualquier punto del radar y se abre su ficha
  (callsign, ICAO24, distancia, rumbo cardinal, altitud, velocidad,
  coordenadas). La zona tocable es más grande que el punto dibujado, así que
  no hace falta puntería. La ficha queda congelada mientras la mirás; tocás
  de nuevo en cualquier lado y volvés al radar.
  A los pocos segundos de abrirla aparece, si OpenSky la tiene, la **ruta
  estimada** (origen -> destino, por ejemplo "Aeroparque -> Ezeiza"): llega
  aparte porque es una segunda consulta a la API, y no vale la pena pedirla
  para cada avión que aparece en el radar, solo para el que estás mirando.
- **Modo Aeropuertos**: lista de tráfico con altitud baja (<3000m) cerca de
  Ezeiza (SAEZ), Aeroparque (SABE) o El Palomar (SADP). Un botón en la parte
  inferior ("Siguiente aeropuerto >") rota entre los 3. Tocar una fila abre la
  ficha del avión, igual que en Radar y Mapa.
- **Más info**: sub-menú con tres botones táctiles, "NOTICIAS", "CLIMA" e "ISS".
- **Pantalla Noticias**: los 5 titulares más recientes de Argentina (GNews,
  `country=ar&lang=es`). Cada fila muestra el titular partido en hasta dos
  renglones, más el medio y la hora de publicación en horario argentino. Los
  titulares llegan en UTF-8 con tildes y eñes, que las fuentes embebidas de
  TFT_eSPI no tienen, así que se pliegan a ASCII antes de dibujarlos
  (`TextUtils::toAscii`); lo que no entra a lo ancho se corta con "...".
  **Tocá un titular y se abre la noticia**: el título completo sin recortar, el
  medio, la hora y el resumen que manda GNews. Cualquier toque vuelve a la
  lista.
- **Pantalla Clima**: temperatura, sensación térmica, condición, humedad y
  viento para `HOME_LAT`/`HOME_LON`, con un ícono dibujado a mano con
  primitivas de TFT_eSPI (sol, nubes, lluvia, nieve, niebla, tormenta) según
  el código WMO que devuelve Open-Meteo. No se descarga ninguna imagen.
  Tocá la pantalla para el **pronóstico extendido** (hasta 5 días, hoy
  incluido): día, condición y máxima/mínima. Viene en la misma request que el
  clima actual, así que no cuesta una llamada más a la API.
- **Pantalla ISS**: un mapamundi en miniatura con un punto amarillo (celeste
  si está eclipsada, sin luz solar directa) marcando dónde está la Estación
  Espacial Internacional ahora mismo, y una cruz blanca fija marcando tu casa
  — de un vistazo se ve si está sobre el Pacífico, África, encima tuyo o del
  otro lado del planeta. El contorno de los continentes es un bitmap de 220x110
  generado una sola vez con `tools/build-worldmap.mjs` a partir de datos
  públicos de Natural Earth y compilado directo al firmware (no se descarga
  nada en tiempo de ejecución). Sobre el mapa también se dibuja la **traza de
  la órbita**: por dónde pasó (línea celeste apagada) y hacia dónde va (línea
  naranja) en los ~45 minutos a cada lado del momento actual —
  `wheretheiss.at` puede reconstruir posiciones pasadas y predecir futuras a
  partir del TLE que tiene guardado, así que alcanza con pedirle varios
  timestamps a la vez—. Esa traza cambia poco en minutos, así que se
  refresca cada 5 en vez de junto con la posición puntual. Debajo del mapa,
  en texto: distancia y rumbo cardinal desde `HOME_LAT`/`HOME_LON`, altura
  orbital, velocidad y si está a la luz del sol o en la sombra de la Tierra.
  La posición puntual sí se pide cada 10 segundos: a ~27.600 km/h un dato con
  más de eso ya no coincide ni de cerca con la posición real.
- **Cache**: noticias, clima e ISS se guardan en RAM y no se vuelven a pedir
  mientras el cache siga vigente (20, 15 y 0.17 minutos respectivamente, en
  `config.h`). Si una request falla, se reintenta recién al minuto en vez de
  martillar la API. Con eso el free tier de GNews (100 requests/día) alcanza
  de sobra aunque dejes la pantalla puesta todo el día.
- **Barra de estado**: arriba de todo, en todas las pantallas menos Home. A la
  izquierda la navegación ("< HOME" y dónde estás), pegado al borde derecho el
  **reloj** en hora local (aparece cuando NTP sincroniza, unos segundos después
  de conectar), y en el medio la **antigüedad del último dato bueno de
  OpenSky**: "hace 5s", "hace 2m". Reemplaza al viejo indicador RAPIDO/NORMAL,
  que ahora lo dice el color: verde cuando está refrescando rápido por tráfico
  cerca, plateado en ritmo normal y **ámbar cuando el dato pasó de 90 segundos**.
  Eso último importa en el Radar: el barrido gira igual aunque no llegue nada,
  así que sin este aviso una API caída se ve exactamente igual que todo
  funcionando.
- **Actualizar por WiFi**: `desk-radar.local/update` recibe un `firmware.bin` y
  reinicia con la version nueva, sin cable. Ver [OTA](#actualizar-por-wifi-ota).
- **Volver a Home**: en cualquier pantalla, tocá la franja superior (donde
  dice "< HOME") para volver al menú principal.

## Estructura actual del proyecto

La estructura principal queda separada por responsabilidad:

```
src/
|-- app/
|   `-- main.cpp                 # punto de entrada y ciclo principal
|-- core/
|   |-- Clock.{h,cpp}            # hora por NTP
|   |-- DataLock.{h,cpp}         # candado entre la red y el loop
|   |-- NetTask.{h,cpp}          # toda la red, en el core 0
|   |-- Banner.{h,cpp}           # mensajes animados
|   |-- DeviceConfig.{h,cpp}     # configuracion persistente
|   |-- DisplayManager.{h,cpp}   # TFT y barra de estado
|   |-- TouchManager.{h,cpp}     # calibracion y eventos tactiles
|   `-- WebPortal.{h,cpp}        # portal cautivo, dashboard y mDNS
|-- models/
|   |-- AircraftBlip.h           # modelo de aeronave
|   |-- UiRect.h                 # rectangulo para hit-testing
|   `-- WorldMapAsset.h          # bitmap del mapamundi, generado (ver Pantalla ISS)
|-- screens/
|   |-- AirportScreen.{h,cpp}
|   |-- DetailScreen.{h,cpp}
|   |-- HomeScreen.{h,cpp}
|   |-- InfoMenuScreen.{h,cpp}
|   |-- MapScreen.{h,cpp}
|   |-- NewsDetailScreen.{h,cpp}   # la noticia abierta desde la lista
|   |-- ISSScreen.{h,cpp}          # posicion de la ISS
|   |-- NewsScreen.{h,cpp}
|   |-- RadarScreen.{h,cpp}
|   |-- SettingsScreen.{h,cpp}
|   |-- WeatherForecastScreen.{h,cpp} # pronostico extendido, se abre desde Clima
|   `-- WeatherScreen.{h,cpp}
|-- services/
|   |-- ISSClient.{h,cpp}         # posicion de la ISS y cache
|   |-- MapTiles.{h,cpp}          # mapas raster en LittleFS
|   |-- NewsClient.{h,cpp}        # titulares y cache
|   |-- OpenSkyClient.{h,cpp}     # vuelos y OAuth2
|   `-- WeatherClient.{h,cpp}     # clima y cache
|-- utils/
|   |-- AirportUtils.h            # nombre legible para un ICAO de aeropuerto
|   |-- DateUtils.h               # dia de semana de una fecha, sin tocar time.h
|   |-- StrUtils.h                # copia a buffers fijos, sin dependencias
|   |-- GeoMap.h                  # proyeccion geografica
|   |-- GeoUtils.h                # calculos geograficos
|   `-- TextUtils.h               # texto y ajuste por ancho
`-- MapAssets.h                   # generado por tools/build-map.mjs
```

Todos los `#include` internos usan la ruta modular completa
(`#include "core/DisplayManager.h"`), que resuelve porque `src/` está en el
include path (`-I src` en `platformio.ini`). La raíz de `src/` ya no tiene
headers-puente: el único archivo suelto es `MapAssets.h`, que es generado.

### Tests

```
test/
|-- test_geo/                     # GeoUtils (haversine, rumbo, bbox), GeoMap
                                   # y DateUtils
`-- test_text/                    # TextUtils, StrUtils, AirportUtils y
                                   # OpenSkyClient::nextBackoffMs
```

Se corren **sobre la placa**, con el cable puesto:

```bash
pio test -e esp32dev              # las dos suites (61 tests)
pio test -e esp32dev -f test_geo  # solo una
```

Son las unidades que no tienen nada de hardware y donde un error no se ve en
la pantalla: un avión dibujado 3 km corrido no tira ninguna excepción, sólo
aparece sobre la calle equivocada. Cubren los valores de referencia externos
(un grado de latitud, antípodas), las distancias y rumbos reales a los tres
aeropuertos, la coherencia entre la proyección del firmware y el mapa generado
(`test_la_casa_cae_en_el_centro_de_cada_mapa` falla si `MapAssets.h` y los
`.bin` dejaron de ser del mismo lugar), la aritmética del backoff de OpenSky,
y el día de la semana de una fecha calendario contra hechos verificables por
fuera del proyecto (1900-01-01 y 2024-01-01 fueron lunes, el Y2K fue sábado).

Al terminar, la placa queda con el firmware de test: volvé a subir el real con
`pio run -t upload`.

`platformio.ini` tiene `test_build_src = yes`: sin eso, `pio test` no compila
los `.cpp` de `src/` y sólo linkea lo que ya era header-only, así que un test
que referencia algo definido en un `.cpp` real (como
`OpenSkyClient::nextBackoffMs`) falla el link con "undefined reference" sin
ningún error de compilación que lo explique. Y como eso compila **todo**
`src/`, incluido `main.cpp`, su `setup()`/`loop()` chocan con los del test
runner ("multiple definition") — de ahí el `#ifndef UNIT_TEST` que los rodea:
`UNIT_TEST` es la macro que PlatformIO define solo al compilar para test.

## Mapa pre-renderizado

El fondo del modo Mapa **no se descarga en la placa**: es una imagen raster
generada una sola vez en la PC y guardada en la flash del ESP32.

### Regenerarlo

```bash
cd tools
npm install            # solo la primera vez
node build-map.mjs     # baja tiles, genera data/*.bin y src/MapAssets.h
node verify-map.mjs    # opcional: marca aeropuertos para chequear la proyección
cd ..
pio run -t uploadfs    # sube los mapas a la partición spiffs
pio run -t upload      # sube el firmware
```

Mirá `tools/preview80.png` y `preview40.png` antes de flashear: es exactamente
lo que va a verse en la pantalla.

**Para mover el área**, cambiá `HOME_LAT`/`HOME_LON` en `include/config.h` (es
la única fuente de verdad: `build-map.mjs` los lee de ahí) y volvé a correr el
generador.

### Por qué pre-renderizado y no tiles en vivo

| Enfoque | RAM | Veredicto |
|---|---|---|
| Sprite de pantalla completa 16 bpp | 150 KB | no entra (WROOM sin PSRAM) |
| Tiles PNG descargados y decodificados en la placa | ~40 KB por decode | 2-5 s por tile, y hay que cachear igual |
| **Raster pre-renderizado en flash** | ~8 KB de buffer de banda | **elegido** |
| Vectorial dibujado a mano | casi nada | no se parece a un mapa real |

Como el área es fija, todo el trabajo pesado (bajar, pegar, recortar, reducir)
se hace en la PC y el ESP32 solo hace `pushImage`.

Cada `.bin` son 240 × 262 × 2 = **125.760 bytes**; los dos suman 251 KB de la
partición `spiffs` de 896 KB (ver [Particiones](#particiones)).

### El mapa de fondo del radar

El radar usa **otro archivo y otro formato**: `data/radar.bin`, de 200×200 px
en **4 bpp indexado** (20.000 bytes), sin capa de etiquetas y recortado en
círculo.

El motivo es la RAM. El disco del radar es un `TFT_eSprite` de 4 bpp porque uno
de 16 bpp costaría 80 KB y no convive con el handshake TLS. Así que el mapa se
cuantiza a **5 grises** y se guarda **ya empaquetado con el layout exacto de
TFT_eSprite** (`(x + y*w)>>1`, nibble alto para x par). El firmware lo carga una
vez a RAM y después, en cada frame, lo mete al sprite con un `memcpy` en vez de
convertir píxel por píxel:

```
RAM del radar:  20 KB (sprite) + 20 KB (copia del mapa) = 40 KB
                contra los 80 KB que costaría un sprite de 16 bpp
```

Los 5 grises salen de **k-means sobre el histograma real**, no de una escala
fija ni de cuantiles por población. El basemap oscuro tiene tres picos enormes
(agua ~35, tierra ~71 y ~78) y las rutas viven dispersas entre 82 y 98 con
poquísimos píxeles: repartir por población mete 4 de los 5 niveles dentro del
rango 75-80 y **se come las rutas**, que son justo lo que hace reconocible el
mapa. k-means encuentra los clusters reales (35, 60, 71, 78, 87). Después se
estiran a `RADAR_STRETCH_MIN..MAX` para que se distingan en una TFT chica sin
taparle el protagonismo a los blips.

La paleta de 16 queda repartida así: índice 0 negro (fuera del círculo), 1-5
los grises del mapa, y los 10 restantes para el radar. Por eso la estela tiene
**3 bandas de brillo y no 7** como cuando el fondo era negro.

Los aviones y El Palomar se ubican por **distancia + rumbo** (polar), no por
Mercator, para que compartan el sistema de coordenadas de los anillos. Coincide
con el mapa porque el generador usa la misma escala: `RADAR_RING_MAX / alcance`
= 88 px / 20 km = 227,3 m/px, que es exactamente el m/px del recorte. A 20 km
las dos proyecciones difieren menos de un píxel.

⚠️ `RADAR_RANGE_KM` de `config.h` tiene que coincidir con el alcance con el que
se generó el mapa. Si no, los anillos y el mapa quedan a distinta escala y los
aviones caen sobre calles que no son. Hay un `static_assert` en
`RadarScreen.cpp` que **no deja compilar** si se desincronizan.

### Geometría

| Modo | Zoom | m/px nativo | m/px final | Fuente | Factor | Cobertura |
|---|---|---|---|---|---|---|
| 80 km | 9 | 251,7 | 333,3 | 318×347 | 0,755 | 80 × 87,3 km |
| 40 km | 10 | 125,9 | 166,7 | 318×347 | 0,755 | 40 × 43,7 km |
| radar | 10 | 125,9 | 227,3 | 361×361 | 0,554 | 45,5 km de lado (20 km de radio) |

Los zooms nativos de OSM no caen justo en 80/40 km, así que se baja a mayor
resolución y se reduce con Lanczos. Las etiquetas quedan a ~75% del tamaño de
diseño: legibles, pero un toque blandas. Si en la pantalla real no se leen, la
salida es usar el zoom nativo 1:1 (cobertura 60/30 km) cambiando `widthKm` en
`MODES`, sin tocar una línea de C++.

⚠️ En modo 40 km **Ezeiza queda fuera del recorte** (está a ~28 km al sur y el
alto visible son ±21,8 km). Es esperable, no un bug.

### Tres trampas que ya nos costaron

1. **Los tiles de CARTO y Stadia devuelven HTTP 200 con una imagen que dice
   "API KEY REQUIRED"** en vez de un error. Chequear el status code no alcanza:
   hay que *mirar* el resultado. Por eso el proyecto usa el basemap oscuro de
   Esri, que no pide key.
2. **Esri usa `{z}/{fila}/{columna}` en la URL, o sea Y antes que X**, al revés
   de la convención de OSM. Y sirve el fondo y las etiquetas como dos capas
   separadas que hay que componer.
3. **Los `.bin` se guardan en big-endian a propósito**, que es como los quiere el
   ILI9341. Así el ESP32 hace `setSwapBytes(false)` y empuja el buffer sin tocar
   un byte. Si algún día los colores salen psicodélicos, el sospechoso es
   `toRgb565BE()` en `build-map.mjs`.

### Proyección

El mapa está en **Web Mercator**, no en equirectangular: la latitud no se puede
interpolar linealmente contra el alto de la imagen o los aviones quedan corridos
en vertical. `src/utils/GeoMap.h` hace la proyección correcta usando las constantes de
`src/MapAssets.h`, que **genera el mismo script que arma los `.bin`** — así la
imagen y la proyección del firmware no se pueden desincronizar.

`verify-map.mjs` valida esto sin flashear: proyecta Ezeiza, Aeroparque, El
Palomar y el Obelisco con la misma fórmula y los marca sobre el preview.

### Atribución

Los tiles son **© Esri — World Dark Gray Canvas** (Esri, HERE, Garmin,
© OpenStreetMap contributors y la comunidad GIS).

### Un solo fetch para Radar y Mapa

Las dos pantallas miran la misma zona, así que comparten una única llamada a
OpenSky (`HOME_FETCH_RADIUS_KM = 45`, que cubre a la vez el radio de 40 km del
radar y el recorte de 87,3 km de alto del mapa). Cambiar de Radar a Mapa, o
alternar el zoom, **no dispara ninguna request**: son recortes de datos que ya
están en RAM.

## APIs usadas

| Pantalla | API | ¿API key? | Free tier |
|---|---|---|---|
| Radar / Mapa / Aeropuertos / Detalle | [OpenSky Network](https://opensky-network.org/) | OAuth2 client id + secret | gratis para uso personal |
| Noticias | [GNews.io](https://gnews.io/) | sí (`GNEWS_API_KEY`) | 100 requests/día, **sin tarjeta de crédito** |
| Clima | [Open-Meteo](https://open-meteo.com/) | **no hace falta** | libre para uso no comercial |
| ISS | [wheretheiss.at](https://wheretheiss.at/) | **no hace falta** | sin límite documentado, uso liviano (cache de 10s) |

### Backoff y contador de OpenSky

`REFRESH_FAST_MS` es 5 segundos: con tráfico cerca de casa son 720 requests por
hora. El panel muestra **"Requests OpenSky hoy"** (se reinicia a las 00:00 hora
local, necesita NTP) para poder mirarlo contra tu cupo.

Si OpenSky devuelve **429**, el cliente entra en backoff exponencial —arranca en
1 minuto y se duplica en cada 429 sucesivo hasta un techo de 30 minutos— en vez
de seguir golpeando al mismo ritmo. Un fetch bueno lo resetea a cero. Mientras
dura, el panel muestra cuánto falta para el próximo intento.

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
`timezone=auto` la hora de la medición ya viene en horario local, así que esa
pantalla no depende del reloj del dispositivo.

El reloj de la barra de estado sí usa NTP (`src/core/Clock.h`): se sincroniza
solo cuando aparece la IP y no bloquea nada, porque el cliente SNTP del core
corre en segundo plano. Mientras no haya hora creíble, simplemente no se dibuja
—mostrar "00:00" sería peor que no mostrar nada—. Argentina es UTC-3 todo el
año, así que alcanza con `TZ_OFFSET_H` y no hace falta arrastrar la base de
datos de zonas horarias.

## Notas de implementación

### `AircraftState` sin `String`

Los dos campos de texto de un avión son `char icao24[7]` y `char callsign[9]`,
no `String`. Los largos son fijos por especificación ADS-B: el ICAO24 son 6
dígitos hexadecimales y el callsign, 8 caracteres.

El motivo es la fragmentación del heap. Cada fetch trae hasta 60 aviones, y con
dos `String` por avión eran unos **120 malloc/free cada 5 a 30 segundos**, más
los que agregaban las copias de `AircraftBlip` y el `std::sort`. En una pantalla
pensada para quedar encendida semanas eso va picando el heap, y justo las dos
cosas que más lo necesitan piden bloques **grandes y contiguos**: el sprite del
radar (20 KB) y el stack de la tarea de red (14 KB).

Por eso el dashboard muestra ahora **"Bloque contiguo mayor"** además del heap
libre: es la medida real de fragmentación. El heap puede tener 150 KB libres
repartidos en pedacitos y no poder darte los 20 KB seguidos del sprite. Si ese
número baja con las horas, algo está fragmentando.

El texto entra por `StrUtils::copyTrimmed()`, que recorta y **nunca desborda**.
El recorte no es cosmético: OpenSky rellena el callsign a 8 caracteres con
espacios (`"AAL123  "`), y sin sacarlos las etiquetas quedan descentradas y
`textWidth()` mide de más. Está testeada, con centinela incluido para detectar
una escritura fuera de rango.

Las cuatro pantallas que hacían `callsign.length() ? callsign : icao24` a mano
usan `AircraftState::label()`.

### Una fila por pantalla

`main.cpp` tenía nueve modos repartidos en **cuatro cadenas de `if/else`
distintas**: una para el fetch, otra para el render, otra en `enterMode()` y
otra en el `loop()`. Agregar una pantalla obligaba a tocar las cuatro, y
olvidarse de una no daba error de compilación: daba una pantalla que no
refrescaba, o que no respondía al toque.

Ahora cada modo es **una fila de `MODE_OPS`**, con todo lo que necesita el
router: de qué se alimenta, si la barra de arriba vuelve a Home, cuánto duerme
el loop, y los punteros a `onEnter` / `onExit` / `render` / `tick` /
`handleTap`. Los lambdas van sin captura a propósito: así convierten a puntero
de función y la tabla queda en flash en vez de en RAM.

`handleTap` devuelve **a qué modo ir**; para quedarse, devuelve el mismo. Las
acciones que no son navegación —cambiar el zoom del mapa, rotar de aeropuerto,
recalibrar el touch— se resuelven adentro del lambda y redibujan solas.

Y hay una red de seguridad:

```cpp
static_assert(MODE_COUNT == (int)Mode::Settings + 1,
              "Falta (o sobra) una fila en MODE_OPS: ...");
```

Agregar un modo al `enum` y olvidar la fila **no compila**. La pantalla de
noticia (`Mode::NewsDetail`) se sumó después de este refactor y fue exactamente
eso: una fila.

### El radar suelta sus 40 KB al salir

`RadarScreen` reserva un sprite de 20 KB para el disco y otros 20 KB para la
copia del mapa de fondo. Se pedían la primera vez y no se liberaban nunca, ni
estando en Clima. Son los mismos 40 KB que compiten con el pico del handshake
TLS y con el stack de la tarea de red, y encima se piden **contiguos**, que es
lo primero que escasea cuando el heap se fragmenta.

`onExit()` los suelta al cambiar de pantalla y deja que `ensureDisc()` vuelva a
asignarlos al volver. Reentrar cuesta releer 20 KB de LittleFS: imperceptible al
lado del fetch que igual se dispara al entrar. Al salir queda la cuenta en el
log:

```
[Radar] Sprite y mapa liberados, heap libre 202064, bloque mayor 110592
```

### La red vive en el core 0

El ESP32 tiene dos núcleos y durante mucho tiempo el firmware usó uno solo. Los
tres clientes HTTP se llamaban desde el `loop()`, que corre en el core 1 junto
con el touch, el barrido del radar y el servidor web. Un fetch de OpenSky son
varios segundos entre el handshake TLS y la respuesta, y en ese rato **no corría
nada más**: el barrido se congelaba, el touch no respondía y el dashboard no
atendía. El cartel "Buscando aviones..." existía sólo para tapar eso.

Ahora `src/core/NetTask` corre en el **core 0** (donde ya vive el stack de WiFi)
con 14 KB de stack — el handshake TLS es lo que más pide; con los 4 KB del
default la tarea se muere en el primer fetch (ver también
[Pronóstico extendido](#pronóstico-extendido-mismo-request-y-el-stack-de-la-red-subió),
que fue lo que subió el número de 10 a 14). El loop pide trabajo con
`request()` y sigue dibujando; el resultado se recoge en `netTick()`, que no
bloquea nunca.

Lo que cruza entre núcleos —la lista de aviones, los titulares, el clima— se
publica bajo `src/core/DataLock`. La regla que hace que el candado no arruine lo
que vinimos a arreglar: **se toma sólo para publicar o leer un resultado ya
armado, nunca durante una request**. Publicar es un swap de vector; leer es
dibujar una pantalla.

Medido en la placa con `RADAR_DEBUG_TIMING=1`, con el fetch cayendo a los 32 s:

```
 30.9s   18 fps   21.8 ms/frame
 32.2s   >>> [OpenSky] Token renovado OK
 32.9s   18 fps   21.9 ms/frame
 34.9s   18 fps   21.8 ms/frame
```

Ni un frame perdido: 18-19 fps constantes durante los 80 s de la prueba. Antes
esa ventana era un congelamiento de varios segundos.

Un efecto que sí queda: durante el handshake TLS el core 0 se satura y **el
dashboard puede tardar unos segundos en responder**, porque lwIP también vive
ahí. La pantalla, que es lo que se mira, no se entera.

### Por qué el servidor web es sincrónico y no ESPAsyncWebServer

Se usa el `WebServer` del core de ESP32, no `ESPAsyncWebServer` + `AsyncTCP`.
El motivo de mirar async era no bloquear el loop de 30 ms, pero
`WebServer::handleClient()` **ya es una máquina de estados**:

```cpp
void WebServer::handleClient() {
  if (_currentStatus == HC_NONE) {
    _currentClient = _server.available();
    if (!_currentClient) { ...; return; }       // sin cliente: vuelve ya
  }
  ...
  case HC_WAIT_READ:
    if (_currentClient.available()) { _parseRequest(...); _handleRequest(); }
    else if (millis() - _statusChange <= HTTP_MAX_DATA_WAIT) {
      keepCurrentClient = true;                 // espera ENTRE llamadas, no adentro
    }
}
```

Si no hay cliente vuelve enseguida; si el cliente todavía no mandó datos, lo
guarda y **retorna** en vez de esperar. Solo procesa cuando los datos ya
llegaron. Además se llama `enableDelay(false)`, porque si no mete un `delay(1)`
en cada vuelta del loop (y el radar corre con `delay(5)`).

Ventajas concretas de esta decisión:

- **Dos dependencias menos.** `AsyncTCP` en ESP32 arrastra una historia de
  cuelgues y stack overflows.
- **Los handlers corren en el hilo del loop**, porque los llama `handleClient()`
  desde `tick()`. O sea que pasar el mensaje del POST a la pantalla es asignar
  una variable: **no hace falta ni cola de FreeRTOS ni mutex**, y con eso se va
  toda una clase de bugs de concurrencia.
- El portal cautivo, el dashboard y el formulario comparten un solo servidor y
  un solo diseño HTML.

También se descartó **WiFiManager (tzapu)**: no está en el registro de
PlatformIO (habría que pinnear una URL de git), y con `WiFiManagerParameter`
habría que mantener el formulario del portal **y** el del dashboard por
separado. Todo lo que hacía falta ya está en el core: `DNSServer`, `WebServer`,
`Preferences` y `ESPmDNS`.

### La tabla de campos: agregar una API key es una fila

`CONFIG_FIELDS` en `src/core/DeviceConfig.cpp` es la única fuente de verdad. De ahí
salen el formulario del portal cautivo, el de `/config`, el guardado en NVS y la
detección de "cambió el WiFi, hay que reiniciar":

```cpp
static const DeviceConfig::Field CONFIG_FIELDS[] = {
  // clave NVS   name    etiqueta                 ayuda   secreto  es_wifi
  { "wifi_ssid", "ssid", "Red WiFi",              "...",  false,   true  },
  { "os_secret", "ossec","OpenSky Client Secret", "",     true,    false },
  // ← agregar acá: aparece solo en los dos formularios
};
```

⚠️ Las claves de NVS no pueden pasar de **15 caracteres**.

### El marcador `seeded`

`include/secrets.h` sigue existiendo como comodidad de desarrollo, pero solo
precarga NVS **una vez en la vida del dispositivo**, marcado con una flag
`seeded` en NVS.

Sin ese marcador el botón *"Reiniciar config WiFi"* quedaría roto en cualquier
equipo que tenga `secrets.h`: `clearWifi()` deja el SSID vacío, y en el arranque
siguiente la precarga lo volvería a llenar, así que el reset se desharía solo y
en silencio.

## Actualizar por WiFi (OTA)

Con el dispositivo conectado, entrá a **`desk-radar.local/update`**, elegí el
`firmware.bin` que PlatformIO deja en `.pio/build/esp32dev/` y dale a
actualizar. Sube con barra de progreso y el aparato se reinicia solo. Una
actualización de 1,2 MB tarda unos 11 segundos.

La página te dice desde qué partición estás corriendo y en cuál se va a
escribir: van alternando `app0` → `app1` → `app0`. Si algo sale mal a mitad de
camino, el bootloader sigue arrancando la que estaba, porque la nueva se marca
como válida recién cuando terminó de escribirse entera.

Para confirmar que entró, mirá la **versión** (`FIRMWARE_VERSION` en `config.h`)
en el panel: si cambió el número, está corriendo el binario nuevo.

También se puede desde la línea de comandos:

```bash
curl --digest -u admin:TU_PIN -H "Expect:" \
     -F "u=@.pio/build/esp32dev/firmware.bin" \
     http://desk-radar.local/update
```

Dos cosas que **no** cambian por OTA: los mapas (van en su propia partición, se
suben con `pio run -t uploadfs`) y la tabla de particiones. Si venís de una
versión anterior a `partitions_ota.csv`, el primer flasheo tiene que ser por
cable.

### Particiones

`partitions_ota.csv`: dos slots de app de 1,5 MB, 896 KB de LittleFS y el
coredump. El firmware ocupa ~1,19 MB, o sea el 76 % de un slot.

Al pasar de `huge_app.csv` a esta tabla se cuidó que **`spiffs` quedara en el
mismo offset (`0x310000`) y con el mismo tamaño (`0xE0000`)**, así que —a
diferencia del cambio de esquema anterior, que está documentado abajo como
trampa— **no hizo falta volver a subir los mapas**. `nvs` también sigue en
`0x9000`, así que la calibración del touch, las credenciales y el PIN
sobrevivieron.

### Partición `huge_app` (esquema anterior)

Con el esquema `default` la app tenía 1,31 MB y ya estaba al 83%: el portal web
no entraba. Se pasó a `huge_app.csv` (3 MB de app, sin slot OTA, que no se usa).

⚠️ Eso mueve la partición `spiffs` de `0x290000` a `0x310000`, así que después
de este cambio hay que correr **`pio run -t uploadfs` una vez más**. La
partición `nvs` sigue en `0x9000` en los dos esquemas, así que la calibración
del touch y la configuración guardada sobreviven.

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

### TLS validado, sin bundle de CAs

Los clientes HTTPS (OpenSky, GNews, Open-Meteo) usaban
`client.setInsecure()`, que acepta cualquier certificado — es lo mismo que no
tener TLS: alguien en el medio del WiFi podía hacerse pasar por cualquiera de
ellos y quedarse con el client secret de OpenSky o la API key de GNews.
Estaba bloqueado hasta que hubo NTP (validar una cadena de certificados
necesita saber qué día es); con el reloj ya andando, se cerró. El cliente de
la ISS, agregado después, se sumó directamente con `setCACert` — para entonces
ya no hacía falta discutir el enfoque.

Se descartó armar un bundle completo de CAs (el que usan los navegadores):
`arduino_esp_crt_bundle_attach` existe en el framework, pero el binario del
bundle en sí no viene incluido — hay que generarlo aparte con una herramienta
de ESP-IDF, y agregaría varias decenas de KB justo cuando la flash ya está al
76 % de un slot OTA. En su lugar se **pinea el root CA** que comparten todos
los hosts:

```cpp
static const char* TLS_ROOT_CA_PEM = R"CERT(
-----BEGIN CERTIFICATE-----
...
-----END CERTIFICATE-----
)CERT";
```

Los cinco hosts (`opensky-network.org`, `auth.opensky-network.org`, `gnews.io`,
`api.open-meteo.com`, `api.wheretheiss.at`) terminan en el mismo root: **ISRG
Root X1**, de Let's Encrypt. Se pinea el **root** y no un certificado de hoja
ni un intermedio a propósito: los de hoja rotan cada ~90 días y los
intermedios de tanto en tanto, pero el root tiene vigencia hasta 2035 — con eso
alcanza para no tener que tocar esto de nuevo.

El PEM se extrajo de un almacén de confianza **local** (el bundle de CAs que
trae Git para Windows), no de una conexión en vivo a los hosts: confiar en lo
que devuelve una conexión hecha desde un entorno de desarrollo en la nube
sería darle la razón a un posible intermediario en el camino, justo lo que
esto viene a evitar. Se confirmó funcionando en la práctica, con el
dispositivo en su red real: los servicios respondieron con el
certificado validado (`[OpenSky] Token renovado OK`, `[News] N titulares
actualizados`, `[Clima] N.N C, código WMO N`, `[ISS] N km de altura...`, todos
sin ningún error de handshake).

### Ruta de un avión: una segunda consulta, no una tabla de aeropuertos

La ficha de detalle pide el origen/destino a `/flights/aircraft` con una
ventana de 24 h hacia atrás (`begin`/`end` en epoch, por eso necesita NTP igual
que la validación de TLS) y se queda con el **último** vuelo de la lista, que
es el que está en curso o el más reciente si el avión está en tierra. Es una
segunda request, separada de `/states/all`, así que se pide **una sola vez al
abrir la ficha**, no para cada avión que aparece en el radar — comparte el
mismo contador y backoff que `fetchStates()`, porque un 429 en cualquiera de
los dos endpoints significa lo mismo: se agotó el cupo de la cuenta.

OpenSky devuelve el origen y destino como **código ICAO** (`SAEZ`, no
"Ezeiza"). El proyecto solo traduce a nombre los 3 aeropuertos que ya tiene en
`AIRPORTS` (`config.h`); para cualquier otro muestra el código tal cual. Una
base completa de aeropuertos son miles de filas para un dato de más en una
ficha, no una app de vuelos.

Como es una segunda consulta, la respuesta llega **después** de que la ficha ya
se dibujó (la ficha no espera). Si para cuando llega el usuario ya volvió al
radar o pasó a mirar otro avión, se descarta: `RouteInfo` lleva el `icao24` al
que corresponde, y sólo se aplica si sigue siendo el avión que se está mirando.

Probado contra la API real: para un avión sin callsign visto en el radar,
`/flights/aircraft` devolvió 3 vuelos en la ventana y el último traía
`estDepartureAirport=SABE` (Aeroparque) sin destino todavía — la ficha muestra
"Aeroparque -> ?" en ese caso, mejor que no mostrar nada.

### Pronóstico extendido: mismo request, y el stack de la red subió

Igual que la ruta de un avión, el pronóstico de 5 días **no es una request
aparte**: va en el mismo `&daily=...` que ya le pide Open-Meteo el clima
actual (`WeatherClient::refresh()`), así que mostrarlo no cuesta una llamada
más a la API. Open-Meteo no manda el día de la semana, sólo la fecha
(`"2026-09-05"`); `DateUtils::dayOfWeek()` lo calcula con el algoritmo de
Zeller, una función pura sin tocar `time.h` — el día de una fecha calendario
no depende de la hora del sistema ni de NTP, así que no hace falta arrastrar
esa dependencia (ni sus casos borde de zona horaria) para lo que es aritmética
pura. Eso de paso lo hace testeable sin reloj.

Agregar este bloque hizo la respuesta de Open-Meteo bastante más grande (4
arrays más, hasta 5 elementos cada uno), y en el primer arranque de prueba la
tarea de red murió con un `Guru Meditation Error` de backtrace corrupto —la
firma típica de un stack overflow— con los 10 KB de stack que traía desde
antes. No se pudo reproducir en 3 reinicios limpios midiendo con
`uxTaskGetStackHighWaterMark()` (~5,3 KB libres en cada uno), así que no hay
certeza de que haya sido eso exactamente; pudo ser un problema puntual de
red/NTP en ese primer arranque. De cualquier forma, subir el margen sale
gratis frente a los 320 KB de RAM totales del ESP32, así que el stack de la
tarea de red pasó de 10 a 14 KB en vez de dejarlo al límite medido.

### ⚠️ Leer el body con `getString()`, nunca con `getStream()`

Las APIs de este proyecto (OpenSky, GNews, Open-Meteo y wheretheiss.at) responden con
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

- Usar T_IRQ (GPIO17) por interrupción en vez de polling, para ahorrar CPU.
- Usar el NeoPixel para alertar visualmente cuando hay tráfico muy cerca.
- Cachear también los resultados de OpenSky, para no gastar cupo si dos modos
  piden zonas solapadas (Noticias y Clima ya cachean).

## Licencia

El código es [MIT](LICENSE): usalo, modificalo, hacé lo que quieras.

Eso no cubre los datos de terceros que el proyecto consume, que tienen sus
propios términos: los tiles del mapa son © Esri y colaboradores (ver
[Atribución](#atribución)), y los datos de OpenSky, GNews, Open-Meteo y
wheretheiss.at se usan bajo las condiciones de cada API (ver
[APIs usadas](#apis-usadas)).
