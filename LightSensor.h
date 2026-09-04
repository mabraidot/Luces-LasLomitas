/*
  LightSensor.h - Lectura del LDR en A0.

  Tres capas: sobremuestreo, media movil exponencial, y mapeo logaritmico a
  una escala de 0 a 100. El razonamiento esta en README.md.
*/

#ifndef LIGHTSENSOR_H
#define LIGHTSENSOR_H

#include <Arduino.h>

// Configura el sensor. Requiere que settingsLoad() ya haya corrido.
void lightSensorInit();

// Muestrea. Llamar seguido desde loop(); se autolimita a LDR_PERIOD_MS.
void lightSensorLoop();

/*
  Falso durante los primeros LDR_READY_MS, mientras la media se estabiliza.
  El controlador no debe evaluar ninguna regla hasta que esto sea verdadero:
  al arrancar al mediodia, una media todavia sin asentar puede parecer
  "oscuridad total" y disparar un encendido que no corresponde.
*/
bool lightSensorReady();

// Crudo suavizado en la escala de siempre, 0 a 1023. Para diagnostico.
uint16_t lightSensorRaw();

/*
  Nivel de luz de 0 (noche cerrada) a 100 (dia pleno), en escala
  logaritmica: cada duplicacion del crudo suma una cantidad fija de puntos.
  Es la escala en la que se configura el umbral.
*/
uint8_t lightSensorLevel();

/*
  Relee la calibracion desde Settings y recalcula la escala.

  Llamar despues de cada settingsSave(). La calibracion vive en Settings, que
  es la unica fuente de verdad; este modulo solo cachea los dos logaritmos
  precalculados para no hacerlos en cada muestra.

  Como se toman los valores:
    rawNight  de noche y CON LA LAMPARA APAGADA. Con las luces encendidas se
              estaria midiendo la propia lampara.
    rawDay    al mediodia, con el sensor ya montado en su lugar definitivo. No
              sirve el valor medido a cielo abierto: bajo la galeria da
              bastante menos, y calibrar alto comprime todo el rango real.
*/
void lightSensorReloadCalibration();

#endif // LIGHTSENSOR_H
