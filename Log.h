/*
  Log.h - Buffer circular de mensajes, espejado a Serial.

  En la instalacion no hay monitor serie: la placa va a estar enchufada a la
  pared, sin USB. Este modulo guarda las ultimas LOG_LINES lineas en RAM para
  que WebUi las pueda mostrar en el navegador.

  Las lineas se sanitizan al entrar (se quitan comillas, barras y caracteres
  de control), asi que se pueden meter directo en un JSON sin escapar nada.
*/

#ifndef LOG_H
#define LOG_H

#include <Arduino.h>

// Inicializa el buffer. Llamar despues de Serial.begin().
void logInit();

// Agrega una linea. Formato tipo printf. Se imprime tambien por Serial.
void logPrintf(const char *fmt, ...);

// Cantidad de lineas disponibles ahora (0..LOG_LINES).
uint8_t logCount();

// Linea por indice: 0 es la mas antigua de las disponibles.
// Devuelve "" si el indice esta fuera de rango.
const char *logLine(uint8_t index);

// Total de lineas emitidas desde el arranque, sin importar el tamano del
// buffer. Sirve para que el navegador detecte si se perdio algo.
uint32_t logTotal();

#endif // LOG_H
