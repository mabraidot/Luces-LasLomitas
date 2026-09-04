/*
  Button.cpp - Implementacion de la llave de pared.
*/

#include "Button.h"
#include "Config.h"

// La tecla se sensa por el divisor de la FUENTE 2 (ver Config.h): abierta
// lee LOW, cerrada lee HIGH. Al reves de lo que daria un contacto seco con
// pull-up interno.
#define RAW_CLOSED(level)   ((level) == BUTTON_LEVEL_CLOSED)

static bool     stableClosed  = false;  // posicion filtrada
static bool     candidate     = false;  // ultima lectura cruda distinta
static uint32_t candidateAt   = 0;      // cuando aparecio esa lectura
static bool     eventPending  = false;

void buttonInit() {
  // INPUT pelado: el divisor externo define los dos niveles, y el pull-up
  // interno (unos 40k a 3V3) levantaria el nivel de "abierta" peleandole al
  // resistor de abajo del divisor.
  pinMode(PIN_BUTTON, INPUT);

  // Una lectura inmediata define la referencia. Sin evento: ver Button.h.
  stableClosed = RAW_CLOSED(digitalRead(PIN_BUTTON));
  candidate    = stableClosed;
  candidateAt  = millis();
  eventPending = false;
}

void buttonLoop() {
  bool raw = RAW_CLOSED(digitalRead(PIN_BUTTON));
  uint32_t now = millis();

  if (raw != candidate) {
    // Cambio la lectura cruda: arranca de nuevo la cuenta del antirrebote.
    candidate   = raw;
    candidateAt = now;
    return;
  }

  if (raw == stableClosed) {
    return;   // nada nuevo
  }

  // La lectura se mantuvo distinta de la estable el tiempo suficiente.
  if (now - candidateAt >= BUTTON_DEBOUNCE_MS) {
    stableClosed = raw;
    eventPending = true;
  }
}

bool buttonTakeEvent() {
  if (!eventPending) {
    return false;
  }
  eventPending = false;
  return true;
}

bool buttonIsClosed() {
  return stableClosed;
}
