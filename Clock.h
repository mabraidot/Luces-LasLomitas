/*
  Clock.h - Manejo del RTC DS1302 para el proyecto Luces LasLomitas.

  Encapsula toda la logica del reloj. El sketch principal solo necesita:
    clockInit()          -> una vez en setup(), NO bloquea
    clockLoop()          -> desde loop(); refresca la cache y reintenta
    clockGet()           -> ultima lectura buena, en campos numericos
    clockIsOk()          -> si el modulo esta respondiendo
    clockDateTimeText()  -> texto en español listo para mostrar
    clockSet()           -> poner el reloj en hora

  El conexionado y los pines estan en Config.h.
*/

#ifndef CLOCK_H
#define CLOCK_H

#include <Arduino.h>

// Fecha y hora leidas del RTC
struct ClockDateTime {
  uint8_t  hour;      // 0..23
  uint8_t  minute;    // 0..59
  uint8_t  second;    // 0..59
  uint8_t  day;       // 1..31
  uint8_t  month;     // 1..12
  uint16_t year;      // 2000..2099
  uint8_t  weekday;   // 0 = domingo ... 6 = sabado
};

/*
  Inicializa el RTC y arranca el oscilador si estaba detenido.
  Devuelve true si el reloj quedo funcionando.

  IMPORTANTE: no bloquea. Prueba CLOCK_INIT_TRIES veces y sigue de largo.
  La version anterior tenia un 'while (!rtc.begin()) delay(3000);' que con el
  modulo desconectado o la pila agotada dejaba la placa colgada para siempre
  dentro de setup(): sin rele, sin sensor y sin web. Las luces son la funcion
  principal del aparato y no pueden depender de que el reloj este sano; si el
  RTC no aparece, el controlador trabaja en modo degradado.
*/
bool clockInit();

/*
  Refresca la cache interna cada CLOCK_PERIOD_MS y, si el modulo no responde,
  reintenta cada CLOCK_RETRY_MS. Llamar seguido desde loop(); se autolimita.
  El bus del DS1302 es bit-banging: leerlo a cada vuelta seria un desperdicio.
*/
void clockLoop();

// Verdadero si la ultima lectura fue valida.
bool clockIsOk();

/*
  Ultima lectura buena, sin tocar el bus. Es la forma normal de consultar la
  hora. Devuelve false si todavia no hay ninguna lectura valida.
*/
bool clockGet(ClockDateTime &dt);

/*
  Lectura directa del modulo. Devuelve false si no es valida (modulo
  desconectado, pila agotada o datos corruptos). Normalmente conviene
  clockGet(), que no toca el bus.
*/
bool clockRead(ClockDateTime &dt);

/*
  Ajusta la fecha y hora del RTC. El dia de la semana se calcula solo.
  Los campos van en hora LOCAL, no en UTC.
  Ejemplo: clockSet(12, 34, 56, 31, 12, 2024);

  Ojo: despues de llamar a esto el reloj salta, asi que hay que resincronizar
  el rastreador de flancos del controlador sin disparar eventos.
*/
bool clockSet(uint8_t hour, uint8_t minute, uint8_t second,
              uint8_t day, uint8_t month, uint16_t year);

/*
  Devuelve la fecha y hora como texto en español, con este formato:
    "Domingo 31 de diciembre de 2024, 12:34:56"
  Si no hay lectura valida devuelve un mensaje de error.
  El puntero apunta a un buffer interno: usarlo o copiarlo antes de
  la proxima llamada.
*/
const char *clockDateTimeText();

// Igual que la anterior, pero formatea una lectura ya obtenida.
const char *clockDateTimeText(const ClockDateTime &dt);

// Minutos desde medianoche de la ultima lectura buena, o -1 si no hay.
// Es la unidad en la que se configuran la hora limite y la ventana segura.
int16_t clockMinuteOfDay();

/*
  Verdadero si 'minute' cae en el intervalo [start, end), manejando la vuelta
  por medianoche: con start=900 (15:00) y end=240 (04:00), las 23:00 caen
  dentro y las 10:00 no.
*/
bool clockMinuteInRange(uint16_t minute, uint16_t start, uint16_t end);

/*
  Verdadero si 'target' quedo dentro del intervalo (prev, now], tambien con
  vuelta por medianoche. Es la deteccion de flanco de los eventos de reloj:
  evaluando cada 200 ms no se pierde ningun minuto.
*/
bool clockMinuteCrossed(uint16_t prev, uint16_t now, uint16_t target);

#endif // CLOCK_H
