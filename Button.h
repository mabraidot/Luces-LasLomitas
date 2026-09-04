/*
  Button.h - Llave de pared en D2, con antirrebote.

  La tecla NO llega al pin como un contacto seco. Es una llave de 220 V cuyo
  retorno alimenta una segunda fuente, y lo que se sensa es la salida de esa
  fuente a traves de un divisor: cerrada = HIGH, abierta = LOW. El esquema
  completo esta en Config.h.

  Es una llave de palanca (latching), no un pulsador: queda trabada en una
  posicion. Por eso el evento se genera en CUALQUIER flanco, de abierta a
  cerrada y de cerrada a abierta. Mover la llave alterna la luz, sin importar
  hacia donde.

  Consecuencia asumida: la posicion de la llave NO indica el estado de la luz,
  porque el reloj, el sensor y la web tambien lo cambian. Un pulsador
  momentaneo eliminaria esa ambiguedad; queda para mas adelante.
*/

#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

/*
  Configura el pin y toma la posicion actual como referencia, SIN generar
  evento. Si generara uno, despues de cada corte de luz la posicion de la
  llave -- que es arbitraria respecto del estado de la luz -- provocaria un
  cambio fantasma al arrancar.
*/
void buttonInit();

// Muestrea la llave. Llamar en cada vuelta de loop(): un digitalRead cuesta
// microsegundos y muestrear seguido da mejor respuesta que limitar la cadencia.
void buttonLoop();

/*
  Devuelve true una sola vez por flanco confirmado, y limpia el evento.
  Quien la llama se hace cargo de actuar.
*/
bool buttonTakeEvent();

// Posicion actual ya filtrada. Solo para diagnostico en la web.
bool buttonIsClosed();

#endif // BUTTON_H
