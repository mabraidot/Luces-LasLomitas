/*
  LightController.h - La maquina de eventos que decide el estado de la luz.

  Es el unico dueño del rele: ni la web ni la llave lo tocan directo, todo pasa
  por aca. Consume Clock, LightSensor, Button y Settings, y no conoce Net ni
  WebUi -- por eso una falla de red no puede afectar a las luces.

  El diseño completo esta en README.md. Lo esencial:

  Las reglas automaticas son TRANSICIONES, no condiciones continuas. El error
  clasico es evaluar 'if (nivel < umbral) encender(); else apagar();' y eso
  falla de dos formas: el modo manual se vuelve imposible (la condicion revierte
  cualquier accion en el ciclo siguiente) y el sensor se realimenta con la
  propia lampara, oscilando para siempre.

  Los eventos son seis:

    Atardecer   nivel bajo el umbral, sostenido, dentro de la franja de
                encendido y con el disparador armado  -> encender
    Aclaro      nivel sobre umbral+histeresis, sostenido 2 min, y solo si
                estaba OSCURO cuando se encendio      -> apagar y rearmar
    Dormir      el reloj cruza la hora limite         -> apagar
    Ventana     el reloj cruza el fin de la ventana   -> apagar, incondicional
    Manual      web o llave                          -> invertir
    Watchdog    demasiadas horas encendido            -> apagar

  El evento "aclaro" existe por un caso concreto: una tormenta oscurece el
  cielo a las cuatro de la tarde, las luces encienden bien, y si se disipa
  antes del atardecer quedarian prendidas en pleno dia hasta la hora de
  dormir.

  Por eso solo actua si estaba OSCURO al encender. El criterio no es quien
  encendio sino si habia luz en ese momento: quien prende la luz con el cielo
  claro ya sabia que estaba claro, y que aclare no le aporta nada -- prenderla
  de dia a proposito y que se apague sola diez minutos despues seria un aparato
  roto. En cambio quien la prende a oscuras la prendio PORQUE no habia luz, asi
  que apagar cuando vuelve es coherente con esa misma intencion, sea el
  atardecer o una persona a las cinco de la mañana.

  Pero es tambien el evento peligroso, porque es el que puede realimentar: la
  lampara ilumina el sensor, el nivel sube, apaga, el nivel baja, enciende. La
  garantia que lo hace seguro no es una estimacion sino una PRUEBA: cuando el
  evento apaga, el aparato mira que le pasa al nivel tres segundos despues. Si
  se queda alto, era luz de dia y la decision estuvo bien; si cae por debajo de
  umbral+histeresis, lo que lo tenia alto era la lampara, y el evento se
  desactiva hasta que se cambien los ajustes. Cuesta un unico ciclo aprenderlo,
  y la web lo avisa.
*/

#ifndef LIGHTCONTROLLER_H
#define LIGHTCONTROLLER_H

#include <Arduino.h>

void lightControllerInit();

// Evalua las reglas. Llamar seguido desde loop(); se autolimita.
void lightControllerLoop();

/*
  Evento manual: invierte el estado actual y manda hasta el proximo evento.
  La llaman WebUi (toque en el foco) y el sketch (flanco de la llave).
*/
void lightControllerManualToggle();

/*
  Avisa que el reloj SALTO, tras ponerlo en hora por NTP o desde el navegador.
  Resincroniza el rastreador de flancos y reaplica la regla de la hora de
  dormir (ver lightControllerSettingsChanged, es el mismo problema).
*/
void lightControllerClockChanged();

/*
  Avisa que los ajustes cambiaron.

  Hace falta porque los eventos de reloj se detectan por FLANCO: si se mueve la
  hora de dormir a una que ya paso, ese cruce no vuelve a ocurrir nunca y las
  luces se quedarian encendidas hasta el cierre de la ventana. Cambiar un
  ajuste es una intencion explicita del usuario y tiene que tomar efecto ya.
*/
void lightControllerSettingsChanged();

// --- diagnostico para la web ---

// Si el disparador del atardecer esta armado (ver README.md).
bool lightControllerArmed();

// "auto", "sin reloj" o "midiendo", segun en que modo esta trabajando.
const char *lightControllerModeText();

// Motivo del ultimo cambio de estado, para el log de la web.
const char *lightControllerLastReason();

/*
  Estado de la cuenta de confirmacion del atardecer, para que la web pueda
  mostrar que algo esta pasando.

  El atardecer no dispara al cruzar el umbral: el nivel tiene que sostenerse
  bajo 30 segundos, para rechazar los faros de un auto o una nube. Sin
  exponerlo, esos 30 segundos son indistinguibles de un aparato roto.

    >= 0   segundos que faltan para que dispare
    -1     no hay cuenta corriendo (el nivel esta sobre el umbral, o la luz
           ya esta encendida)
    -2     la cuenta corre pero el evento NO va a disparar: el disparador
           esta desarmado, o estamos fuera de la franja de encendido
*/
int16_t lightControllerConfirmLeft();

/*
  Igual que la anterior, pero para el evento "aclaro":

    >= 0   segundos que faltan para apagar
    -1     no hay cuenta corriendo
    -2     la cuenta corre pero el evento esta desactivado: se comprobo que
           apagar por exceso de luz realimenta con la propia lampara
*/
int16_t lightControllerClearLeft();

// Verdadero si se comprobo que apagar por exceso de luz realimenta, y por eso
// el evento "aclaro" quedo desactivado hasta que cambien los ajustes.
bool lightControllerClearUnsafe();

/*
  Cuanto le suma la lampara al sensor, en puntos, segun la ULTIMA medicion.
  Es el numero contra el que hay que dimensionar la histeresis.

  Ojo al leerlo en la mesa de trabajo: la medicion se toma tres segundos
  despues de encender y no distingue la lampara de un cambio de luz ambiente.
  Si se destapa el sensor en esos tres segundos, mide la mano. Para medirlo
  bien hay que dejar el sensor quieto unos segundos despues de que encienda.
*/
uint8_t lightControllerLampBoost();

#endif // LIGHTCONTROLLER_H
