/*
  WebUi.h - Servidor web y sus rutas.

  Usa el ESP8266WebServer del core, sincronico. Cero dependencias extra, y para
  un cliente que hace un GET cada 2 s va sobrado.

  Rutas:
    GET  /   -> la pagina (PROGMEM, cacheada)
    GET  /s  -> estado, JSON chico
    GET  /l  -> ultimas lineas del log
    POST /t  -> alternar la luz
*/

#ifndef WEBUI_H
#define WEBUI_H

#include <Arduino.h>

void webUiInit();

// Atiende clientes. Llamar en cada vuelta de loop(): es inofensivo sin WiFi.
void webUiLoop();

#endif // WEBUI_H
