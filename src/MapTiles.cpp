#include "MapTiles.h"
#include <LittleFS.h>
#include <esp_partition.h>

bool MapTiles::begin() {
  if (_mounted) return true;

  // LittleFS monta por defecto la particion con label "spiffs" (la del esquema
  // default.csv). Si no existe, el problema es la tabla de particiones, no el
  // uploadfs: conviene distinguir los dos casos o se pierde tiempo al pedo.
  const esp_partition_t* part = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs");
  if (!part) {
    Serial.println("[Mapa] No existe la particion 'spiffs'. Revisa board_build.partitions.");
    return false;
  }
  Serial.printf("[Mapa] Particion spiffs: %u bytes en 0x%06X\n",
                (unsigned)part->size, (unsigned)part->address);

  // false = no formatear si falla el montaje. Formatear dejaria una particion
  // vacia y valida, y el error real (nunca se subio el filesystem) quedaria
  // disimulado detras de un "falta el archivo".
  if (!LittleFS.begin(false)) {
    Serial.println("[Mapa] La particion existe pero no monta: todavia no subiste el");
    Serial.println("[Mapa] filesystem. Corre en la PC:  pio run -t uploadfs");
    return false;
  }

  // Chequeo temprano: que estén los dos archivos y con el tamaño exacto. Un
  // .bin truncado dibujaría basura sin ningún error visible.
  for (int i = 0; i < MAP_ASSET_COUNT; i++) {
    fs::File f = LittleFS.open(MAP_ASSETS[i].path, "r");
    if (!f) {
      Serial.printf("[Mapa] Falta %s en LittleFS\n", MAP_ASSETS[i].path);
      return false;
    }
    size_t sz = f.size();
    f.close();
    if (sz != MAP_BIN_BYTES) {
      Serial.printf("[Mapa] %s mide %u bytes, se esperaban %u. Regenera con build-map.mjs\n",
                    MAP_ASSETS[i].path, (unsigned)sz, (unsigned)MAP_BIN_BYTES);
      return false;
    }
  }

  _mounted = true;
  Serial.printf("[Mapa] LittleFS OK: %d mapas, %u de %u bytes usados\n",
                MAP_ASSET_COUNT,
                (unsigned)LittleFS.usedBytes(), (unsigned)LittleFS.totalBytes());
  return true;
}

bool MapTiles::openAsset(int assetIdx, fs::File& f) {
  if (!_mounted) return false;
  if (assetIdx < 0 || assetIdx >= MAP_ASSET_COUNT) return false;

  f = LittleFS.open(MAP_ASSETS[assetIdx].path, "r");
  return (bool)f;
}

bool MapTiles::pushFull(int assetIdx, int screenTop) {
  fs::File f;
  if (!openAsset(assetIdx, f)) return false;

  TFT_eSPI& tft = _display.tft();

  // El .bin ya viene byte-swapped, así que no hay que volver a swapear.
  bool oldSwap = tft.getSwapBytes();
  tft.setSwapBytes(false);

  const size_t rowBytes = (size_t)MAP_VIEW_W * 2;
  bool ok = true;

  for (int y = 0; y < MAP_VIEW_H; y += BAND_ROWS) {
    int rows = MAP_VIEW_H - y;
    if (rows > BAND_ROWS) rows = BAND_ROWS;

    size_t want = rowBytes * rows;
    if (f.read((uint8_t*)_band, want) != want) {
      Serial.printf("[Mapa] Lectura corta en la fila %d\n", y);
      ok = false;
      break;
    }

    tft.pushImage(0, screenTop + y, MAP_VIEW_W, rows, _band);
  }

  tft.setSwapBytes(oldSwap);
  f.close();
  return ok;
}

bool MapTiles::pushRect(int assetIdx, const UiRect& r, int screenTop) {
  // Recortar contra los límites de la imagen: un avión puede estar medio
  // afuera y su rect de borrado se saldría del archivo.
  int x0 = r.x < 0 ? 0 : r.x;
  int y0 = r.y < 0 ? 0 : r.y;
  int x1 = r.x + r.w; if (x1 > MAP_VIEW_W) x1 = MAP_VIEW_W;
  int y1 = r.y + r.h; if (y1 > MAP_VIEW_H) y1 = MAP_VIEW_H;
  if (x1 <= x0 || y1 <= y0) return true; // nada que restaurar

  int w = x1 - x0;
  int h = y1 - y0;

  // El rect tiene que entrar en _band. Sin este chequeo, un rect grande
  // desbordaría el buffer y pisaría memoria ajena.
  const size_t capacity = sizeof(_band) / sizeof(_band[0]);
  if ((size_t)w * (size_t)h > capacity) {
    Serial.printf("[Mapa] pushRect %dx%d no entra en el buffer de %u px\n",
                  w, h, (unsigned)capacity);
    return false;
  }

  fs::File f;
  if (!openAsset(assetIdx, f)) return false;

  TFT_eSPI& tft = _display.tft();
  bool oldSwap = tft.getSwapBytes();
  tft.setSwapBytes(false);

  const size_t rowBytes = (size_t)w * 2;
  bool ok = true;

  // Una fila por vez: cada una está en un offset distinto del archivo, así que
  // igual hace falta un seek por fila.
  for (int y = y0; y < y1; y++) {
    size_t offset = ((size_t)y * MAP_VIEW_W + x0) * 2;
    if (!f.seek(offset)) { ok = false; break; }

    uint16_t* dst = _band + (size_t)(y - y0) * w;
    if (f.read((uint8_t*)dst, rowBytes) != rowBytes) { ok = false; break; }
  }

  if (ok) {
    tft.pushImage(x0, screenTop + y0, w, h, _band);
  }

  tft.setSwapBytes(oldSwap);
  f.close();
  return ok;
}

bool MapTiles::loadRaw(const char* path, uint8_t* dst, size_t bytes) {
  if (!_mounted || !dst) return false;

  fs::File f = LittleFS.open(path, "r");
  if (!f) {
    Serial.printf("[Mapa] Falta %s\n", path);
    return false;
  }

  // Chequeo de tamano exacto: un archivo corto dibujaria basura en la mitad de
  // abajo del disco sin dar ningun error visible.
  if (f.size() != bytes) {
    Serial.printf("[Mapa] %s mide %u bytes, se esperaban %u\n",
                  path, (unsigned)f.size(), (unsigned)bytes);
    f.close();
    return false;
  }

  size_t got = f.read(dst, bytes);
  f.close();

  if (got != bytes) {
    Serial.printf("[Mapa] Lectura corta de %s (%u de %u)\n",
                  path, (unsigned)got, (unsigned)bytes);
    return false;
  }
  return true;
}
