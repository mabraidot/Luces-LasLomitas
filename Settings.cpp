/*
  Settings.cpp - Implementacion de la configuracion persistente.
*/

#include "Settings.h"
#include "Config.h"
#include "Clock.h"
#include "Log.h"

#include <EEPROM.h>

#define SETTINGS_MAGIC    0x4C554304UL   // "LUC" + version de formato
#define SETTINGS_VERSION  1
#define SETTINGS_EEPROM   64             // reservado, con lugar para crecer

struct Stored {
  uint32_t     magic;
  uint16_t     version;
  uint16_t     crc;      // CRC-16 de 'data'
  SettingsData data;
};

static SettingsData current;

static const SettingsData DEFAULTS = {
  SET_THRESHOLD_DEFAULT,
  SET_HYSTERESIS_DEFAULT,
  SET_BEDTIME_DEFAULT,
  SET_WINDOW_START_DEFAULT,
  SET_WINDOW_END_DEFAULT,
  SET_MAX_ON_DEFAULT,
  LDR_RAW_NIGHT_DEFAULT,
  LDR_RAW_DAY_DEFAULT
};

// CRC-16/CCITT. No hace falta nada mejor: solo distingue un bloque escrito por
// nosotros de basura o de una EEPROM virgen.
static uint16_t crc16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;

  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

static uint16_t clampMinute(uint16_t value, uint16_t fallback) {
  return (value < 1440) ? value : fallback;
}

/*
  Recorta cada campo a un rango con sentido y arregla las combinaciones
  imposibles. La web valida del lado del navegador, pero un pedido HTTP se
  puede armar a mano: un umbral de 300 o una ventana de largo cero dejarian al
  controlador tomando decisiones absurdas para siempre.
*/
static void sanitize(SettingsData &s) {
  if (s.threshold > 100) {
    s.threshold = DEFAULTS.threshold;
  }
  if (s.hysteresis > 50) {
    s.hysteresis = DEFAULTS.hysteresis;
  }

  s.bedtime     = clampMinute(s.bedtime,     DEFAULTS.bedtime);
  s.windowStart = clampMinute(s.windowStart, DEFAULTS.windowStart);
  s.windowEnd   = clampMinute(s.windowEnd,   DEFAULTS.windowEnd);

  // Una ventana de largo cero no dejaria encender nunca.
  if (s.windowStart == s.windowEnd) {
    s.windowStart = DEFAULTS.windowStart;
    s.windowEnd   = DEFAULTS.windowEnd;
  }

  /*
    La hora de dormir tiene que caer dentro de la ventana segura, porque la
    franja de encendido es [inicio de ventana, hora de dormir). Si queda
    afuera, esa franja no tiene sentido: se la lleva al fin de la ventana, que
    es el comportamiento mas conservador (apaga lo antes posible).
  */
  if (!clockMinuteInRange(s.bedtime, s.windowStart, s.windowEnd)) {
    s.bedtime = s.windowEnd;
  }

  if (s.maxOnMinutes > 24 * 60) {
    s.maxOnMinutes = DEFAULTS.maxOnMinutes;
  }

  if (s.rawNight > 1023) {
    s.rawNight = DEFAULTS.rawNight;
  }
  if (s.rawDay > 1023) {
    s.rawDay = DEFAULTS.rawDay;
  }
  // Rango invertido o nulo: el mapeo logaritmico daria valores absurdos.
  if (s.rawDay <= s.rawNight) {
    s.rawNight = DEFAULTS.rawNight;
    s.rawDay   = DEFAULTS.rawDay;
  }
}

static void write() {
  Stored stored;

  stored.magic   = SETTINGS_MAGIC;
  stored.version = SETTINGS_VERSION;
  stored.data    = current;
  stored.crc     = crc16((const uint8_t *)&current, sizeof(current));

  EEPROM.put(0, stored);
  EEPROM.commit();
}

void settingsLoad() {
  Stored stored;

  EEPROM.begin(SETTINGS_EEPROM);
  EEPROM.get(0, stored);

  bool valid = stored.magic == SETTINGS_MAGIC
               && stored.version == SETTINGS_VERSION
               && stored.crc == crc16((const uint8_t *)&stored.data,
                                      sizeof(stored.data));

  if (!valid) {
    current = DEFAULTS;
    sanitize(current);
    write();
    logPrintf("Ajustes: EEPROM vacia o invalida, cargo defaults");
    return;
  }

  current = stored.data;
  sanitize(current);
  logPrintf("Ajustes: umbral %u, dormir %02u:%02u, ventana %02u:%02u-%02u:%02u",
            (unsigned)current.threshold,
            (unsigned)(current.bedtime / 60), (unsigned)(current.bedtime % 60),
            (unsigned)(current.windowStart / 60), (unsigned)(current.windowStart % 60),
            (unsigned)(current.windowEnd / 60), (unsigned)(current.windowEnd % 60));
}

const SettingsData &settings() {
  return current;
}

bool settingsSave(const SettingsData &next) {
  SettingsData candidate = next;
  sanitize(candidate);

  if (memcmp(&candidate, &current, sizeof(candidate)) == 0) {
    return false;   // nada cambio: no gastar un ciclo de flash
  }

  current = candidate;
  write();
  logPrintf("Ajustes: guardados");
  return true;
}

void settingsReset() {
  current = DEFAULTS;
  sanitize(current);
  write();
  logPrintf("Ajustes: vueltos a los valores por defecto");
}
