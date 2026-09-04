# Luces exteriores — Las Lomitas

Controlador automático de luces exteriores sobre **ESP8266 NodeMCU v1**.
Enciende al oscurecer, apaga a una hora configurable, y se maneja desde una
tecla de pared o desde una página web que sirve la propia placa en la red local.

En una línea: **al oscurecer se encienden; a la hora de dormir se apagan; una
acción manual manda hasta el próximo evento.**

---

## Conexionado

### Lado de red, 220 V

Cablear **sin tensión**. Ojo: la tecla de pared y su cable de retorno llevan
220 V.

De la red cuelgan **tres circuitos independientes**, cada uno entre fase y
neutro. Ninguno se mezcla con otro:

```
  1) ALIMENTACION PERMANENTE

     FASE (L) --------> [L]  FUENTE 1  [N] --------> NEUTRO (N)

                             [+V] = 5V
                             [-V] = GND


  2) SENSADO DE LA TECLA

                        +-----------+
     FASE (L) --------> |   TECLA   | --retorno--> [L]  FUENTE 2  [N] --> NEUTRO (N)
                        | de pared  |
                        +-----------+                   [+V] = 5Vb
                                                        [-V] = GNDb

     La FUENTE 2 solo tiene tension cuando la tecla esta CERRADA: es asi como
     la placa sabe en que posicion esta, sin tocar los 220 V.


  3) LAS LUCES

                        +----------------------+
     FASE (L) --------> | [COM]           [NO] | ----> LUCES ----> NEUTRO (N)
                        |         RELE         |
                        | [NC] sin conectar    |
                        +----------------------+

     COM trae la tension; NO la entrega cuando el rele cierra. Va en NO y
     nunca en NC: asi un controlador muerto, un reset o un corte de
     alimentacion dejan las luces APAGADAS.
```

Las tres conexiones a fase (F1, la tecla y COM del relé) y las tres a neutro
(F1, F2 y un polo de las luces) van al **mismo** par de conductores de la
instalación: los tres circuitos comparten los rieles L y N.

### Lado de baja tensión

```
                        +=====================+
      5V (F1, +V) ------| VIN                 |
      GND (F1, -V) -----| GND     NodeMCU v1  |
                        |                     |
                        | 3V3 |----+------------> DS1302  VCC2
                        |          +------------> LDR, extremo alto
                        |                     |
                        |  A0 |--- LDR ------> 3V3
                        |     |--- 10k ------> GND
                        |                     |
                        |  D1 |--------------> IN   del modulo de rele
                        |  D2 |--------------> divisor de la tecla
                        |  D4 |  LED interno, no se cablea
                        |  D5 |--------------> CLK  DS1302
                        |  D6 |--------------> DAT  DS1302
                        |  D7 |--------------> RST  DS1302
                        +=====================+
```

**Riel del relé, separado del de la placa:**

```
      5V (F1) ---> [IN]  AMS1117-3.3  [OUT] ---> VCC del modulo de rele
      GND -------> [GND]                          (y de ahi a la bobina)

      D1 (GPIO5) --------------------------> IN del modulo
      GND (directo al punto de masa de F1) -> GND del modulo
      Jumper de logica del modulo: INVERSA
```

**Divisor de la tecla, alimentado por la fuente 2:**

```
      5Vb (F2, +V) ---[ 10k ]---+--- D2 (GPIO4)
                                |
                              [ 15k ]
                                |
                               GND (masa comun)
```

10k arriba (al `+V` de F2) y 15k a GND, **en ese orden**: invertidos darían
2,0 V y el pin no leería HIGH nunca. El divisor va del lado del ESP, no del
lado de F2, así el tramo largo de cable lleva 5 V y el nodo de alta impedancia
queda corto.

### Tabla de pines

| Pin | GPIO | Uso | Notas |
|---|---|---|---|
| D1 | 5 | Relé | Lógica inversa: LOW = encendido |
| D2 | 4 | Tecla de pared | `INPUT`, vía divisor desde F2; cerrada = HIGH |
| D4 | 2 | LED de estado | `LED_BUILTIN`, lógica inversa |
| D5 | 14 | RTC DS1302 — SCLK | |
| D6 | 12 | RTC DS1302 — I/O | |
| D7 | 13 | RTC DS1302 — CE | |
| A0 | ADC | LDR | Divisor con 10k a GND |
| D0 | 16 | *libre* | No usar: ver abajo |
| D8 | 15 | *libre* | No usar: ver abajo |

---

## Lo que no se ve en el diagrama

Ocho decisiones que costaron encontrar y que el dibujo no muestra.

**El relé va en D1, no en D0.** GPIO16 vive en el dominio RTC y su estado
durante el reset no está garantizado, lo que con lógica inversa significa relé
activado. Y no es un pico: el bootloader corre **200 a 300 ms** antes de que se
ejecute nuestra primera línea, tiempo de sobra para que la bobina levante y las
luces prendan en cada arranque. El escenario grave no es el clic sino el bucle
de arranque: con una fuente al límite, la placa entra en reset repetido y el
relé castañetea, que es exactamente la condición que pega contactos.

**La tecla no va en D8.** GPIO15 debe estar en LOW al encender o la ESP8266 no
arranca. Con una llave de palanca trabada en cerrado durante un corte de luz,
el dispositivo no volvería hasta que alguien fuera a mover la tecla.

**El relé arranca apagado por hardware.** El pull-up interno del módulo mantiene
`IN` en alto durante todo el bootloader. En código, `digitalWrite(HIGH)` va
**antes** de `pinMode(OUTPUT)`, para que el pin salga a nivel seguro sin un
glitch por bajo.

**Las dos masas DC tienen que estar unidas.** Las fuentes chicas de 220 V son
aisladas, así que sus salidas flotan una respecto de la otra; sin unir los
negativos, el divisor de la tecla no tiene referencia y la lectura de D2 no
significa nada. Es el error más fácil de cometer y el más difícil de
diagnosticar, porque a veces "casi" funciona por acoplamiento capacitivo.

**El AMS1117 alimenta solo la bobina.** El RTC y el divisor del LDR van al 3V3
del NodeMCU. El ADC del ESP8266 no mide de forma ratiométrica, así que si el LDR
colgara del riel del relé, cada conmutación metería un escalón en la lectura de
luz — un error *correlacionado con el estado del relé*, justo lo que la lógica
de control evita por diseño.

**El relé de 3 V consume más que el de 5 V, no menos.** La potencia de bobina de
la familia JQC3F es constante en ~0,36 W, así que al bajar la tensión sube la
corriente: 132 mA a 3,3 V contra 71 mA a 5 V. Por eso hace falta el AMS1117
dedicado, y por eso la bobina no puede colgar del regulador del NodeMCU, que
alimenta al ESP8266 y sus picos de 300 mA.

**Los pines GND del NodeMCU son un solo nodo**, pero estar en la misma red no es
lo mismo que ser equivalente: la corriente de la bobina atraviesa el plano de la
placa. Los números dicen que no importa — 5 a 20 mΩ dan 0,7 a 2,6 mV con
132 mA, contra 3,22 mV que vale una cuenta del ADC. Lo ideal teórico sería masa
en estrella al borne `[-V]` de F1, pero no justifica rehacer el cableado.

**El DS1302 se alimenta con 3V3, no con 5 V.** D5/D6/D7 no toleran 5 V y las
lecturas del bus salen corruptas.

---

## La lógica de control

Es la parte central. El enfoque ingenuo es evaluar una condición continua:

```cpp
if (luz < umbral) encender(); else apagar();     // NO
```

Eso falla de dos maneras. **El modo manual se vuelve imposible**: apagás la luz
y la condición la vuelve a encender en el ciclo siguiente. Y **el sensor se
realimenta** con la propia lámpara: ilumina el LDR, la lectura sube, apaga, la
lectura baja, enciende. Oscila para siempre.

La solución es tratar las reglas automáticas como **transiciones**, no como
estados. El relé es una variable que solo cambia cuando ocurre un evento.

| Evento | Cuándo dispara | Acción |
|---|---|---|
| **Atardecer** | nivel bajo el umbral, sostenido 30 s, dentro de la franja de encendido, con el disparador armado | Encender |
| **Aclaró** | nivel sobre `umbral + histéresis`, sostenido 2 min, y estaba oscuro cuando se encendió | Apagar y rearmar |
| **Hora de dormir** | el reloj cruza la hora límite | Apagar |
| **Cierre de ventana** | el reloj cruza el fin de la ventana segura | Apagar, incondicional |
| **Manual** | toque en la web, o cualquier flanco de la tecla | Invertir |
| **Watchdog** | el relé lleva N horas encendido | Apagar |

### La franja de encendido no es la ventana segura

La **franja de encendido** es `[inicio de ventana, hora de dormir)` — 15:00 a
23:00 con los valores por defecto. Es el único tramo en que el atardecer puede
disparar. Entre la hora de dormir y el cierre de la ventana (23:00 a 04:00) no
puede: ya pasó la hora de dormir.

Del rango seguro, cada extremo hace algo distinto. **El inicio abre la franja**,
así que una tormenta al mediodía no enciende las luces. **El fin apaga
incondicionalmente**, y es puro seguro para los encendidos manuales que ningún
otro evento alcanzaría a apagar.

### El armado del disparador

Una sola variable, `duskArmed`, hace funcionar el resto:

- Se **desarma** cuando el atardecer dispara.
- Se **rearma** cuando el nivel sube sobre `umbral + histéresis` **y el relé
  está apagado**.

Esa condición `!relayIsOn()` no es un detalle. Bajo la galería la lámpara le
pega al sensor, así que con la luz encendida el nivel sube sobre
`umbral + histéresis` y el disparador se rearmaría. Entonces la hora de dormir
apagaría y, medio minuto después, el atardecer volvería a encender: luces
prendidas toda la noche, u oscilando. Como la lámpara solo puede alterar la
lectura cuando está encendida, ignorar el rearme mientras está prendida quita
por completo su influencia.

**Arranca en `true`**, y eso no es obvio: si arrancara desarmado, un corte de
luz a las 21:00 dejaría las luces apagadas toda la noche, porque el nivel ya
está bajo y el disparador solo se arma cuando la luz *sube*.

### "Aclaró": el evento peligroso

Existe por un caso concreto: una tormenta oscurece el cielo a las cuatro de la
tarde, las luces encienden bien, y si se disipa antes del atardecer quedarían
prendidas en pleno día hasta las 23:00.

Pero es el único evento que apaga por exceso de luz, o sea el que puede
realimentar. Tres cosas lo acotan:

**Solo actúa si estaba oscuro al encender.** El criterio no es *quién* encendió
sino *si había luz* en ese momento. Quien prende la luz con el cielo claro ya
sabía que estaba claro; que aclare no le aporta nada. Quien la prende a oscuras
la prendió **porque** no había luz, así que apagar cuando vuelve es coherente —
sea el atardecer o una persona a las cinco de la mañana.

**Se comprueba, no se predice.** El aparato controla la lámpara, así que hace un
experimento sobre sí mismo: cuando "aclaró" apaga, mira qué le pasa al nivel
tres segundos después.

| Al apagar | Qué significa |
|---|---|
| el nivel se queda alto | era luz de día; la decisión estuvo bien |
| el nivel **cae** bajo `umbral + histéresis` | lo tenía alto la propia lámpara: **queda probado** que la histéresis no alcanza |

En el segundo caso el evento se desactiva hasta que cambien los ajustes, y la
web lo informa. Esto no se puede engañar, porque no depende de interpretar una
medición ambigua: es una prueba controlada con una sola variable — la lámpara —
que el aparato mismo maneja. Cuesta un único ciclo de aprendizaje.

Una versión anterior intentaba **predecirlo**, midiendo cuánto sube el nivel al
encender. Esa medición es una heurística frágil: no distingue "la lámpara sumó
luz" de "cambió la luz ambiente", así que tapar y destapar el sensor con la mano
la contamina. Sigue existiendo, pero **solo como dato informativo** — sirve para
saber si conviene ponerle una visera al sensor.

**Confirmación de 2 minutos**, cuatro veces la del atardecer. Pero la protección
principal contra el ciclado no es ese tiempo sino la **histéresis**: para volver
a encender, el nivel tiene que caer desde `umbral + histéresis` hasta el
`umbral`, unos 10 puntos en escala logarítmica, o sea **un factor de 1,5 en el
crudo**. No alcanza un parpadeo.

### Arranque en frío

Al energizar, el controlador no sabe nada del pasado y tiene que deducir cómo
arrancar. No hace falta ninguna lógica especial: alcanza con relé apagado,
`duskArmed = true`, y no evaluar nada hasta que el sensor esté asentado.

| Hora del arranque | Nivel | Resultado a los ~5 s |
|---|---|---|
| 21:00 (en la franja) | bajo | **enciende** — recuperación tras un corte |
| 17:00 | alto | apagado, enciende al atardecer |
| 16:00 | bajo (tormenta) | **enciende** |
| 02:00 (pasó la hora límite) | bajo | apagado ✓ |
| 10:00 (antes de la franja) | alto | apagado, enciende al atardecer |

La fila de las 02:00 es la que justifica la definición de la franja: sin ella,
arrancar armado a las 2 de la mañana con el nivel bajo encendería las luces con
la hora de dormir ya pasada.

La confirmación del atardecer es de **3 s** durante los primeros 20 s de vida en
lugar de 30 s: en el arranque no seguimos una transición sino que leemos un
estado estable.

### Cuando cambia la referencia, no la hora

Los eventos de reloj se detectan por **flanco**, y eso deja un hueco: si lo que
cambia no es la hora sino la *referencia*, el cruce no ocurre nunca.

- Se mueve la hora de dormir a una **ya pasada**: el reloj no vuelve a cruzarla.
- **NTP corrige el RTC** saltando por encima de la hora de dormir.

En los dos casos las luces se quedarían encendidas hasta el cierre de la
ventana. Se ve como "el modo manual no se suelta nunca", pero no hay ningún modo
manual: es el evento que nunca llega a existir.

La solución es reaplicar la regla en forma de **estado** cuando cambia la
referencia, acotada de dos maneras: **solo puede apagar, nunca encender**, y
**solo corre cuando cambian los ajustes o salta el reloj**. El tramo que se
comprueba es `[hora de dormir, fin de ventana)`, así que un encendido manual de
las 10 de la mañana sobrevive a que se edite cualquier ajuste.

### Si el RTC no responde

Modo degradado: el atardecer dispara igual ignorando la franja, "aclaró" sigue
funcionando —no necesita reloj—, y el apagado queda a cargo del watchdog con
`maxOnMinutes` reducido a **6 h**. Se sostiene solo día tras día: apaga cerca de
la 1, el nivel sigue bajo así que no rearma, al amanecer rearma o apaga por
"aclaró", y al atardecer siguiente enciende.

Con el RTC muerto el dispositivo sigue haciendo su trabajo principal; solo
pierde precisión en el horario de apagado.

---

## El sensor de luz

LDR entre 3V3 y A0, con 10k entre A0 y GND. La lectura sube con la luz.

**Rango medido:** ~1023 a cielo abierto (saturado), ~200 en interior con día
nublado, ~10 a 20 de noche. Dos décadas.

Que sature en 1023 no es un problema: en el extremo brillante solo hace falta
saber "hay luz de día", no cuánta. Toda la información que importa vive en el
extremo oscuro, y bajar la resistencia para evitar el recorte **empeoraría** ese
extremo.

Tres capas de procesamiento:

**Sobremuestreo.** 16 lecturas de `analogRead()` cada 50 ms, acumuladas sin
dividir. Conserva 4 bits extra y promedia el ruido del ADC (±2 a ±5 cuentas,
peor con el WiFi transmitiendo).

**Media móvil exponencial** en `float`, con constante de tiempo de ~800 ms. En
`float` a propósito: la ESP8266 no tiene FPU, pero a 20 muestras por segundo el
costo es irrelevante y evita el error de truncamiento de la EMA entera, donde la
media nunca llega al valor final. **Se siembra con la primera lectura real**, no
en 0 — si arrancara en 0, el nivel reportado durante el primer segundo sería
"oscuridad total" mientras la media trepa, y el arranque en frío vería un falso
"está oscuro" justo cuando decide si encender.

**Mapeo logarítmico a 0–100.** Con dos décadas de rango, un mapeo lineal metería
todo el crepúsculo en los primeros 10 puntos. El logaritmo puro da una escala
perceptualmente uniforme, donde cada duplicación del crudo suma una cantidad
fija de puntos:

```cpp
nivel = 100 × (ln(raw) − ln(rawNoche)) / (ln(rawDia) − ln(rawNoche))
```

| crudo | lineal | **logarítmico** | qué es |
|---|---|---|---|
| 15 | 0 | **0** | noche cerrada |
| 30 | 1 | **16** | |
| 60 | 4 | **33** | crepúsculo |
| 200 | 18 | **61** | interior, día nublado |
| 1023 | 100 | **100** | exterior a pleno |

Efecto lateral útil: en escala logarítmica, una cantidad fija de puntos es un
**factor fijo** en el crudo, que es el comportamiento correcto para un sensor de
luz.

### Calibración

`rawNoche` y `rawDia` son configuración en EEPROM, ajustables desde la web con
un botón "actual". Dos trampas:

- **`rawDia` se mide bajo la galería**, no a cielo abierto. Bajo el techo la
  lectura es bastante menor, y calibrar con 1023 comprime todo el rango real en
  la mitad baja de la escala.
- **`rawNoche` se mide con la lámpara apagada.** Con las luces encendidas se
  está midiendo la propia lámpara.

---

## Arquitectura

Funciones libres con prefijo, estado `static` privado en cada `.cpp`, sin clases
ni objetos globales expuestos. El `.ino` no tiene lógica: solo llama a los
`*Init()` y los `*Loop()`.

| Archivo | Responsabilidad |
|---|---|
| `Config.h` | Pines y constantes, en un solo lugar |
| `Secrets.h` | Credenciales. **No se versiona** |
| `Log.*` | Buffer circular en RAM, espejado a Serial |
| `Relay.*` | D1 con la lógica inversa encapsulada |
| `Button.*` | D2, antirrebote, flancos en modo latching |
| `Settings.*` | EEPROM con magic + versión + CRC |
| `LightSensor.*` | A0: sobremuestreo, EMA, mapeo logarítmico |
| `Clock.*` | RTC DS1302, con caché y reintento no bloqueante |
| `LightController.*` | **El cerebro.** La máquina de eventos |
| `Net.*` | WiFi con backoff, mDNS, NetBIOS, LED de estado |
| `TimeSync.*` | NTP → RTC |
| `Ota.*` | Actualización de firmware por WiFi |
| `WebUi.*` + `Page.h` + `Icon.h` | Servidor, página y icono |

**`LightController` es el único dueño del relé.** Ni la web ni la tecla lo tocan
directo. Y no conoce `Net` ni `WebUi`, por lo que una falla de red no puede
afectar a las luces.

Dos llamadas cruzan entre módulos y merecen nombre propio:

- **`lightControllerManualToggle()`** — la invocan `WebUi` y el sketch.
- **`lightControllerClockChanged()`** — la invocan `TimeSync` y `WebUi` después
  de poner el reloj en hora. Resincroniza el rastreador de flancos, sin lo cual
  corregir la hora a las 22:50 dispararía "hora de dormir" al instante.

**Sin un solo `delay()` en `loop()`.** Cada módulo se autolimita con `millis()`.

| Tarea | Cada |
|---|---|
| `server.handleClient()`, OTA | cada vuelta |
| tecla | cada vuelta (un `digitalRead` cuesta µs) |
| sensor | 50 ms |
| máquina de eventos | 200 ms |
| lectura del RTC | 500 ms (el bus es bit-banging) |
| NTP | 6 h |

---

## Red

**La red es opcional.** El aparato tiene que encender y apagar las luces igual de
bien sin WiFi, sin internet y sin router. Nada bloquea el arranque, y **nunca se
reinicia la placa por problemas de red**: un reinicio hace clic en el relé y
pierde el estado.

**Backoff escalonado:** 15 s, 30 s, 60 s, y de ahí cada 60 s indefinidamente.
Reinicio de la radio cada 10 fallos, porque la pila WiFi de la ESP8266 se traba
en algunos estados y no sale sola.

Tres detalles de la ESP8266 que hay que resolver a mano:

**`WiFi.persistent(false)`** es obligatorio. Por defecto el SDK escribe las
credenciales en flash en **cada** `WiFi.begin()`. Con reintentos cada 60 s son
~1400 escrituras por día sobre el mismo sector: desgaste real que termina en
corrupción a los meses, y no se parece a un problema de red cuando aparece.

**`WiFi.setSleepMode(WIFI_NONE_SLEEP)`.** El modem sleep apaga el receptor entre
beacons, y eso descarta paquetes **multicast** — que es lo que usa mDNS. Es la
razón por la que `luces.local` resolvía a veces y a veces no. También se
pierden SYN entrantes, así que el primer pedido tras un rato de inactividad
falla. Cuesta ~15 mA, irrelevante enchufado a la red.

**`server.keepAlive(true)`.** El core lo trae desactivado, así que cada consulta
de `/s` abría y cerraba su propia conexión TCP: ~1800 por hora, cada una
dejando un socket en `TIME_WAIT`. La lwip de la ESP8266 tiene pocos PCB, se
agotan, y las conexiones nuevas se rechazan.

### Cómo llegar a la placa

| Desde | Usar |
|---|---|
| Windows | `http://luces/` — NetBIOS, que es lo que Windows entiende de fábrica |
| iPhone, iPad, macOS, Android 12+ | `http://luces.local/` — mDNS |
| Cualquier cosa | la IP, que siempre funciona |

Windows no resuelve `.local` sin Bonjour instalado: Chrome devuelve
`DNS_PROBE_POSSIBLE` porque el resolver del sistema no contesta. Por eso el
proyecto responde a los dos protocolos.

El cartel del arranque imprime las tres opciones por Serial, en **cada**
reconexión y no solo la primera, porque con DHCP la IP puede cambiar. Y el LED
integrado parpadea buscando red y queda fijo al conectar, que resuelve el "¿se
conectó o no?" parado abajo de la galería sin necesidad de Serial.

### Actualización por WiFi

La placa aparece en **Herramientas → Puerto → Puertos de red** del IDE como
`luces`, y se sube igual que por USB.

El anuncio por mDNS lo hace `Net`, no `Ota`: `ArduinoOTA.begin()` arranca su
propio responder por defecto, y dos responders en la misma placa se pelean el
puerto multicast. Así `Ota` llama a `begin(false)` y `Net` publica el servicio
junto al de HTTP, rehaciendo los dos en cada reconexión.

Dos efectos que conviene conocer. **Durante la subida `loop()` se detiene** — el
relé mantiene su estado, porque es un contacto mecánico, pero no se evalúa
ningún evento. Y **al terminar la placa se reinicia**, lo que pasa por
`relayInit()`: actualizar de noche apaga las luces y las vuelve a encender unos
cinco segundos después. Conviene actualizar de día.

Después de un OTA, el diagnóstico muestra `Software/System restart` como motivo
del último reinicio. Conviene saberlo para no confundirlo con un brownout de la
bobina, que es lo que ese campo está ahí para detectar.

---

## La web

`ESP8266WebServer` del core, síncrono. Cero dependencias extra, y para un
cliente que consulta cada 2 s va sobrado.

| Ruta | Qué hace |
|---|---|
| `GET /` | La página, desde PROGMEM |
| `GET /s` | Estado, ~250 bytes de JSON |
| `GET /l` | Últimas líneas del log |
| `GET /i.png`, `/m.webmanifest` | Icono y manifiesto de app |
| `POST /t` | Alternar |
| `POST /c?...` | Guardar configuración |
| `POST /h?...` | Poner el reloj en hora |

Todo se arma con `snprintf` sobre buffers `static`. **Nunca con `String`**: la
fragmentación del heap es la causa número uno de reinicios misteriosos en
proyectos ESP8266 con servidor web.

**La página no se cachea, a propósito.** El uso es esporádico —pueden pasar
semanas sin que nadie la abra mientras las luces funcionan solas—, así que
cualquier `max-age` vence antes del próximo uso y no ahorra nada. Lo único que
lograría es mostrar una versión vieja después de reprogramar. El icono y el
manifiesto sí se cachean: se piden en cada carga y pesan tres veces más.

Detalles de la página: foco SVG con glow por `filter: drop-shadow()`,
actualización **optimista** al tocarlo, sondeo con `setTimeout` encadenado y no
`setInterval` —con `setInterval`, si un pedido tarda, los siguientes se apilan y
saturan la ESP—, y pausa del sondeo con la pestaña oculta.

El bloque de **diagnóstico** existe porque en la instalación no hay monitor
serie: motivo del último reinicio, uptime, memoria libre, RSSI, aporte de la
lámpara y el log en RAM.

### Poner el reloj en hora

Se mandan los campos **ya en hora local**, no el epoch. `getTime()` devuelve
UTC, y convertirlo en el micro es arriesgar un error de tres horas.

---

## La hora

Un DS1302 sano deriva **segundos por día**. El de este proyecto atrasa cerca de
**un minuto por hora** — un error del 2%, unas 1400 veces lo esperable, que
apunta a cristal equivocado o módulo clon. Ningún ajuste de software lo arregla.

Como el WiFi ya está, NTP lo resuelve de raíz:

- **NTP** sincroniza al conectar y cada 6 h, y escribe al RTC solo si la
  diferencia llega a **60 s**. Los eventos tienen resolución de un minuto, así
  que un error menor no cambia ninguna decisión.
- **El RTC** cubre los cortes de luz y las caídas de WiFi. Es la fuente que lee
  el controlador **siempre**, así la lógica no necesita saber de dónde vino la
  hora ni si hay red.
- **El botón del navegador** queda como respaldo manual.

Se pide **UTC** y el desplazamiento de −3 h se aplica aritméticamente, con un
`daysFromCivil` propio para comparar instantes. El signo del huso en
`configTime()` de la ESP8266 es una fuente clásica de errores de tres horas;
así es imposible de confundir. Argentina es UTC−3 fijo, sin horario de verano.

**El guard que no puede faltar:** mientras NTP no sincronizó, `time(nullptr)`
devuelve un valor cercano a 0, o sea 1970. Escribir eso al RTC destruiría la
hora buena que ya tenía. No es un riesgo teórico: pasa en **cada arranque**
durante los primeros segundos, así que sin el guard falla siempre.

**Y el guard simétrico, el de frescura:** una vez que SNTP puso la hora, el
reloj de sistema de la ESP sigue corriendo solo aunque no vuelva a hablar con
ningún servidor, así que `time(nullptr)` queda plausible para siempre. Con el
router prendido y sin internet — un Starlink caído es exactamente eso — la placa
terminaría “corrigiendo” el DS1302, que tiene cristal, contra su propio oscilador
interno, y peor cuanto más durara el corte. Por eso `settimeofday_cb()` anota
cuándo sincronizó SNTP **de verdad**, y una hora de más de **3 h** no pisa el
RTC. La lwip del core reintenta cada hora, así que 3 h son tres intentos
fallidos. Saltear una corrección no cuesta nada: el RTC sigue llevando la hora y
se corrige en el próximo chequeo con internet.

---

## Ajustes

En EEPROM, con magic + versión + CRC. Si está virgen o corrupta, carga los
valores por defecto en lugar de arrancar con basura. **Solo escribe cuando algo
cambió de verdad** (`memcmp` antes del `commit`): la EEPROM emulada reescribe
todo un sector en cada commit.

| Ajuste | Default |
|---|---|
| Umbral de luz | 30 |
| Histéresis de rearme | 10 |
| Hora de dormir | 23:00 |
| Ventana segura | 15:00 – 04:00 |
| Máximo encendido | 12 h |
| `rawNoche` / `rawDia` | 15 / 1023 *(provisorio)* |

Todo campo se recorta al cargar y al guardar. La web valida del lado del
navegador, pero un POST se puede armar a mano: un umbral de 300 o una ventana de
largo cero dejarían al controlador tomando decisiones absurdas para siempre.

---

## Compilar y subir

Requiere el core **ESP8266 3.1.2** y la librería
[Alash_DS1302](https://github.com/Alash-electronics/Alash_DS1302).

1. Copiar `Secrets.h.template` a `Secrets.h` y completar SSID y contraseña.
2. Placa: **NodeMCU 1.0 (ESP-12E Module)**.
3. **CPU Frequency: 160 MHz.** Duplica el rendimiento del parsing HTTP y es
   gratis.
4. **Flash Size: 4MB (FS:none, OTA:~1019KB).** El proyecto no usa LittleFS — la
   página y el icono viven en PROGMEM —, así que reservar filesystem sólo recorta
   el espacio libre que la OTA necesita para escribir la imagen nueva mientras
   corre la vieja. El binario anda por los 372 KB: con `FS:none` el tope de OTA
   es ~1019 KB, con `FS:3MB` sería ~512 KB. Los ajustes guardados no dependen de
   esto: el sector de EEPROM está en `0x405fb000` en los tres layouts.
5. Subir por USB la primera vez; después por red. Por USB, **Erase Flash: Only
   Sketch**. Con *All Flash Contents* se borra la EEPROM, y con ella el umbral,
   las ventanas y la calibración del LDR.

```
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2:xtal=160,eesz=4M .
```

Para regenerar el icono después de cambiar el dibujo o los colores:

```
python tools-icon.py     # reescribe Icon.h
```

---

## Pendiente

- **Calibrar el LDR** en su posición definitiva: `rawDia` al mediodía con cielo
  despejado, `rawNoche` de noche con la lámpara apagada, y el crudo en el
  crepúsculo para elegir el umbral.
- **Mirar "Aporte de la lámpara"** la primera vez que encienda de noche ya
  montado. Ese número dice si conviene ponerle una visera al sensor.
- **IP fija con caída a DHCP**, cuando estén la subred y el gateway del
  Starlink. El router de Starlink no tiene reservas de DHCP, así que la IP fija
  va en la ESP; conviene una dirección alta (`.240` y por ahí). El respaldo a
  DHCP es lo que evita que una IP mal configurada deje el dispositivo
  inalcanzable.
- **Pulsador en lugar de la tecla de palanca.** Con una llave de palanca la
  posición no indica el estado de la luz, porque el reloj, el sensor y la web
  también lo cambian. Un pulsador momentáneo no tiene posición que pueda
  mentir.
- **Contraseña de OTA**, si se quiere: sin ella, cualquiera en la red local
  puede reprogramar una placa que maneja 220 V.
