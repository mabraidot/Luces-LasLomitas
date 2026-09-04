/*
  Ota.cpp - Implementacion de la actualizacion por WiFi.
*/

#include "Ota.h"
#include "Config.h"
#include "Secrets.h"
#include "Net.h"
#include "Log.h"

#include <ArduinoOTA.h>

static bool started = false;

void otaInit() {
  ArduinoOTA.setPort(OTA_PORT);
  ArduinoOTA.setHostname(NET_HOSTNAME);

  if (OTA_HAS_PASSWORD) {
    ArduinoOTA.setPassword(OTA_PASSWORD);
  }

  ArduinoOTA.onStart([]() {
    // Se registra por Serial y en el log, aunque el log se pierda al
    // reiniciar: si la subida falla NO hay reinicio, y ahi el mensaje queda.
    logPrintf("OTA: empieza la actualizacion");
  });

  ArduinoOTA.onEnd([]() {
    logPrintf("OTA: terminada, reiniciando");
  });

  ArduinoOTA.onError([](ota_error_t error) {
    const char *text = "?";
    switch (error) {
      case OTA_AUTH_ERROR:    text = "contrasena incorrecta"; break;
      case OTA_BEGIN_ERROR:   text = "no arranco (espacio?)"; break;
      case OTA_CONNECT_ERROR: text = "fallo la conexion";     break;
      case OTA_RECEIVE_ERROR: text = "fallo la recepcion";    break;
      case OTA_END_ERROR:     text = "fallo al cerrar";       break;
    }
    logPrintf("OTA: error %u, %s", (unsigned)error, text);
  });

  started = false;
}

void otaLoop() {
  /*
    ArduinoOTA.begin() necesita la red arriba, asi que se arranca en la primera
    vuelta con conexion en lugar de en setup(). El 'false' es para que no
    arranque su propio responder mDNS: el de Net ya esta corriendo, y dos
    responders en la misma placa se pelean el puerto multicast.
  */
  if (!started) {
    if (!netIsConnected()) {
      return;
    }
    ArduinoOTA.begin(false);
    started = true;
    logPrintf("OTA: escuchando en el puerto %u%s", (unsigned)OTA_PORT,
              OTA_HAS_PASSWORD ? " (con contrasena)" : " (sin contrasena)");
  }

  ArduinoOTA.handle();
}
