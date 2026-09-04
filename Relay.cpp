/*
  Relay.cpp - Implementacion de la salida del rele.
*/

#include "Relay.h"
#include "Config.h"

static bool     on      = false;
static uint32_t onSince = 0;

void relayInit() {
  /*
    El orden importa y no es intercambiable.

    digitalWrite() sobre un pin que todavia es entrada carga el latch de
    salida sin conmutar nada. Al llamar despues a pinMode(OUTPUT), el pin sale
    ya con el nivel correcto. Al reves -- pinMode primero -- el pin arranca en
    LOW por defecto, y con logica inversa LOW es "encendido": un pulso corto
    pero real en cada arranque.
  */
  digitalWrite(PIN_RELAY, RELAY_LEVEL_OFF);
  pinMode(PIN_RELAY, OUTPUT);

  on      = false;
  onSince = 0;
}

void relaySet(bool wanted) {
  if (wanted == on) {
    return;
  }

  digitalWrite(PIN_RELAY, wanted ? RELAY_LEVEL_ON : RELAY_LEVEL_OFF);
  on      = wanted;
  onSince = wanted ? millis() : 0;
}

void relayToggle() {
  relaySet(!on);
}

bool relayIsOn() {
  return on;
}

uint32_t relayOnSeconds() {
  if (!on) {
    return 0;
  }
  // La resta en aritmetica sin signo sobrevive la vuelta de millis() a los
  // 49 dias, asi que no hace falta ningun caso especial.
  return (millis() - onSince) / 1000;
}
