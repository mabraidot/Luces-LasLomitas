/*
  Log.cpp - Implementacion del buffer circular de mensajes.
*/

#include "Log.h"
#include "Config.h"

#include <stdarg.h>

static char     lines[LOG_LINES][LOG_LINE_LEN];
static uint8_t  next  = 0;   // donde se escribe la proxima linea
static uint8_t  count = 0;   // cuantas hay disponibles (se satura en LOG_LINES)
static uint32_t total = 0;

/*
  Deja el texto listo para meterse en un JSON sin escapar: saca comillas,
  barras invertidas y cualquier caracter de control. Es mas barato hacerlo una
  vez al guardar que en cada pedido HTTP.
*/
static void sanitize(char *text) {
  for (char *c = text; *c; c++) {
    if (*c == '"' || *c == '\\' || (uint8_t)*c < 0x20) {
      *c = ' ';
    }
  }
}

void logInit() {
  next  = 0;
  count = 0;
  total = 0;
  lines[0][0] = '\0';
}

void logPrintf(const char *fmt, ...) {
  char *target = lines[next];

  va_list args;
  va_start(args, fmt);
  vsnprintf(target, LOG_LINE_LEN, fmt, args);
  va_end(args);

  sanitize(target);

  // Marca de tiempo relativa al arranque: en la instalacion no hay otra forma
  // de saber si un mensaje es de hace un rato o de hace tres dias.
  Serial.printf("[%8lu] %s\n", millis() / 1000, target);

  next = (uint8_t)((next + 1) % LOG_LINES);
  if (count < LOG_LINES) {
    count++;
  }
  total++;
}

uint8_t logCount() {
  return count;
}

const char *logLine(uint8_t index) {
  if (index >= count) {
    return "";
  }

  // 'next' apunta a la posicion libre; con el buffer lleno, esa misma posicion
  // es la mas antigua.
  uint8_t oldest = (count < LOG_LINES) ? 0 : next;
  return lines[(oldest + index) % LOG_LINES];
}

uint32_t logTotal() {
  return total;
}
