/*
  LightSensor.cpp - Implementacion de la lectura del LDR.
*/

#include "LightSensor.h"
#include "Config.h"
#include "Settings.h"

#include <math.h>

// Acumulado de LDR_SAMPLES lecturas, suavizado. Se guarda SIN dividir: los
// 4 bits extra del sobremuestreo son justo lo que mejora el extremo oscuro,
// donde vive el umbral.
static float    ema      = 0.0f;
static bool     seeded   = false;
static uint32_t startAt  = 0;
static uint32_t lastAt   = 0;

// Precalculado en cada cambio de calibracion, para no hacerlo por muestra.
static float logNight = 0.0f;
static float invRange = 1.0f;

static uint32_t sampleOnce() {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < LDR_SAMPLES; i++) {
    sum += (uint32_t)analogRead(PIN_LDR);
  }
  return sum;
}

void lightSensorInit() {
  lightSensorReloadCalibration();

  seeded  = false;
  startAt = millis();
  lastAt  = startAt;

  /*
    Sembrar la media con la primera lectura real, no arrancar en 0. Con la
    media en 0, el nivel informado durante el primer segundo es "oscuridad
    total" mientras trepa hacia el valor verdadero. Arrancando de dia, eso es
    un falso "esta oscuro" justo cuando el controlador decide como arrancar.
  */
  ema    = (float)sampleOnce();
  seeded = true;
}

void lightSensorLoop() {
  uint32_t now = millis();
  if (now - lastAt < LDR_PERIOD_MS) {
    return;
  }
  lastAt = now;

  float sample = (float)sampleOnce();

  if (!seeded) {
    ema    = sample;
    seeded = true;
    return;
  }

  // En float a proposito. La ESP8266 no tiene unidad de punto flotante, pero
  // a 20 muestras por segundo el costo es irrelevante, y evita el error de
  // truncamiento de la EMA entera: con >>4 sobre enteros la media nunca llega
  // al valor final y queda un error permanente.
  ema += (sample - ema) * LDR_EMA_ALPHA;
}

bool lightSensorReady() {
  return seeded && (millis() - startAt >= LDR_READY_MS);
}

uint16_t lightSensorRaw() {
  float raw = ema / (float)LDR_SAMPLES;
  if (raw < 0.0f) {
    raw = 0.0f;
  }
  if (raw > 1023.0f) {
    raw = 1023.0f;
  }
  return (uint16_t)(raw + 0.5f);
}

uint8_t lightSensorLevel() {
  // Se usa el acumulado con sus bits extra, no el crudo redondeado.
  float raw = ema / (float)LDR_SAMPLES;
  if (raw < 1.0f) {
    raw = 1.0f;   // logf(0) es -infinito
  }

  float level = (logf(raw) - logNight) * invRange;

  if (level <= 0.0f) {
    return 0;
  }
  if (level >= 100.0f) {
    return 100;
  }
  return (uint8_t)(level + 0.5f);
}

void lightSensorReloadCalibration() {
  uint16_t rawNight = settings().rawNight;
  uint16_t rawDay   = settings().rawDay;

  // Settings ya valida el rango, pero logf(0) es -infinito y una division por
  // cero aca dejaria el nivel en NaN para siempre: barato asegurarlo dos veces.
  if (rawNight < 1) {
    rawNight = 1;
  }
  if (rawDay <= rawNight) {
    rawDay = (uint16_t)(rawNight + 1);
  }

  logNight = logf((float)rawNight);
  invRange = 100.0f / (logf((float)rawDay) - logNight);
}
