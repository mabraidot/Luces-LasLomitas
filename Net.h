/*
  Net.h - Conexion WiFi, no bloqueante.

  Regla de diseño: la red es OPCIONAL. El aparato tiene que encender y apagar
  las luces igual de bien sin WiFi, sin internet y sin router. Nada de este
  modulo bloquea, y nunca reinicia la placa por problemas de red: un reinicio
  hace clic en el rele y pierde el estado. Que no haya WiFi es aceptable;
  perder el control de las luces, no.
*/

#ifndef NET_H
#define NET_H

#include <Arduino.h>
#include <IPAddress.h>

// Arranca la conexion y vuelve enseguida. No espera la asociacion.
void netInit();

// Maneja reintentos, reconexion, mDNS y el LED de estado. No bloquea.
void netLoop();

bool netIsConnected();

// Direccion asignada, o 0.0.0.0 si no hay conexion.
IPAddress netIP();

// Potencia de señal en dBm, o 0 si no hay conexion.
int32_t netRssi();

// Texto corto del estado actual, para mostrar en la web.
const char *netStateText();

#endif // NET_H
