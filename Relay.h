/*
  Relay.h - Salida del rele, con la logica inversa encapsulada.

  Nadie mas toca PIN_RELAY. El resto del programa habla de "encendido" y
  "apagado"; que eso sea LOW o HIGH es problema de este modulo.
*/

#ifndef RELAY_H
#define RELAY_H

#include <Arduino.h>

/*
  Deja el rele apagado y configura el pin. Tiene que ser la PRIMERA linea de
  setup(), antes incluso de Serial.begin(): ver el comentario en Relay.cpp.
*/
void relayInit();

// Enciende o apaga. Si ya estaba en ese estado, no hace nada.
void relaySet(bool on);

// Invierte el estado actual.
void relayToggle();

bool relayIsOn();

/*
  Segundos que el rele lleva encendido sin interrupcion, o 0 si esta apagado.
  Lo usa el watchdog de maximo encendido del controlador.
*/
uint32_t relayOnSeconds();

#endif // RELAY_H
