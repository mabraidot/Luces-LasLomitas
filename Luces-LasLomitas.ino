/*
  Luces exteriores - Las Lomitas

  Controlador automatico de luces sobre ESP8266 NodeMCU v1.
  La documentacion completa esta en README.md.

  Este archivo no tiene logica: solo llama a los *Init() y los *Loop(). Toda
  decision sobre el estado de la luz vive en LightController.
*/

#include "Config.h"
#include "Log.h"
#include "Relay.h"
#include "Button.h"
#include "Settings.h"
#include "LightSensor.h"
#include "Clock.h"
#include "LightController.h"
#include "Net.h"
#include "TimeSync.h"
#include "Ota.h"
#include "WebUi.h"

void setup() {
  /*
    Primero de todo, antes incluso de Serial.begin(): el bootloader ya corrio
    unos 250 ms con el pin indefinido, y lo unico que podemos hacer desde el
    software es acortar esa ventana lo mas posible.
  */
  relayInit();

  Serial.begin(115200);
  Serial.println();

  logInit();
  logPrintf("Luces exteriores - Las Lomitas");
  logPrintf("Arranque: %s", ESP.getResetReason().c_str());

  // El orden importa: LightSensor y LightController leen ajustes, y el sensor
  // necesita su calibracion antes de dar el primer nivel.
  settingsLoad();
  clockInit();            // no bloquea aunque el RTC no responda
  lightSensorInit();
  buttonInit();
  lightControllerInit();

  netInit();              // no bloquea aunque no haya router
  otaInit();
  timeSyncInit();
  webUiInit();
}

void loop() {
  // Sin un solo delay() aca. Cada modulo se autolimita con millis().
  netLoop();
  otaLoop();              // primero: una subida en curso manda
  timeSyncLoop();
  webUiLoop();
  buttonLoop();
  lightSensorLoop();
  clockLoop();
  lightControllerLoop();

  // La llave es una fuente de eventos manuales, nada mas: no toca el rele.
  if (buttonTakeEvent()) {
    lightControllerManualToggle();
  }
}
