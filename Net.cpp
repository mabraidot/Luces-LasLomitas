/*
  Net.cpp - Implementacion de la conexion WiFi.

  Por ahora solo DHCP. La IP fija con caida a DHCP entra cuando esten los
  datos de la red de instalacion (subred y gateway del Starlink).
*/

#include "Net.h"
#include "Config.h"
#include "Secrets.h"
#include "Log.h"

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266NetBIOS.h>

enum NetState {
  NET_CONNECTING,   // begin() ya llamado, esperando asociacion e IP
  NET_WAITING,      // radio en reposo, contando el backoff
  NET_CONNECTED
};

static NetState state       = NET_CONNECTING;
static uint32_t stateAt     = 0;
static uint32_t waitMs      = NET_BACKOFF_1_MS;
static uint8_t  failures    = 0;
static uint32_t heartbeatAt = 0;
static bool     mdnsUp      = false;
static bool     nbnsUp      = false;

static void setState(NetState next) {
  state   = next;
  stateAt = millis();
}

static void startConnecting() {
  /*
    WiFi.persistent(false) es obligatorio y no es un detalle de estilo.

    Por defecto el SDK de la ESP8266 escribe las credenciales en flash en
    CADA WiFi.begin(). Con una maquina de reintentos que llama a begin() cada
    60 s, eso son ~1400 escrituras por dia sobre el mismo sector. La flash
    tiene ciclos finitos: es desgaste real que termina en corrupcion a los
    meses, y no se parece a un problema de red cuando aparece.
  */
  WiFi.persistent(false);

  // Explicito: si no, la placa puede levantar tambien un access point propio,
  // gastando RAM y energia y dejando una red abierta dando vueltas.
  WiFi.mode(WIFI_STA);

  /*
    Radio siempre despierta.

    Por defecto la ESP8266 usa modem sleep: apaga el receptor entre beacons y
    lo despierta cada tanto. Ahorra unos 15 mA y rompe dos cosas justo para
    este uso:

      - Se pierden paquetes MULTICAST, y las consultas mDNS son multicast. Es
        la razon por la que "luces.local" resolvia a veces y a veces no.
      - Se pierden SYN entrantes, asi que el primer pedido despues de un rato
        de inactividad falla y hay que refrescar.

    El aparato esta enchufado a la red electrica: los 15 mA no importan.
  */
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  // Viaja en el pedido DHCP: la placa aparece identificable en la lista de
  // dispositivos del router, que es una de las formas de encontrar su IP.
  WiFi.hostname(NET_HOSTNAME);

  // Ayuda, pero no reemplaza a esta maquina de estados: no cubre todos los
  // modos de falla.
  WiFi.setAutoReconnect(true);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  setState(NET_CONNECTING);
}

/*
  La pila WiFi de la ESP8266 se queda trabada en algunos estados y no sale
  sola. Apagar la radio y volver a prenderla la destraba. Es la diferencia
  entre reconectar solo y quedarse sin web hasta que alguien vaya a cortar la
  alimentacion.
*/
static void resetRadio() {
  logPrintf("WiFi: reiniciando la radio tras %u fallos", (unsigned)failures);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(50);
  WiFi.mode(WIFI_STA);
}

static void announceConnected() {
  IPAddress ip = WiFi.localIP();

  // Se imprime en CADA reconexion, no solo la primera: con DHCP la IP puede
  // cambiar, y este cartel es la unica forma de enterarse desde el monitor.
  logPrintf("WiFi conectado a %s", WIFI_SSID);
  logPrintf("  IP:      %s", ip.toString().c_str());
  logPrintf("  Gateway: %s", WiFi.gatewayIP().toString().c_str());
  logPrintf("  Mascara: %s", WiFi.subnetMask().toString().c_str());
  logPrintf("  DNS:     %s", WiFi.dnsIP().toString().c_str());
  logPrintf("  Senal:   %ld dBm", (long)WiFi.RSSI());
  logPrintf("  Web:     http://%s/", ip.toString().c_str());
  logPrintf("  o bien:  http://%s.local/", NET_HOSTNAME);

  // mDNS se liga a la IP, asi que hay que rearrancarlo en cada reconexion y
  // no una sola vez en setup().
  if (mdnsUp) {
    MDNS.end();
    mdnsUp = false;
  }
  if (MDNS.begin(NET_HOSTNAME)) {
    MDNS.addService("http", "tcp", WEB_PORT);

    /*
      Anuncia el servicio "arduino", que es lo que hace aparecer la placa en
      Herramientas > Puerto > Puertos de red del IDE. Va aca y no en Ota
      porque mDNS es de este modulo: en cada reconexion se rehace el responder
      y hay que volver a publicar los dos servicios.
    */
    MDNS.enableArduino(OTA_PORT, OTA_HAS_PASSWORD);
    mdnsUp = true;
  }

  /*
    NetBIOS, para que "http://luces/" ande desde Windows.

    Windows no resuelve nombres .local de forma confiable sin Bonjour
    instalado: Chrome devuelve DNS_PROBE_POSSIBLE porque el resolver del
    sistema no contesta. Lo que Windows si entiende de fabrica es NetBIOS, y
    responder a eso cuesta un socket UDP.

    Queda entonces un nombre por plataforma, y la IP como respaldo universal:
      luces.local  -> iPhone, iPad, macOS, Android 12+
      luces        -> Windows
    Solo hace falta una vez: al responder usa la IP que la interfaz tenga en
    ese momento, asi que sobrevive los cambios de IP sin reiniciarlo.
  */
  if (!nbnsUp) {
    nbnsUp = NBNS.begin(NET_HOSTNAME);
  }
  if (nbnsUp) {
    logPrintf("  y desde Windows: http://%s/", NET_HOSTNAME);
  }
  logPrintf("  OTA: %s en Puertos de red del IDE", NET_HOSTNAME);
}

// Parpadeo lento buscando red, fijo al conectar. Sin necesidad de Serial,
// resuelve el "se conecto o no" parado abajo de la galeria.
static void updateLed() {
  bool on;

  if (state == NET_CONNECTED) {
    on = true;
  } else {
    on = ((millis() / NET_BLINK_MS) % 2) == 0;
  }

  digitalWrite(PIN_STATUS_LED, on ? LED_LEVEL_ON : LED_LEVEL_OFF);
}

void netInit() {
  digitalWrite(PIN_STATUS_LED, LED_LEVEL_OFF);
  pinMode(PIN_STATUS_LED, OUTPUT);

  failures    = 0;
  waitMs      = NET_BACKOFF_1_MS;
  heartbeatAt = millis();

  logPrintf("WiFi: conectando a %s", WIFI_SSID);
  startConnecting();
}

void netLoop() {
  uint32_t now = millis();
  bool connected = (WiFi.status() == WL_CONNECTED);

  switch (state) {

    case NET_CONNECTING:
      if (connected) {
        failures = 0;
        waitMs   = NET_BACKOFF_1_MS;
        setState(NET_CONNECTED);
        announceConnected();
        heartbeatAt = now;
        break;
      }
      if (now - stateAt >= NET_CONNECT_TIMEOUT_MS) {
        failures++;
        logPrintf("WiFi: sin conexion, reintento en %lu s", waitMs / 1000);
        WiFi.disconnect();
        setState(NET_WAITING);
      }
      break;

    case NET_WAITING:
      if (now - stateAt < waitMs) {
        break;
      }
      // Backoff escalonado: recupera rapido de un corte breve del router, que
      // es lo mas comun, sin machacar el aire durante una caida larga.
      if (waitMs == NET_BACKOFF_1_MS) {
        waitMs = NET_BACKOFF_2_MS;
      } else if (waitMs < NET_BACKOFF_MAX_MS) {
        waitMs = NET_BACKOFF_MAX_MS;
      }
      if (failures > 0 && (failures % NET_RADIO_RESET_TRIES) == 0) {
        resetRadio();
      }
      startConnecting();
      break;

    case NET_CONNECTED:
      if (!connected) {
        logPrintf("WiFi: se perdio la conexion");
        waitMs = NET_BACKOFF_1_MS;
        setState(NET_WAITING);
        break;
      }
      if (mdnsUp) {
        MDNS.update();
      }
      if (now - heartbeatAt >= NET_HEARTBEAT_MS) {
        heartbeatAt = now;
        logPrintf("WiFi ok, %ld dBm, heap %u",
                  (long)WiFi.RSSI(), (unsigned)ESP.getFreeHeap());
      }
      break;
  }

  updateLed();
}

bool netIsConnected() {
  return state == NET_CONNECTED && WiFi.status() == WL_CONNECTED;
}

IPAddress netIP() {
  if (!netIsConnected()) {
    return IPAddress(0, 0, 0, 0);
  }
  return WiFi.localIP();
}

int32_t netRssi() {
  if (!netIsConnected()) {
    return 0;
  }
  return WiFi.RSSI();
}

const char *netStateText() {
  switch (state) {
    case NET_CONNECTED:  return "conectado";
    case NET_CONNECTING: return "buscando";
    default:             return "esperando";
  }
}
