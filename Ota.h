/*
  Ota.h - Actualizacion de firmware por WiFi, desde el IDE de Arduino.

  Una vez conectada, la placa aparece en Herramientas > Puerto > Puertos de red
  como "luces". Se selecciona ahi y se sube como si estuviera por USB.

  El anuncio por mDNS lo hace Net, que es el dueño de mDNS en este proyecto:
  este modulo llama a ArduinoOTA.begin(false) para no arrancar un segundo
  responder que le pelearia al de Net.

  DOS COSAS QUE CONVIENE SABER ANTES DE USARLO:

  1. Durante la subida, loop() se detiene. El rele mantiene su estado -- es un
     contacto mecanico, no depende del software -- pero no se evalua ningun
     evento hasta que termine.

  2. Al terminar, la placa se reinicia, y eso pasa por relayInit(): el rele
     queda APAGADO y el controlador vuelve a resolver desde cero. O sea que
     actualizar de noche apaga las luces y las vuelve a encender unos cinco
     segundos despues. No es un error, es el arranque en frio que
     describe el README. Conviene actualizar de dia.

  Despues de una actualizacion, el diagnostico va a mostrar "Software/System
  restart" como motivo del ultimo reinicio. Eso es normal y no hay que
  confundirlo con un brownout de la bobina.
*/

#ifndef OTA_H
#define OTA_H

#include <Arduino.h>

// Configura nombre, contraseña y avisos. No arranca nada todavia: hace falta
// que haya WiFi, y de eso se encarga otaLoop().
void otaInit();

// Atiende la actualizacion. Llamar seguido desde loop(): mientras no hay
// subida en curso no cuesta practicamente nada.
void otaLoop();

#endif // OTA_H
