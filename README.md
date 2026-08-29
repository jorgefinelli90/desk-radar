# Flight Radar de escritorio (ESP32 WROOM/WROVER + TFT 2.4" táctil)

MVP de un radar de vuelos casero: muestra tráfico aéreo cerca de tu casa
(modo Radar) o cerca de aeropuertos elegidos (modo Aeropuertos), usando la
API gratuita de [OpenSky Network](https://opensky-network.org/).

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
   generados en tu cuenta OpenSky → *My OpenSky* → *API Client*).
3. Conectá el ESP32 y compilá/subí:
   ```bash
   pio run -t upload
   pio device monitor
   ```

## Cómo funciona

- **Al encender**: si es la primera vez, corre un wizard de calibración
  táctil (te pide tocar 3 cruces). Se guarda en la memoria NVS del ESP32,
  así que solo pasa una vez.
- **Pantalla Home**: dos botones grandes, "RADAR" y "AEROPUERTOS". Tocás el
  que quieras y entra directo a esa pantalla (con fetch inmediato).
- **Modo Radar**: pantalla circular centrada en tu casa
  (`-34.5858006, -58.5917033`), radio 40 km. Refresca cada 30s, y pasa a
  cada 5s automáticamente si detecta un avión a menos de 10 km (tráfico
  real pasando cerca).
- **Detalle de avión**: tocá cualquier punto del radar y se abre su ficha
  (callsign, ICAO24, distancia, rumbo cardinal, altitud, velocidad,
  coordenadas). La zona tocable es más grande que el punto dibujado, así que
  no hace falta puntería. La ficha queda congelada mientras la mirás; tocás
  de nuevo en cualquier lado y volvés al radar.
- **Modo Aeropuertos**: lista de tráfico con altitud baja (<3000m) cerca de
  Ezeiza (SAEZ), Aeroparque (SABE) o El Palomar (SADP). Un botón en la parte
  inferior ("Siguiente aeropuerto >") rota entre los 3.
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
│   ├── HomeScreen.{h,cpp}      # menú principal con los dos botones
│   ├── RadarScreen.{h,cpp}     # radar circular + hit-test de aviones
│   ├── DetailScreen.{h,cpp}    # ficha de un avión tocado en el radar
│   ├── AirportScreen.{h,cpp}   # dibujo de la lista por aeropuerto
│   └── UiRect.h                # helper de hit-testing para zonas táctiles
└── README.md
```

## Próximos pasos (fuera del MVP)

- Traer datos de ruta (origen/destino) del avión seleccionado con el endpoint
  `/flights/aircraft` de OpenSky, para enriquecer la ficha de detalle.
- Usar T_IRQ (GPIO17) por interrupción en vez de polling, para ahorrar CPU.
- Usar el NeoPixel para alertar visualmente cuando hay tráfico muy cerca.
- Mostrar hora/temperatura (DHT11) en un tercer botón en la Home.
- Cachear resultados para no gastar cupo de la API si dos modos piden zonas
  solapadas.
- Manejo de errores de red más robusto (reintentos con backoff).
