/*
  Config.h - Pines y constantes de todo el proyecto, en un solo lugar.

  Unica fuente de verdad del conexionado. Ningun otro archivo define pines.
  El esquema del conexionado y el razonamiento de cada eleccion estan en
  README.md.
*/

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/*
  El esquema del conexionado esta en README.md: es documentacion para una
  persona que cablea, no una nota para quien edita pines. Aca quedan solo
  los pines y las constantes, con el motivo de cada eleccion.
*/


// ---- Rele: D1 / GPIO5 ----------------------------------------------------
// D0 (GPIO16) NO sirve para esto: vive en el dominio RTC y su estado durante
// el reset no esta garantizado, lo que con logica inversa significa rele
// activado durante los ~250 ms del bootloader. D1 arranca en alta impedancia
// y el pull-up interno del modulo lo mantiene apagado.
#define PIN_RELAY             D1
#define RELAY_LEVEL_ON        LOW      // logica inversa del modulo
#define RELAY_LEVEL_OFF       HIGH

// ---- Llave de pared: D2 / GPIO4 -----------------------------------------
// Llave de palanca (latching): conmuta el estado en CUALQUIER flanco.
// No puede ir en D8/GPIO15: si quedara trabada en cerrado durante un corte
// de luz, la placa no arrancaria.
//
// NO es un contacto seco: la tecla es de 220 V y su retorno alimenta la
// FUENTE 2, cuya salida llega a D2 por un divisor (ver el esquema arriba).
// Por eso el pin va como INPUT pelado, sin pull-up: el divisor ya define los
// dos estados, y el pull-up interno le pelearia al resistor de abajo.
#define PIN_BUTTON            D2
#define BUTTON_LEVEL_CLOSED   HIGH    // F2 encendida = tecla cerrada

// 300 ms, no 40. Los 40 ms tipicos son para rebote mecanico de contactos, que
// aca no es el mecanismo: el capacitor de salida de la FUENTE 2 hace que las
// dos transiciones tarden cientos de milisegundos, y un arranque suave puede
// cruzar el umbral logico mas de una vez. Con una llave de palanca eso seria
// un doble flanco, o sea dos conmutaciones y ningun cambio neto.
#define BUTTON_DEBOUNCE_MS    300

// ---- LED de estado: D4 / GPIO2 (LED_BUILTIN, logica inversa) -----------
#define PIN_STATUS_LED        D4
#define LED_LEVEL_ON          LOW
#define LED_LEVEL_OFF         HIGH

// ---- LDR: A0 -------------------------------------------------------------
// LDR entre 3V3 y A0, resistencia de 10k entre A0 y GND.
// La lectura sube con la luz.
#define PIN_LDR               A0
#define LDR_SAMPLES           16       // sobremuestreo por ciclo
#define LDR_PERIOD_MS         50
#define LDR_EMA_ALPHA         0.0625f  // 1/16 -> constante de tiempo ~800 ms
#define LDR_READY_MS          2000     // hasta aca el nivel no es confiable
#define LDR_RAW_NIGHT_DEFAULT 15       // provisorio, recalibrar (README.md, calibracion)
#define LDR_RAW_DAY_DEFAULT   1023

// ---- RTC DS1302: D5 / D6 / D7 -------------------------------------------
// Alimentar con 3V3, NO con 5V: D5/D6/D7 no toleran 5 V y las lecturas del
// bus salen corruptas.
//
//    DS1302          NodeMCU
//    GND             GND
//    VCC2            3V3
//    SCLK (CLK)      D5   (GPIO14)
//    I/O  (DAT)      D6   (GPIO12)
//    CE   (RST)      D7   (GPIO13)
#define PIN_RTC_CLK           D5
#define PIN_RTC_IO            D6
#define PIN_RTC_CE            D7
#define CLOCK_PERIOD_MS       500      // el bus es bit-banging, es lento
#define CLOCK_INIT_TRIES      3        // acotado: nunca bloquear el arranque
#define CLOCK_RETRY_MS        10000    // reintento en caliente si no responde

// ---- Red -----------------------------------------------------------------
#define NET_HOSTNAME          "luces"  // aparece en el router y como luces.local
#define NET_CONNECT_TIMEOUT_MS 15000
#define NET_BACKOFF_1_MS      15000UL
#define NET_BACKOFF_2_MS      30000UL
#define NET_BACKOFF_MAX_MS    60000UL
#define NET_RADIO_RESET_TRIES 10       // destrabar la pila WiFi cada N fallos
#define NET_HEARTBEAT_MS      900000UL // latido por Serial cada 15 min
#define NET_BLINK_MS          400      // parpadeo del LED buscando red

// ---- Ajustes: valores por defecto ---------------------------------------
// Solo se usan con la EEPROM virgen. Despues manda lo guardado, que se edita
// desde la web (README.md, ajustes).
#define SET_THRESHOLD_DEFAULT     30    // ~53 cuentas crudas con la calibracion provisoria
#define SET_HYSTERESIS_DEFAULT    10    // en log, equivale a que el crudo suba ~1,5 veces
#define SET_BEDTIME_DEFAULT       (23 * 60)      // 23:00
#define SET_WINDOW_START_DEFAULT  (15 * 60)      // 15:00
#define SET_WINDOW_END_DEFAULT    (4 * 60)       // 04:00
#define SET_MAX_ON_DEFAULT        (12 * 60)      // 12 h; 0 = desactivado

// ---- Controlador ---------------------------------------------------------
#define CTRL_PERIOD_MS            200   // cadencia de evaluacion de reglas
#define CTRL_CONFIRM_MS           30000 // el nivel tiene que sostenerse 30 s
#define CTRL_BOOT_CONFIRM_MS      3000  // ...salvo al arrancar (README.md)
#define CTRL_BOOT_GRACE_MS        20000 // cuanto dura esa ventana de arranque
#define CTRL_DEGRADED_MAX_ON_MIN  360   // sin RTC, apagar a las 6 h (README.md, modo degradado)

// Evento "aclaro": una tormenta que se disipa tiene que apagar las luces.
// La confirmacion es varias veces mas larga que la del atardecer, para que un
// claro pasajero de sol no apague y vuelva a encender un minuto despues. El
// costo de ser lento es lampara encendida de mas; el de ser rapido, conmutar
// el rele de mas.
//
// La proteccion principal contra el ciclado no es este tiempo sino la
// histeresis: para volver a encender, el nivel tiene que caer de
// umbral+histeresis hasta el umbral, que en escala logaritmica son unos 10
// puntos, o sea un factor de 1,5 en el crudo. No alcanza un parpadeo.
#define CTRL_CLEAR_CONFIRM_MS     120000UL

// Medicion del aporte de la propia lampara al sensor. Al encender se compara
// el nivel de antes con el de unos segundos despues; la diferencia es cuanto
// se ilumina el LDR a si mismo. Es lo que acota la histeresis (README.md, evento "aclaro").
// 3 s: la media movil tiene una constante de tiempo de 800 ms, asi que a los
// 3 s ya esta asentada al 98%. Cuanto mas corta la ventana, menos posibilidad
// de que un cambio de luz ambiente se cuele en la medicion.
#define CTRL_BOOST_SETTLE_MS      3000
#define CTRL_BOOST_MARGIN         3     // puntos de margen sobre lo medido

// ---- NTP -----------------------------------------------------------------
// Argentina es UTC-3 fijo, sin horario de verano.
#define TZ_OFFSET_SECONDS         (-3L * 3600L)
#define NTP_SERVER_1              "pool.ntp.org"
#define NTP_SERVER_2              "time.google.com"
#define NTP_PERIOD_MS             21600000UL   // 6 h
#define NTP_FIRST_CHECK_MS        8000UL       // primer intento tras conectar
// Diferencia a partir de la cual se corrige el RTC. No escribirlo al ras: los
// eventos del controlador tienen resolucion de un minuto, asi que un error de
// menos de un minuto no cambia ninguna decision.
#define NTP_MIN_DIFF_SECONDS      60
// Debajo de esto, time(nullptr) todavia no sincronizo (es 2020-09-13).
#define NTP_EPOCH_PLAUSIBLE       1600000000UL

// ---- OTA -----------------------------------------------------------------
// 8266 es el que espera el IDE de Arduino. La contrasena vive en Secrets.h.
#define OTA_PORT              8266

// ---- Web -----------------------------------------------------------------
#define WEB_PORT              80

// ---- Log circular --------------------------------------------------------
// Espeja a Serial y se puede leer desde la web: en la instalacion no hay
// monitor serie, y es la unica forma de ver por que se reinicio la placa.
#define LOG_LINES             24
#define LOG_LINE_LEN          80

#endif // CONFIG_H
