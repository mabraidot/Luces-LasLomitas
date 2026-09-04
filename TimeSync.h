/*
  TimeSync.h - Sincronizacion de la hora por NTP, con el RTC de respaldo.

  Un DS1302 sano deriva segundos por dia, no un minuto por hora como el de este
  proyecto. Un minuto por hora es un error del 2%, unas 1400 veces lo
  esperable, y apunta a cristal equivocado o modulo clon. Ningun ajuste de
  software lo arregla: habria que corregir la hora todos los dias.

  Como el WiFi ya esta, NTP lo resuelve de raiz. La jerarquia queda asi:

    NTP     sincroniza al conectar y cada 6 h, y escribe al RTC solo si la
            diferencia importa.
    RTC     cubre los cortes de luz y las caidas de WiFi. Es la fuente que lee
            el controlador, SIEMPRE: la logica no necesita saber de donde vino
            la hora ni si hay red.
    Web     el boton "poner en hora" del navegador, respaldo manual.
*/

#ifndef TIMESYNC_H
#define TIMESYNC_H

#include <Arduino.h>

void timeSyncInit();

// No bloquea. Se puede llamar sin conexion sin ningun problema.
void timeSyncLoop();

// Segundos desde la ultima sincronizacion exitosa, o -1 si nunca hubo.
int32_t timeSyncAge();

#endif // TIMESYNC_H
