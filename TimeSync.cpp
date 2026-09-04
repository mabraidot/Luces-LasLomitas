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
#include <coredecls.h>          // settimeofday_cb

static uint32_t lastCheckAt = 0;
static uint32_t lastSyncAt  = 0;
static bool     everSynced  = false;

/*
  Marca de la ultima vez que SNTP puso la hora DE VERDAD.

  No hace falta volatile ni proteger el acceso: el core no llama al callback
  desde una interrupcion, lo agenda con schedule_recurrent_function_us() y
  corre en el mismo contexto que loop().
*/
static uint32_t lastSntpAt  = 0;
static bool     sntpEver    = false;

// Ultima diferencia medida entre NTP y el RTC, con signo: positivo = el RTC
// atrasa. Sirve para medir la deriva del cristal sin tener que reiniciar.
static int32_t  lastDiff    = 0;
static bool     hasDiff     = false;

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
  lastSntpAt = 0;
  sntpEver   = false;

  /*
    El callback va ANTES de configTime() para no perderse la primera
    sincronizacion, que llega a los pocos segundos de conectar.

    El argumento distingue el origen: es true solo cuando la hora la puso el
    cliente SNTP. Un settimeofday() nuestro llega con false y no cuenta como
    sincronizacion, que es exactamente lo que queremos.
  */
  settimeofday_cb([](bool fromSntp) {
    if (fromSntp) {
      lastSntpAt = millis();
      sntpEver   = true;
    }
  });

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

  /*
    El segundo guard: frescura.

    El de arriba descarta el 1970 del arranque, pero no el caso contrario, que
    es el que se da con el router prendido y sin internet: la hora se puso hace
    dias y desde entonces corre libre sobre el oscilador de la ESP. Sin esto el
    codigo la tomaria por buena y le escribiria al RTC la deriva acumulada,
    degradando la unica fuente de hora que sobrevive los cortes de luz.

    Que se salte una correccion no cuesta nada: el RTC sigue llevando la hora y
    en el proximo chequeo, ya con internet, se corrige.
  */
  if (!sntpEver || (now - lastSntpAt) > NTP_FRESH_MS) {
    // Solo con el periodo de 6 h. Antes de la primera sincronizacion se vuelve
    // por aca cada 8 s y el log seria una catarata. everSynced implica
    // sntpEver, asi que si se loguea, lastSntpAt es un valor real.
    if (everSynced) {
      logPrintf("NTP: sin sincronizar hace %lu min, no toco el RTC",
                (unsigned long)((now - lastSntpAt) / 60000UL));
    }
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
    int64_t diff  = ntpEpoch - rtcEpoch;
    int64_t adiff = (diff < 0) ? -diff : diff;

    /*
      Se guarda acotado. Un RTC en 1970, o en un año absurdo despues de una
      lectura sucia, da una diferencia que no entra en 32 bits; de un numero
      asi lo unico que interesa es que es enorme.
    */
    const int64_t CAP = 2000000000LL;
    int64_t shown = (diff > CAP) ? CAP : ((diff < -CAP) ? -CAP : diff);
    lastDiff = (int32_t)shown;
    hasDiff  = true;

    /*
      Se loguea en CADA chequeo, no solo en el primero.

      Con el periodo de 6 h son cuatro lineas por dia, y la serie de esas
      lineas es lo unico que permite medir la deriva del cristal del RTC: la
      pendiente entre dos puntos separados 6 h da los segundos por dia. Un
      valor suelto no dice nada, porque el DS1302 tiene resolucion de 1 s y esa
      cuantizacion sola vale mas que la deriva de unas horas.
    */
    if (shown == 0) {
      logPrintf("NTP: el RTC esta en hora");
    } else {
      logPrintf("NTP: el RTC %s %ld s",
                (shown > 0) ? "atrasa" : "adelanta",
                (long)((shown < 0) ? -shown : shown));
    }

    if (!everSynced) {
      everSynced = true;
      lastSyncAt = now;
    }

    if (adiff < NTP_MIN_DIFF_SECONDS) {
      lastSyncAt = now;
      return;
    }
    logPrintf("NTP: corrigiendo el RTC, %ld s de diferencia", (long)shown);
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

bool timeSyncDiff(int32_t &seconds) {
  if (!hasDiff) {
    return false;
  }
  seconds = lastDiff;
  return true;
}

int32_t timeSyncAge() {
  if (!everSynced) {
    return -1;
  }
  return (int32_t)((millis() - lastSyncAt) / 1000UL);
}
