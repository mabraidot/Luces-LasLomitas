/*
  LightController.cpp - Implementacion de la maquina de eventos.
*/

#include "LightController.h"
#include "Config.h"
#include "Settings.h"
#include "Clock.h"
#include "LightSensor.h"
#include "Relay.h"
#include "Log.h"

/*
  El disparador del atardecer. Es UNA variable, y es lo que hace funcionar todo
  el resto: es la memoria del flanco.

  Arranca ARMADO, y eso no es obvio: si arrancara desarmado, un corte de luz a
  las 21:00 dejaria las luces apagadas toda la noche, porque el nivel ya esta
  bajo y el disparador nunca llegaria a armarse (solo se arma cuando la luz
  SUBE). Ver README.md, arranque en frio.
*/
static bool duskArmed = true;

// Cuando el nivel bajo del umbral por primera vez; 0 = no esta abajo.
static uint32_t belowSince = 0;

// Idem para el evento "aclaro": cuando el nivel supero umbral+histeresis.
static uint32_t aboveSince = 0;

/*
  Aporte medido de la propia lampara al sensor, en puntos de la escala.

  Es la cifra que hace seguro el evento "aclaro". Se mide al encender: se
  guarda el nivel de antes y, tres segundos despues, se compara.

  ES SOLO INFORMATIVO: ya no bloquea nada. Sirve para saber si conviene ponerle
  una visera al sensor. La garantia contra el bucle es 'clearUnsafe', mas abajo.

  ES UNA HEURISTICA Y SE PUEDE ENGAÑAR. La medicion no distingue "la lampara
  sumo luz" de "cambio la luz ambiente"; asume que el ambiente es estable en
  esos tres segundos. Eso vale en un atardecer real, donde el nivel baja del
  orden de un punto por minuto, y NO vale en la mesa de trabajo: destapar el
  sensor justo despues de que encienda mide la mano, no la lampara.

  Por eso se guarda la ULTIMA medicion y no el maximo historico. El maximo era
  la eleccion conservadora, pero convertia un error transitorio en un bloqueo
  permanente: una sola medicion contaminada dejaba el evento "aclaro"
  desactivado hasta reiniciar la placa. Con la ultima, el proximo encendido
  con el sensor quieto corrige el valor solo.

  La contaminacion, ademas, es asimetrica y juega a favor: para equivocarse
  hacia ABAJO -- que es el lado peligroso -- el ambiente tendria que oscurecerse
  tanto como aporta la lampara en esos tres segundos, algo que no pasa ni en
  una tormenta. Equivocarse hacia arriba solo desactiva la funcion, que es
  molesto pero seguro.
*/
static uint8_t  lampBoost     = 0;
static bool     boostPending  = false;
static uint32_t boostAt       = 0;
static uint8_t  boostBaseline = 0;

/*
  Se comprobo que apagar por exceso de luz realimenta con la propia lampara.

  Es la garantia de verdad contra el bucle, y reemplaza a la comparacion contra
  el aporte medido. En lugar de PREDECIR la realimentacion a partir de una
  medicion fragil, se la DETECTA con un experimento que el aparato hace sobre
  si mismo: cuando "aclaro" apaga, mira que le pasa al nivel tres segundos
  despues.

    el nivel se queda alto  -> era luz de dia de verdad, la decision estuvo bien
    el nivel cae por debajo -> lo que lo tenia alto era la lampara: la
                               histeresis no alcanza, y queda probado

  No se puede engañar, porque no depende de interpretar una medicion ambigua:
  es una prueba controlada con una sola variable, la lampara, que el aparato
  mismo maneja. Cuesta un unico ciclo de aprendizaje -- las luces quedan
  apagadas unos 30 s hasta que el atardecer las vuelve a encender -- y despues
  queda resuelto.
*/
static bool     clearUnsafe    = false;
static bool     verifyPending  = false;
static uint32_t verifyAt       = 0;

// Ultimo minuto del dia evaluado, para detectar los flancos de reloj.
// -1 = todavia sin referencia (no disparar nada en la primera vuelta).
static int16_t lastMinute = -1;

/*
  Si estaba oscuro cuando la luz se encendio.

  Es lo que habilita el evento "aclaro", y el criterio no es QUIEN encendio
  sino SI HABIA LUZ en ese momento:

    encendida con el cielo claro  -> que aclare no es informacion nueva; quien
                                     la prendio ya sabia que estaba claro
    encendida a oscuras           -> se prendio PORQUE no habia luz, asi que
                                     cuando vuelve, apagar es coherente con esa
                                     misma intencion

  Cubre de una sola regla los tres casos que importan: el atardecer (siempre a
  oscuras, asi que siempre habilitado), el encendido manual de dia (no se
  apaga solo) y el encendido manual de madrugada (se apaga al amanecer, en
  lugar de quedar hasta que salte el watchdog doce horas despues).

  Es la diferencia con los otros apagados. La hora de dormir y el cierre de
  ventana son eventos de reloj y valen sin importar por que esta encendida.
*/
static bool turnedOnInDark = false;

static uint32_t startAt   = 0;
static uint32_t lastEvalAt = 0;

static const char *lastReason = "arranque";

static void apply(bool on, const char *reason) {
  if (relayIsOn() == on) {
    return;   // el evento ocurrio pero el estado ya era el correcto
  }

  relaySet(on);
  lastReason = reason;
  logPrintf("Luz -> %s (%s)", on ? "ON" : "OFF", reason);

  // Se anota si habia luz o no al encender: es lo que habilita el evento
  // "aclaro" mas tarde. Cualquier apagado lo deja sin efecto.
  turnedOnInDark = on && (lightSensorLevel() < settings().threshold);

  /*
    Las dos cuentas de confirmacion miden "cuanto lleva la condicion sostenida
    desde el ultimo cambio de estado", asi que un cambio las reinicia.
  */
  belowSince = 0;
  aboveSince = 0;

  if (on) {
    // Arranca la medicion del aporte de la lampara.
    boostBaseline = lightSensorLevel();
    boostAt       = millis();
    boostPending  = true;
  } else {
    boostPending = false;
  }
}

/*
  Cierra la medicion del aporte de la lampara, unos segundos despues de
  encender. La luz ambiente tambien cambia en ese rato, pero al atardecer baja
  del orden de un punto por minuto: en ocho segundos la contaminacion es
  despreciable frente al escalon que produce la lampara.
*/
static void measureLampBoost() {
  if (!boostPending) {
    return;
  }
  if (millis() - boostAt < CTRL_BOOST_SETTLE_MS) {
    return;
  }
  boostPending = false;

  uint8_t level = lightSensorLevel();
  if (level <= boostBaseline) {
    return;   // la lampara no le llega al sensor: mejor todavia
  }

  uint8_t boost = (uint8_t)(level - boostBaseline);
  if (boost != lampBoost) {
    lampBoost = boost;
    logPrintf("La lampara le suma %u puntos al sensor", (unsigned)boost);
  }
}

/*
  Cierra la comprobacion de un apagado por "aclaro", tres segundos despues.

  Si al apagar el nivel cayo por debajo de umbral+histeresis, la lectura alta
  venia de la propia lampara y no del cielo: la histeresis no alcanza y el
  evento se desactiva hasta que se cambien los ajustes.
*/
static void verifyClearDecision() {
  if (!verifyPending) {
    return;
  }
  if (millis() - verifyAt < CTRL_BOOST_SETTLE_MS) {
    return;
  }
  verifyPending = false;

  const SettingsData &s = settings();
  if (lightSensorLevel() <= (uint16_t)s.threshold + s.hysteresis) {
    clearUnsafe = true;
    logPrintf("Apagar por luz realimenta: desactivo 'aclaro' (histeresis %u)",
              (unsigned)s.hysteresis);
  }
}

// Franja de encendido automatico: [inicio de ventana, hora de dormir).
// NO es la ventana segura completa: entre la hora de dormir y el fin de la
// ventana el atardecer no puede disparar, ya paso la hora de dormir.
static bool inOnStretch(uint16_t minute) {
  const SettingsData &s = settings();
  return clockMinuteInRange(minute, s.windowStart, s.bedtime);
}

// Cuanto tiene que sostenerse el nivel bajo antes de encender. Los 30 s
// normales rechazan transitorios (faros de un auto, una nube); en el arranque
// no seguimos una transicion sino que leemos un estado estable, asi que
// alcanzan 3 s y "a los pocos segundos" es literal.
static uint32_t confirmMs() {
  if (millis() - startAt < CTRL_BOOT_GRACE_MS) {
    return CTRL_BOOT_CONFIRM_MS;
  }
  return CTRL_CONFIRM_MS;
}

static void evaluateClockEvents(uint16_t minute) {
  const SettingsData &s = settings();

  if (lastMinute < 0) {
    lastMinute = (int16_t)minute;   // primera referencia: sin eventos
    return;
  }
  if ((uint16_t)lastMinute == minute) {
    return;
  }

  uint16_t prev = (uint16_t)lastMinute;
  lastMinute = (int16_t)minute;

  // El cierre de ventana se evalua despues, asi que gana si caen los dos en el
  // mismo minuto. Da igual: los dos apagan.
  if (clockMinuteCrossed(prev, minute, s.bedtime)) {
    apply(false, "hora de dormir");
  }
  if (clockMinuteCrossed(prev, minute, s.windowEnd)) {
    apply(false, "cierre de ventana");
  }
}

static void evaluateLightEvents(bool clockOk, uint16_t minute) {
  const SettingsData &s = settings();
  uint8_t level = lightSensorLevel();

  /*
    Rearme del disparador. La condicion '!relayIsOn()' no es un detalle: bajo
    la galeria la lampara le pega al sensor, asi que con la luz encendida el
    nivel sube sobre umbral+histeresis y el disparador se rearmaria. Entonces
    la hora de dormir apagaria y, medio minuto despues, el atardecer volveria a
    encender -- luces prendidas toda la noche, u oscilando.

    Como la lampara solo puede alterar la lectura cuando esta encendida,
    ignorar el rearme mientras esta prendida quita por completo su influencia.
  */
  if (!relayIsOn() && level > s.threshold + s.hysteresis) {
    if (!duskArmed) {
      duskArmed = true;
      logPrintf("Disparador rearmado (nivel %u)", (unsigned)level);
    }
  }

  if (level >= s.threshold) {
    belowSince = 0;
    return;
  }

  // Esta abajo del umbral: arrancar o continuar la cuenta de confirmacion.
  uint32_t now = millis();
  if (belowSince == 0) {
    belowSince = now;
    return;
  }
  if (now - belowSince < confirmMs()) {
    return;
  }

  if (!duskArmed) {
    return;
  }

  /*
    Sin reloj no se puede evaluar la franja, asi que se la ignora y el apagado
    queda a cargo del watchdog con un maximo reducido (README.md, modo degradado). Es
    preferible a no encender nada y dejar la casa a oscuras esperando que
    alguien note que el RTC murio.
  */
  if (clockOk && !inOnStretch(minute)) {
    return;
  }

  duskArmed = false;
  apply(true, clockOk ? "atardecer" : "atardecer sin reloj");
}

/*
  Evento "aclaro": la tormenta se disipo y volvio la luz de dia, asi que las
  luces no tienen por que seguir encendidas.

  Es el unico evento que apaga por exceso de luz, y por eso es el que podria
  realimentar con la propia lampara. Tres condiciones lo acotan:

    - Solo dentro de la franja de encendido. De noche un nivel alto solo puede
      venir de la lampara o de algo ajeno al cielo, asi que ahi no se evalua.
    - Solo si la histeresis supera el aporte medido de la lampara. Sin eso, el
      evento no dispara: falla hacia el lado seguro (luces encendidas de mas)
      en lugar de oscilar.
    - Confirmacion de 2 minutos, varias veces la del atardecer, para que un
      claro pasajero no apague.

  Al disparar REARMA el disparador del atardecer, para que el atardecer de
  verdad pueda encender mas tarde el mismo dia.
*/
/*
  Condiciones del evento "aclaro", sin la confirmacion de tiempo.

  Antes esto exigia ademas estar dentro de la franja de encendido. Esa guarda
  se saco: el amanecer cae JUSTO fuera de la franja, asi que con ella el evento
  nunca podia apagar de mañana, que es la mitad del problema que resuelve.

  Se puede sacar sin perder nada porque las dos protecciones reales siguen en
  pie -- la histeresis contra el aporte medido de la lampara, y los minutos de
  confirmacion --, y la guarda de la franja era redundante con ellas. De
  paso el evento deja de necesitar el reloj, asi que tambien funciona en modo
  degradado.

  Lo unico que queda sin cubrir es una luz ajena y permanente apuntando al
  sensor. Pero en ese caso el atardecer tampoco dispararia nunca (el nivel no
  bajaria del umbral), asi que un sensor mal ubicado no funciona igual, con
  guarda o sin ella.
*/
static bool clearConditionsHold() {
  if (!relayIsOn() || !turnedOnInDark) {
    return false;
  }

  const SettingsData &s = settings();
  return lightSensorLevel() > (uint16_t)s.threshold + s.hysteresis;
}

static void evaluateClearEvent() {
  if (!clearConditionsHold() || clearUnsafe) {
    aboveSince = 0;
    return;
  }

  uint32_t now = millis();
  if (aboveSince == 0) {
    aboveSince = now;
    return;
  }
  if (now - aboveSince < CTRL_CLEAR_CONFIRM_MS) {
    return;
  }

  duskArmed = true;
  apply(false, "aclaro");

  // Arranca la comprobacion de que esa decision fue correcta.
  verifyPending = true;
  verifyAt      = millis();
}

static void evaluateWatchdog(bool clockOk) {
  uint16_t limit = settings().maxOnMinutes;

  // Sin reloj el watchdog deja de ser una red de seguridad y pasa a ser la
  // logica de apagado, asi que se acorta para aproximar la hora de dormir:
  // encendiendo al atardecer, apaga alrededor de la 1 de la mañana.
  if (!clockOk) {
    limit = CTRL_DEGRADED_MAX_ON_MIN;
  }
  if (limit == 0) {
    return;
  }

  if (relayOnSeconds() >= (uint32_t)limit * 60UL) {
    apply(false, clockOk ? "maximo encendido" : "maximo sin reloj");
  }
}

void lightControllerInit() {
  duskArmed      = true;
  turnedOnInDark = false;
  clearUnsafe    = false;
  verifyPending  = false;
  belowSince = 0;
  aboveSince = 0;

  // El arranque es en frio: no hay medicion previa del aporte de la lampara
  // que valga conservar. Hasta que la luz encienda una vez, el evento "aclaro"
  // trabaja con lampBoost = 0, o sea con la histeresis configurada como unica
  // proteccion.
  lampBoost    = 0;
  boostPending = false;
  lastMinute = -1;
  startAt    = millis();
  lastEvalAt = startAt;
  lastReason = "arranque";
}

void lightControllerLoop() {
  uint32_t now = millis();
  if (now - lastEvalAt < CTRL_PERIOD_MS) {
    return;
  }
  lastEvalAt = now;

  /*
    Compuerta de arranque: no evaluar nada hasta que el sensor este asentado.
    Con la media movil todavia sin sembrar, el nivel informado puede parecer
    "oscuridad total" y encender las luces al mediodia.
  */
  if (!lightSensorReady()) {
    return;
  }

  bool clockOk = clockIsOk();
  int16_t minuteNow = clockMinuteOfDay();
  uint16_t minute = 0;

  if (clockOk && minuteNow >= 0) {
    minute = (uint16_t)minuteNow;
    evaluateClockEvents(minute);
  } else {
    // Sin reloj no hay referencia de flanco valida: al recuperarse hay que
    // volver a sembrarla en lugar de comparar contra un minuto viejo.
    lastMinute = -1;
    clockOk = false;
  }

  measureLampBoost();
  verifyClearDecision();
  evaluateLightEvents(clockOk, minute);
  evaluateClearEvent();
  evaluateWatchdog(clockOk);
}

void lightControllerManualToggle() {
  bool wanted = !relayIsOn();

  /*
    Un apagado manual NO rearma el disparador: el rearme necesita que el nivel
    suba sobre umbral+histeresis, o sea que vuelva luz de dia de verdad. Es lo
    que hace que apagar a mano a las 20:00 mantenga las luces apagadas el resto
    de la noche, en lugar de que el atardecer las vuelva a encender.
  */
  apply(wanted, "manual");
}

/*
  Reaplica la regla de la hora de dormir en forma de ESTADO, no de flanco.

  Los eventos de reloj se detectan por cruce, y eso deja un hueco cuando lo que
  cambia no es la hora sino la REFERENCIA: mover la hora de dormir a una ya
  pasada, o corregir el RTC saltando por encima de ella. En los dos casos el
  cruce no ocurre nunca y las luces se quedan encendidas hasta el cierre de la
  ventana.

  Es una excepcion deliberada al diseño por eventos de la seccion 4, y esta
  acotada de dos maneras para que no reintroduzca los problemas que ese diseño
  evita:

    - Solo puede APAGAR, nunca encender. Asi no hay forma de que realimente
      con el sensor ni de que pelee con un encendido manual para prenderse.
    - Solo corre cuando cambian los ajustes o salta el reloj, no en cada
      vuelta. Sigue sin ser una condicion continua.

  El tramo que interesa es [hora de dormir, fin de ventana): ahi las luces
  deben estar apagadas si o si. Fuera de ese tramo no se toca nada -- en
  particular, un encendido manual de las 10 de la mañana sobrevive a que se
  edite cualquier ajuste, porque las 10 no estan en ese tramo.
*/
static void enforceBedtime(const char *reason) {
  if (!clockIsOk()) {
    return;
  }

  int16_t minuteNow = clockMinuteOfDay();
  if (minuteNow < 0) {
    return;
  }

  const SettingsData &s = settings();
  if (clockMinuteInRange((uint16_t)minuteNow, s.bedtime, s.windowEnd)) {
    apply(false, reason);
  }
}

void lightControllerClockChanged() {
  // La referencia de flanco ya no sirve: comparar contra el minuto anterior
  // al salto dispararia eventos que no ocurrieron.
  lastMinute = -1;
  logPrintf("Controlador: referencia de hora resincronizada");

  enforceBedtime("hora corregida");
}

void lightControllerSettingsChanged() {
  /*
    Guardar ajustes descarta la medicion del aporte de la lampara.

    Es la via de escape cuando quedo contaminada: el aviso de histeresis
    insuficiente aparece justo en el panel de Ajustes, asi que tocar Guardar
    -- que es lo que uno hace al leerlo -- tambien limpia el valor. Se vuelve a
    medir en el proximo encendido, tres segundos despues.

    No abre una ventana de riesgo: para que "aclaro" dispare, la luz tiene que
    estar encendida y el nivel alto durante dos minutos, y la medicion nueva
    llega a los tres segundos de encender. Siempre mide antes de poder apagar.
  */
  lampBoost = 0;
  if (clearUnsafe) {
    clearUnsafe = false;
    logPrintf("Ajustes nuevos: vuelvo a probar el apagado por luz");
  }

  enforceBedtime("ajustes");
}

bool lightControllerArmed() {
  return duskArmed;
}

const char *lightControllerModeText() {
  if (!lightSensorReady()) {
    return "midiendo";
  }
  if (!clockIsOk()) {
    return "sin reloj";
  }
  return "automatico";
}

const char *lightControllerLastReason() {
  return lastReason;
}

int16_t lightControllerClearLeft() {
  // Mismas condiciones que el evento, en el mismo orden: la web no debe
  // mostrar una cuenta regresiva que no va a llegar a ninguna parte.
  if (!clearConditionsHold()) {
    return -1;
  }
  if (clearUnsafe) {
    return -2;
  }
  if (aboveSince == 0) {
    return (int16_t)(CTRL_CLEAR_CONFIRM_MS / 1000);
  }

  uint32_t elapsed = millis() - aboveSince;
  if (elapsed >= CTRL_CLEAR_CONFIRM_MS) {
    return 0;
  }
  return (int16_t)((CTRL_CLEAR_CONFIRM_MS - elapsed + 999) / 1000);
}

uint8_t lightControllerLampBoost() {
  return lampBoost;
}

bool lightControllerClearUnsafe() {
  return clearUnsafe;
}

int16_t lightControllerConfirmLeft() {
  if (belowSince == 0) {
    return -1;
  }

  /*
    Con la luz ya encendida no hay nada que esperar. Hace falta decirlo aparte
    porque 'belowSince' sigue en pie despues de que el evento disparo -- el
    nivel sigue bajo toda la noche -- y sin esto la web mostraria una cuenta
    trabada para siempre.
  */
  if (relayIsOn()) {
    return -1;
  }

  // La cuenta corre, pero puede no llevar a ninguna parte.
  if (!duskArmed) {
    return -2;
  }
  if (clockIsOk()) {
    int16_t minuteNow = clockMinuteOfDay();
    if (minuteNow >= 0 && !inOnStretch((uint16_t)minuteNow)) {
      return -2;
    }
  }

  uint32_t elapsed = millis() - belowSince;
  uint32_t need    = confirmMs();
  if (elapsed >= need) {
    return 0;
  }
  return (int16_t)((need - elapsed + 999) / 1000);   // redondeo hacia arriba
}
