/*
  Clock.cpp - Implementacion del manejo del RTC DS1302.
  Libreria usada: https://github.com/Alash-electronics/Alash_DS1302
*/

#include "Clock.h"
#include "Config.h"
#include "Log.h"

#include <Alash_DS1302.h>

// Objeto RTC, privado de este archivo
static Alash_DS1302 rtc = Alash_DS1302(PIN_RTC_CLK, PIN_RTC_IO, PIN_RTC_CE);

// Cache de la ultima lectura buena
static ClockDateTime cached;
static bool     hasCache   = false;
static bool     ok         = false;
static bool     started    = false;   // rtc.begin() respondio alguna vez
static uint32_t lastReadAt = 0;
static uint32_t lastTryAt  = 0;

// Largo del nombre mas largo ("septiembre" / "miércoles" en UTF-8) mas el terminador
#define NAME_LENGTH     12

// Dias de la semana en memoria de programa (indice 0 = domingo)
static const char day_0[] PROGMEM = "Domingo";
static const char day_1[] PROGMEM = "Lunes";
static const char day_2[] PROGMEM = "Martes";
static const char day_3[] PROGMEM = "Miércoles";
static const char day_4[] PROGMEM = "Jueves";
static const char day_5[] PROGMEM = "Viernes";
static const char day_6[] PROGMEM = "Sábado";

static const char * const dayNames[] PROGMEM = {
  day_0, day_1, day_2, day_3, day_4, day_5, day_6
};
#define DAY_COUNT   7

// Meses en memoria de programa (indice 0 = enero)
static const char month_0[]  PROGMEM = "Enero";
static const char month_1[]  PROGMEM = "Febrero";
static const char month_2[]  PROGMEM = "Marzo";
static const char month_3[]  PROGMEM = "Abril";
static const char month_4[]  PROGMEM = "Mayo";
static const char month_5[]  PROGMEM = "Junio";
static const char month_6[]  PROGMEM = "Julio";
static const char month_7[]  PROGMEM = "Agosto";
static const char month_8[]  PROGMEM = "Septiembre";
static const char month_9[]  PROGMEM = "Octubre";
static const char month_10[] PROGMEM = "Noviembre";
static const char month_11[] PROGMEM = "Diciembre";

static const char * const monthNames[] PROGMEM = {
  month_0, month_1, month_2,  month_3,  month_4,  month_5,
  month_6, month_7, month_8,  month_9,  month_10, month_11
};
#define MONTH_COUNT 12

/*
  Copia a 'target' un nombre desde una tabla en flash.
  Valida el indice: sin esto, un mes 0 genera un indice negativo y se leen
  bytes arbitrarios de la flash (caracteres raros en el monitor serie).
*/
static void nameFromTable(char *target, const char * const *table,
                          int index, int count) {
  if (index < 0 || index >= count) {
    strcpy(target, "???");
    return;
  }
  strncpy_P(target, (const char *)pgm_read_ptr(&table[index]), NAME_LENGTH - 1);
  target[NAME_LENGTH - 1] = '\0';
}

/*
  Calcula el dia de la semana a partir de la fecha (algoritmo de Sakamoto).
  Devuelve 0 = domingo ... 6 = sabado.
*/
static uint8_t weekdayFromDate(uint8_t day, uint8_t month, uint16_t year) {
  static const uint8_t offset[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};

  if (month < 1 || month > 12) {
    return 0;
  }
  if (month < 3) {
    year--;
  }
  return (uint8_t)((year + year / 4 - year / 100 + year / 400
                    + offset[month - 1] + day) % 7);
}

// Un solo intento, sin esperas. Es lo que se usa desde clockLoop(), que no
// puede permitirse bloquear ni siquiera unas decenas de milisegundos.
static bool tryStartOnce() {
  if (!rtc.begin()) {
    return false;
  }

  // El oscilador puede quedar detenido si la pila se agoto
  if (!rtc.isRunning()) {
    logPrintf("RTC: oscilador detenido, pila agotada?");
    rtc.clockEnable(true);
  }
  return true;
}

/*
  Varios intentos con una pausa corta entre ellos. Solo desde clockInit(): en
  el arranque un modulo lento puede necesitar un par de reintentos, y 60 ms de
  setup() no molestan a nadie. En loop() se usa tryStartOnce().
*/
static bool tryStartWithRetries() {
  for (uint8_t i = 0; i < CLOCK_INIT_TRIES; i++) {
    if (tryStartOnce()) {
      return true;
    }
    delay(20);
  }
  return false;
}

// Lee el modulo y actualiza la cache. Devuelve si la lectura fue valida.
static bool refresh() {
  ClockDateTime dt;

  if (!clockRead(dt)) {
    ok = false;
    return false;
  }

  cached   = dt;
  hasCache = true;
  ok       = true;
  return true;
}

bool clockInit() {
  uint32_t now = millis();
  lastTryAt  = now;
  lastReadAt = now;

  started = tryStartWithRetries();
  if (!started) {
    logPrintf("RTC: no responde, sigo sin reloj");
    ok = false;
    return false;
  }

  if (!refresh()) {
    logPrintf("RTC: arranco pero la lectura no es valida");
    return false;
  }

  logPrintf("RTC: %s", clockDateTimeText(cached));
  return true;
}

void clockLoop() {
  uint32_t now = millis();

  if (!started || !ok) {
    // Reintento en caliente: si el problema era un cable flojo o una pila
    // recien cambiada, el reloj se recupera solo sin reiniciar la placa.
    if (now - lastTryAt < CLOCK_RETRY_MS) {
      return;
    }
    lastTryAt  = now;
    lastReadAt = now;

    if (!started) {
      started = tryStartOnce();
      if (!started) {
        return;
      }
      logPrintf("RTC: responde de nuevo");
    }

    if (refresh()) {
      logPrintf("RTC: recuperado -> %s", clockDateTimeText(cached));
    }
    return;
  }

  if (now - lastReadAt < CLOCK_PERIOD_MS) {
    return;
  }
  lastReadAt = now;

  if (!refresh()) {
    lastTryAt = now;
    logPrintf("RTC: se perdio la lectura");
  }
}

bool clockIsOk() {
  return ok;
}

bool clockGet(ClockDateTime &dt) {
  if (!hasCache) {
    return false;
  }
  dt = cached;
  return true;
}

bool clockRead(ClockDateTime &dt) {
  if (!started) {
    return false;
  }
  return rtc.getDateTime(&dt.hour, &dt.minute, &dt.second,
                         &dt.day, &dt.month, &dt.year, &dt.weekday);
}

bool clockSet(uint8_t hour, uint8_t minute, uint8_t second,
              uint8_t day, uint8_t month, uint16_t year) {
  if (!started) {
    started = tryStartOnce();
    if (!started) {
      return false;
    }
  }

  if (!rtc.setDateTime(hour, minute, second, day, month, year,
                       weekdayFromDate(day, month, year))) {
    return false;
  }

  // Refrescar ya, para que quien pregunte despues vea la hora nueva y no la
  // vieja que quedaba en la cache.
  refresh();
  logPrintf("RTC: puesto en hora -> %s", clockDateTimeText(cached));
  return true;
}

const char *clockDateTimeText(const ClockDateTime &dt) {
  // "miércoles 30 de septiembre de 2025, 23:59:59" entra holgado en 64 bytes
  static char text[64];
  char dayName[NAME_LENGTH];
  char monthName[NAME_LENGTH];

  nameFromTable(dayName, dayNames, dt.weekday, DAY_COUNT);
  nameFromTable(monthName, monthNames, (int)dt.month - 1, MONTH_COUNT);

  // Formato español: dia de la semana, dia, mes, anio y hora en 24 h
  snprintf(text, sizeof(text), "%s %u de %s de %u, %02u:%02u:%02u",
           dayName,
           (unsigned)dt.day,
           monthName,
           (unsigned)dt.year,
           (unsigned)dt.hour,
           (unsigned)dt.minute,
           (unsigned)dt.second);

  return text;
}

const char *clockDateTimeText() {
  if (!hasCache) {
    return "Sin lectura del RTC";
  }
  return clockDateTimeText(cached);
}

int16_t clockMinuteOfDay() {
  if (!hasCache) {
    return -1;
  }
  return (int16_t)cached.hour * 60 + cached.minute;
}

bool clockMinuteInRange(uint16_t minute, uint16_t start, uint16_t end) {
  if (start == end) {
    return false;           // ventana de largo cero
  }
  if (start < end) {
    return minute >= start && minute < end;
  }
  return minute >= start || minute < end;   // cruza la medianoche
}

bool clockMinuteCrossed(uint16_t prev, uint16_t now, uint16_t target) {
  if (prev == now) {
    return false;
  }
  if (prev < now) {
    return target > prev && target <= now;
  }
  return target > prev || target <= now;    // paso por medianoche
}
