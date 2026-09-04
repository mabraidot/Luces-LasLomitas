/*
  WebUi.cpp - Implementacion del servidor web.

  Todo lo que se manda se arma con snprintf sobre un buffer static. Nunca con
  String: fragmenta el heap y a las horas de uptime la ESP se reinicia sola.
  Es la causa numero uno de reinicios misteriosos en proyectos ESP8266 con
  servidor web.
*/

#include "WebUi.h"
#include "Config.h"
#include "Page.h"
#include "Icon.h"
#include "Clock.h"
#include "LightSensor.h"
#include "Relay.h"
#include "Button.h"
#include "Net.h"
#include "Log.h"
#include "Settings.h"
#include "LightController.h"
#include "TimeSync.h"

#include <ESP8266WebServer.h>
#include <stdarg.h>

static ESP8266WebServer server(WEB_PORT);

// El motivo del reinicio se captura una sola vez: getResetReason() devuelve un
// String, y no conviene llamarlo en cada pedido HTTP.
static char resetReason[24] = "?";

static void sendStatus() {
  static char status[600];
  char ip[16];

  /*
    La deriva del RTC viaja como texto para poder mandar null: sin medicion no
    hay numero que sirva de sentinela, porque una diferencia enorme es un valor
    legitimo (un RTC parado en 1970 da mil millones).
  */
  char ntpDiff[16] = "null";
  int32_t diff;
  if (timeSyncDiff(diff)) {
    snprintf(ntpDiff, sizeof(ntpDiff), "%ld", (long)diff);
  }

  IPAddress addr = netIP();
  snprintf(ip, sizeof(ip), "%u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);

  const SettingsData &cfg = settings();

  snprintf(status, sizeof(status),
           "{\"on\":%u,\"l\":%u,\"r\":%u,\"rdy\":%u,\"sw\":%u,"
           "\"t\":\"%s\",\"rtc\":%u,\"arm\":%u,\"mode\":\"%s\","
           "\"why\":\"%s\",\"cd\":%d,\"cu\":%d,\"lb\":%u,\"cs\":%u,\"ntp\":%ld,"
           "\"ntpd\":%s,"
           "\"th\":%u,\"hy\":%u,\"bt\":%u,\"w0\":%u,\"w1\":%u,"
           "\"mx\":%u,\"rn\":%u,\"rd\":%u,"
           "\"ip\":\"%s\",\"db\":%ld,\"up\":%lu,\"heap\":%u,"
           "\"rst\":\"%s\"}",
           relayIsOn() ? 1u : 0u,
           (unsigned)lightSensorLevel(),
           (unsigned)lightSensorRaw(),
           lightSensorReady() ? 1u : 0u,
           buttonIsClosed() ? 1u : 0u,
           clockDateTimeText(),
           clockIsOk() ? 1u : 0u,
           lightControllerArmed() ? 1u : 0u,
           lightControllerModeText(),
           lightControllerLastReason(),
           (int)lightControllerConfirmLeft(),
           (int)lightControllerClearLeft(),
           (unsigned)lightControllerLampBoost(),
           lightControllerClearUnsafe() ? 1u : 0u,
           (long)timeSyncAge(),
           ntpDiff,
           (unsigned)cfg.threshold,
           (unsigned)cfg.hysteresis,
           (unsigned)cfg.bedtime,
           (unsigned)cfg.windowStart,
           (unsigned)cfg.windowEnd,
           (unsigned)cfg.maxOnMinutes,
           (unsigned)cfg.rawNight,
           (unsigned)cfg.rawDay,
           ip,
           (long)netRssi(),
           millis() / 1000UL,
           (unsigned)ESP.getFreeHeap(),
           resetReason);

  server.send(200, F("application/json"), status);
}

/*
  Manifiesto de aplicacion web. Es lo que hace que Android use el icono del
  foco y el fondo de la pagina al agregar el acceso directo, en lugar de una
  captura recortada.

  'purpose: any maskable' le dice a Android que puede recortar el icono en
  circulo o en cuadrado redondeado: el dibujo esta dentro de la zona segura y
  el resto se rellena con el mismo fondo, sin cuadrado blanco alrededor.
*/
static const char MANIFEST_JSON[] PROGMEM =
  "{\"name\":\"Las Lomitas\",\"short_name\":\"Las Lomitas\","
  "\"description\":\"Luces exteriores\",\"start_url\":\"/\","
  "\"display\":\"standalone\",\"orientation\":\"portrait\","
  "\"background_color\":\"#0d1117\",\"theme_color\":\"#0d1117\","
  "\"icons\":[{\"src\":\"/i.png\",\"sizes\":\"512x512\","
  "\"type\":\"image/png\",\"purpose\":\"any maskable\"}]}";

static void handleRoot() {
  /*
    La pagina NO se cachea, a proposito.

    El uso desde el celular es esporadico: pueden pasar semanas sin que nadie
    abra la web mientras las luces funcionan solas. Con ese patron, cualquier
    max-age razonable vence antes del proximo uso, asi que la cache no ahorra
    nada -- y en cambio garantiza confusion cada vez que se reprograma la
    placa, porque el celular puede seguir mostrando la version anterior.

    El costo de no cachear son unos 5 kB por apertura, unos 50 ms en la red
    local. El icono y el manifiesto SI se cachean: esos se piden en cada carga
    de la pagina y pesan tres veces mas que ella.
  */
  server.sendHeader(F("Cache-Control"), F("no-store"));
  server.send_P(200, PSTR("text/html; charset=utf-8"), PAGE_HTML);
}

static void handleManifest() {
  server.sendHeader(F("Cache-Control"), F("public, max-age=86400"));
  server.send_P(200, PSTR("application/manifest+json"), MANIFEST_JSON);
}

static void handleIcon() {
  server.sendHeader(F("Cache-Control"), F("public, max-age=604800"));
  server.send_P(200, PSTR("image/png"),
                (const char *)ICON_PNG, (size_t)ICON_PNG_LEN);
}

static void handleToggle() {
  // Nadie fuera de LightController toca Relay: el toque en el foco es un
  // evento manual como el de la llave, y el controlador es el que decide.
  lightControllerManualToggle();
  sendStatus();
}

/*
  Lee un entero de la query string. Si el parametro no vino, devuelve el valor
  actual: asi se puede mandar un solo ajuste sin pisar los demas.
*/
static uint16_t argOr(const char *name, uint16_t fallback) {
  if (!server.hasArg(name)) {
    return fallback;
  }
  long value = server.arg(name).toInt();
  if (value < 0) {
    return 0;
  }
  if (value > 65535) {
    return 65535;
  }
  return (uint16_t)value;
}

static void handleConfig() {
  const SettingsData &cur = settings();
  SettingsData next;

  next.threshold    = (uint8_t)argOr("th", cur.threshold);
  next.hysteresis   = (uint8_t)argOr("hy", cur.hysteresis);
  next.bedtime      = argOr("bt", cur.bedtime);
  next.windowStart  = argOr("w0", cur.windowStart);
  next.windowEnd    = argOr("w1", cur.windowEnd);
  next.maxOnMinutes = argOr("mx", cur.maxOnMinutes);
  next.rawNight     = argOr("rn", cur.rawNight);
  next.rawDay       = argOr("rd", cur.rawDay);

  if (settingsSave(next)) {
    // La calibracion pudo cambiar los extremos de la escala: hay que
    // recalcular los logaritmos que LightSensor tiene precalculados.
    lightSensorReloadCalibration();

    // Y los horarios pudieron cambiar: si la hora de dormir quedo en el
    // pasado, su cruce ya no va a ocurrir y hay que aplicarla ahora.
    lightControllerSettingsChanged();
  }

  sendStatus();
}

/*
  Poner el reloj en hora desde el navegador.

  Los campos llegan ya en hora LOCAL, no como epoch. new Date().getTime()
  devuelve UTC, y convertirlo en el micro es aritmetica extra con el riesgo
  clasico de equivocarse en el signo del desplazamiento.
*/
static void handleSetTime() {
  if (!server.hasArg("h") || !server.hasArg("y")) {
    server.send(400, F("text/plain"), F("faltan parametros"));
    return;
  }

  uint16_t hour   = argOr("h",  0);
  uint16_t minute = argOr("i",  0);
  uint16_t second = argOr("s",  0);
  uint16_t day    = argOr("d",  1);
  uint16_t month  = argOr("mo", 1);
  uint16_t year   = argOr("y",  2000);

  if (hour > 23 || minute > 59 || second > 59
      || day < 1 || day > 31 || month < 1 || month > 12
      || year < 2000 || year > 2099) {
    server.send(400, F("text/plain"), F("fecha invalida"));
    return;
  }

  if (!clockSet((uint8_t)hour, (uint8_t)minute, (uint8_t)second,
                (uint8_t)day, (uint8_t)month, year)) {
    server.send(500, F("text/plain"), F("el RTC no responde"));
    return;
  }

  // El reloj salto: resincronizar la referencia de flancos sin disparar
  // eventos, o corregir la hora a las 22:50 apagaria al instante.
  lightControllerClockChanged();
  sendStatus();
}

static char    json[LOG_LINES * (LOG_LINE_LEN + 4) + 48];
static size_t  jsonLen = 0;

/*
  Agrega al buffer sin pasarse nunca.

  snprintf devuelve lo que HABRIA escrito, no lo que escribio. Acumular ese
  valor a ciegas ('n += snprintf(buf + n, sizeof(buf) - n, ...)') es un error
  clasico: si en algun momento trunca, n queda mayor que el buffer y en la
  llamada siguiente 'sizeof(buf) - n' se desborda en size_t, dando un tamano
  gigante y una escritura fuera de rango. Aca se recorta despues de cada uso.
*/
static void jsonAppend(const char *fmt, ...) {
  if (jsonLen >= sizeof(json) - 1) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  int written = vsnprintf(json + jsonLen, sizeof(json) - jsonLen, fmt, args);
  va_end(args);

  if (written < 0) {
    return;
  }
  jsonLen += (size_t)written;
  if (jsonLen > sizeof(json) - 1) {
    jsonLen = sizeof(json) - 1;   // hubo truncamiento
  }
}

static void handleLog() {
  // Las lineas ya vienen sanitizadas desde Log.cpp, asi que entran al JSON sin
  // escapar nada.
  uint8_t count = logCount();

  jsonLen = 0;
  jsonAppend("{\"n\":%lu,\"lines\":[", (unsigned long)logTotal());

  for (uint8_t i = 0; i < count; i++) {
    jsonAppend("%s\"%s\"", (i == 0) ? "" : ",", logLine(i));
  }

  jsonAppend("]}");
  server.send(200, F("application/json"), json);
}

static void handleNotFound() {
  server.send(404, F("text/plain"), F("no existe"));
}

void webUiInit() {
  strncpy(resetReason, ESP.getResetReason().c_str(), sizeof(resetReason) - 1);
  resetReason[sizeof(resetReason) - 1] = '\0';

  server.on("/",              HTTP_GET,  handleRoot);
  server.on("/s",             HTTP_GET,  sendStatus);
  server.on("/l",             HTTP_GET,  handleLog);
  server.on("/i.png",         HTTP_GET,  handleIcon);
  server.on("/m.webmanifest", HTTP_GET,  handleManifest);
  server.on("/t",             HTTP_POST, handleToggle);
  server.on("/c",             HTTP_POST, handleConfig);
  server.on("/h",             HTTP_POST, handleSetTime);
  server.onNotFound(handleNotFound);

  /*
    Conexiones persistentes. El core las trae DESACTIVADAS
    (_keepAlive = false), asi que sin esto cada pedido abre y cierra su propia
    conexion TCP. La pagina consulta /s cada 2 segundos: son ~1800 conexiones
    por hora, y cada una deja un socket en TIME_WAIT un rato despues de
    cerrarse.

    La lwip de la ESP8266 tiene pocos PCB disponibles, asi que se agotan y las
    conexiones nuevas se rechazan hasta que se liberan. Con keep-alive todo el
    sondeo reutiliza una sola conexion. (Esto es distinto del problema de
    resolucion de nombres: aca la conexion se rechaza, alla nunca se intenta.)
  */
  server.keepAlive(true);

  server.begin();
  logPrintf("Web: servidor en el puerto %u", WEB_PORT);
}

void webUiLoop() {
  server.handleClient();
}
