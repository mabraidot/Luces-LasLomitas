/*
  TimeSync.cpp - Implementacion de la sincronizacion por NTP.
*/

#include "TimeSync.h"
#include "Config.h"
#include "Clock.h"
#include "LightController.h"
#include "Net.h"
#include "Log.h"

#include <time.h>

static uint32_t lastCheckAt = 0;
static uint32_t lastSyncAt  = 0;
static bool     everSynced  = false;

/*
  Dias desde 1970-01-01 para una fecha civil (algoritmo de Howard Hinnant).

  Se calcula a mano en lugar de usar mktime() para no depender de la variable
  TZ: el signo del desplazamiento horario en configTime() de la ESP8266 es una
  fuente clasica de errores de tres horas, y aca solo hace falta comparar dos
  instantes. Se pide la hora en UTC y el desplazamiento se aplica aritmetica-
  mente, que es imposible de confundir.
*/
static int32_t daysFromCivil(int32_t y, uint32_t m, uint32_t d) {
  y -= (m <= 2) ? 1 : 0;

  const int32_t  era = (y >= 0 ? y : y - 399) / 400;
  const uint32_t yoe = (uint32_t)(y - era * 400);                    // 0..399
  const uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;        // 0..146096

  return era * 146097 + (int32_t)doe - 719468;
}

// Segundos desde 1970 de una fecha y hora ya expresadas en hora local.
static int64_t localEpoch(uint16_t year, uint8_t month, uint8_t day,
                          uint8_t hour, uint8_t minute, uint8_t second) {
  int64_t days = daysFromCivil((int32_t)year, month, day);
  return days * 86400LL + hour * 3600LL + minute * 60LL + second;
}

void timeSyncInit() {
  /*
    Se pide UTC (desplazamiento 0) y despues se corre a mano. Argentina es
    UTC-3 fijo, sin horario de verano, asi que un desplazamiento constante es
    exacto y no hace falta ninguna base de datos de husos.
  */
  configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);

  lastCheckAt = millis();
  lastSyncAt  = 0;
  everSynced  = false;
}

void timeSyncLoop() {
  uint32_t now = millis();
  uint32_t period = everSynced ? NTP_PERIOD_MS : NTP_FIRST_CHECK_MS;

  if (now - lastCheckAt < period) {
    return;
  }
  lastCheckAt = now;

  if (!netIsConnected()) {
    return;
  }

  /*
    El guard que no puede faltar.

    Mientras NTP no sincronizo, time(nullptr) devuelve un valor cercano a 0, o
    sea 1970. Si eso se escribe al RTC sin verificar, se DESTRUYE la hora buena
    que el RTC ya tenia y el dispositivo pasa a creer que esta en 1970: la
    ventana segura y la hora limite dejan de tener sentido.

    Y no es un riesgo teorico: se da en cada arranque, durante los primeros
    segundos, asi que sin el guard falla siempre.
  */
  time_t utc = time(nullptr);
  if ((uint32_t)utc < NTP_EPOCH_PLAUSIBLE) {
    return;
  }

  // gmtime_r sobre el epoch ya corrido devuelve directamente la hora local.
  time_t shifted = utc + TZ_OFFSET_SECONDS;
  struct tm lt;
  gmtime_r(&shifted, &lt);

  int64_t ntpEpoch = (int64_t)shifted;

  // Si el RTC ya esta en hora, no gastar una escritura.
  ClockDateTime dt;
  if (clockIsOk() && clockGet(dt)) {
    int64_t rtcEpoch = localEpoch(dt.year, dt.month, dt.day,
                                  dt.hour, dt.minute, dt.second);
    int64_t diff = ntpEpoch - rtcEpoch;
    if (diff < 0) {
      diff = -diff;
    }

    if (!everSynced) {
      everSynced = true;
      lastSyncAt = now;
      logPrintf("NTP: ok, el RTC difiere %ld s", (long)diff);
    }

    if (diff < NTP_MIN_DIFF_SECONDS) {
      lastSyncAt = now;
      return;
    }
    logPrintf("NTP: corrigiendo el RTC, %ld s de diferencia", (long)diff);
  }

  if (!clockSet((uint8_t)lt.tm_hour, (uint8_t)lt.tm_min, (uint8_t)lt.tm_sec,
                (uint8_t)lt.tm_mday, (uint8_t)(lt.tm_mon + 1),
                (uint16_t)(lt.tm_year + 1900))) {
    logPrintf("NTP: no se pudo escribir el RTC");
    return;
  }

  // El reloj SALTO: resincronizar la referencia de flancos del controlador,
  // sin lo cual corregir la hora a las 22:50 dispararia "hora de dormir".
  lightControllerClockChanged();

  everSynced = true;
  lastSyncAt = now;
}

int32_t timeSyncAge() {
  if (!everSynced) {
    return -1;
  }
  return (int32_t)((millis() - lastSyncAt) / 1000UL);
}
