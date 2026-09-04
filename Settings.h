/*
  Settings.h - Configuracion persistente en EEPROM.

  Es la unica fuente de verdad de los ajustes: el resto de los modulos los leen
  de aca, no los guardan por su cuenta. Se editan desde la web, no se
  recompilan.

  El bloque guardado lleva magic + version + CRC. Si la EEPROM esta virgen o
  corrupta se cargan los valores por defecto en lugar de arrancar con basura, y
  el campo de version permite agregar ajustes mas adelante sin perder los que
  ya estaban.
*/

#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

struct SettingsData {
  uint8_t  threshold;     // umbral de luz, 0..100 en la escala logaritmica
  uint8_t  hysteresis;    // puntos que tiene que subir el nivel para rearmar
  uint16_t bedtime;       // hora de dormir, minutos desde medianoche
  uint16_t windowStart;   // inicio de la ventana segura, idem
  uint16_t windowEnd;     // fin de la ventana segura, idem
  uint16_t maxOnMinutes;  // maximo encendido continuo; 0 = desactivado
  uint16_t rawNight;      // calibracion: crudo de noche, lampara apagada
  uint16_t rawDay;        // calibracion: crudo al mediodia, en su posicion
};

// Carga desde EEPROM, o los valores por defecto si no hay nada valido.
// Llamar antes que cualquier modulo que use ajustes.
void settingsLoad();

// Ajustes actuales. Solo lectura.
const SettingsData &settings();

/*
  Valida, recorta y guarda. Devuelve true si algo cambio y se escribio.

  Solo escribe la flash cuando hay un cambio real: la EEPROM emulada reescribe
  todo un sector en cada commit(), asi que guardar valores identicos seria
  desgaste puro.
*/
bool settingsSave(const SettingsData &next);

// Vuelve a los valores por defecto y los guarda.
void settingsReset();

#endif // SETTINGS_H
