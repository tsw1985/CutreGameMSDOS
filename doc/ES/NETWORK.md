# NETWORK.md — cómo funciona el juego en red

Documento de `src/net.c` y `header/net.h`, y de los cambios que la red trajo a
`src/main.c`. Explica qué hace cada función, por qué está tomada cada decisión,
y cómo montar las dos máquinas para probarlo.

> **Esto es la REFERENCIA, para consultar cosas sueltas.**
> Si lo que quieres es *entender* cómo funciona, empieza por
> [`MANUAL-RED.md`](MANUAL-RED.md), que es un manual de aprendizaje que va
> desde cero y en orden. Vuelve aquí después.

Se juega en red arrancando el juego así:

```
game.exe /net
```

Sin argumento, el juego sigue siendo exactamente el de siempre: dos jugadores
en el mismo teclado. **No se ha tocado nada de eso.**

---

## Indice

1. El problema de partida, y qué es IPX
2. La decisión de fondo: lockstep
3. El retardo de entrada
4. La redundancia
5. Las tres capas de `net.c`
6. Cómo se encuentran las dos máquinas
7. La detección de desincronización
8. Montar las dos máquinas en DOSBox
9. En hardware real
10. Qué cambió en `main.c`
11. Constantes que se pueden tocar
12. Diagnóstico: qué mirar en el log
13. **El flujo completo, de principio a fin**
14. **Referencia de todas las funciones**

---

## 1. El problema de partida

Turbo C++ 3.0 **no tiene nada de red**. Ni sockets, ni `<sys/socket.h>`, ni
resolución de nombres. Su librería es DOS puro: ficheros, `int86()`, puertos.
Y DOS tampoco trae una pila TCP/IP.

En DOS la red es **un driver que cargas antes del juego**, y hay tres formas
de hablar con él:

| Camino | Qué hace falta | Trabajo |
|---|---|---|
| **IPX** | Un driver IPX cargado | Ninguno, el driver lo hace todo |
| Packet driver + WATTCP | Packet driver + enlazar una pila TCP/IP | Mucho, y hay que recompilar la pila para el modelo HUGE |
| Packet driver a pelo | Escribir IP y UDP tú | Reinventar IPX con más trabajo |

Se ha elegido **IPX**, por cuatro razones:

1. **No hay librería que enlazar.** Esto importa más de lo que parece: el
   juego se compila en modelo HUGE (`-mh`), y las pilas TCP/IP precompiladas
   de la época vienen para LARGE. Con IPX no se enlaza nada, solo se hace una
   llamada lejana al driver.
2. **No hay que teclear ninguna IP.** IPX tiene broadcast, así que las dos
   copias se encuentran solas.
3. **DOSBox emula el IPX directamente**, así que se puede desarrollar y probar
   sin tarjetas de red ni drivers de DOS.
4. Es lo que usaba Doom.

**Nada de este código es específico de DOSBox.** DOSBox implementa las mismas
llamadas que un driver real, así que el mismo `game.exe`, sin recompilar y sin
un solo `#ifdef`, funciona en dos DOSBox hablando por UDP o en dos máquinas
reales con tarjeta e `IPXODI` cargado.

### Qué es IPX exactamente

**IPX** (*Internetwork Packet eXchange*) es el protocolo de red de Novell
NetWare, de mediados de los 80. Fue el protocolo dominante en las redes de
oficina de DOS durante una década, antes de que TCP/IP se lo comiera. Es lo que
usaban Doom, Duke Nukem 3D, Warcraft II, Command & Conquer...

Puesto al lado de lo que conoces hoy:

| | TCP/IP | IPX |
|---|---|---|
| Direcciones | IP (4 bytes) + puerto | Red (4 bytes) + **nodo** (6 bytes) + **socket** (2 bytes) |
| El nodo es | — | **la MAC de la tarjeta**, sin traducción de ningún tipo |
| Nombres | DNS | no hay, y no hacen falta |
| Equivalente a UDP | UDP | **IPX a secas**, que es lo que usamos |
| Equivalente a TCP | TCP | SPX (no lo usamos) |

Tres cosas que lo hacen la elección correcta aquí:

**1. La dirección es la MAC.** No hay que asignar IPs, ni configurar máscaras,
ni montar un DHCP. La tarjeta ya viene con su dirección de fábrica. Y hay
broadcast (`FF:FF:FF:FF:FF:FF`), que es lo que permite que las dos copias del
juego se encuentren solas sin que nadie teclee nada.

**2. No hay pila que enlazar.** Esto es lo decisivo en Turbo C++ 3.0. Con
TCP/IP tendrías que meter WATTCP en el binario, con su ARP, su IP, su UDP y su
gestión de buffers, y recompilarla para el modelo HUGE. Con IPX **el driver ya
está cargado en memoria** y tú solo le haces una llamada lejana.

**3. Es un datagrama y ya está.** IPX no reintenta, no ordena, no garantiza
entrega y no hace control de congestión. Suena a defecto y es justo lo que
quiere un juego: TCP, con sus retransmisiones y su `Nagle`, te metería tirones
de cientos de milisegundos por recuperar un paquete que al llegar ya no sirve
para nada. Lo que hacemos nosotros con los paquetes perdidos —ignorarlos,
porque el siguiente trae la información otra vez— es más barato y más rápido
que cualquier cosa que TCP pueda hacer por ti.

Lo que IPX **no** te da y hay que poner tú: fiabilidad (la resolvemos con
redundancia, sección 4), saber quién está al otro lado (el HELLO de la sección
6), y que las dos partidas cuadren (el lockstep, sección 2).

---

## 2. La decisión de fondo: lockstep

Esta es la decisión importante del módulo, y no tiene nada que ver con IPX.

**No se manda ninguna posición.** Ni la del tanque, ni la de la bala, ni el
marcador. Las **dos** máquinas ejecutan el juego **entero**, los dos tanques
incluidos, y lo único que viaja es **un byte de teclas pulsadas por jugador y
por frame**.

Esto funciona porque el juego cumple una condición que no es habitual:

> Es **determinista**. Todo son enteros, no hay un solo `float`, no hay
> `rand()`, y nada mira el reloj.

Si las dos máquinas arrancan en el mismo estado y reciben la misma secuencia
de teclas, calculan **exactamente los mismos píxeles** hasta el final de la
partida. No pueden divergir.

El bucle queda así:

```
frame N:  mandar mis teclas para el frame N + NET_INPUT_DELAY
          esperar las teclas de la otra máquina para el frame N
          simular el frame N con LAS DOS
```

Lo que se gana:

- **Tráfico ridículo.** 5 bits por jugador y frame. La cabecera IPX (30 bytes)
  pesa más que todo lo demás junto.
- **El código del juego no cambia.** Las colisiones, las balas, las
  explosiones, el dibujado y el reinicio de ronda siguen exactamente igual.
- **El sonido no necesita red.** Las dos máquinas calculan el mismo disparo en
  el mismo frame, así que cada una llama a `sound_play()` por su cuenta, a la
  vez. No se manda nada sobre sonido.

Lo que cuesta:

- **Si un paquete no llega, las dos máquinas se paran.** No es que una vaya
  peor: es que ambas esperan. Eso se compra con el retardo de entrada y la
  redundancia, que es de lo que va la sección siguiente.

### La alternativa que no se ha elegido

Lo otro sería que una máquina fuese servidor autoritativo y mandase
posiciones. Es peor aquí: hay que serializar el estado entero cada frame
(unos 20-30 bytes en vez de 1), el cliente juega con lag visible en su propio
tanque, y hay que escribir código de interpolación. Solo compensa cuando el
determinismo no se puede garantizar, y aquí sí se puede.

---

## 3. El retardo de entrada: la decisión que más se nota

Está en `NET_INPUT_DELAY`, y es **la** constante que hay que tocar si el juego
va a tirones.

Tus propias teclas **no se aplican al momento**. Se guardan en la posición
`frame_actual + NET_INPUT_DELAY` del buffer y se leen cuando el juego llega a
ese frame, que es exactamente lo mismo que tardan en llegar las del otro.

> **Este es EL error clásico de todo el que hace lockstep por primera vez.**
> La tentación es aplicar lo tuyo al instante, porque se siente mucho mejor, y
> lo del otro con retardo. Si haces eso, las dos máquinas están simulando
> partidas distintas y **te desincronizas en el primer segundo**.

Las dos entradas se aplican en el mismo frame en las dos máquinas. Siempre.

Con el bucle a ~70 Hz, un frame son 14,3 ms:

| `NET_INPUT_DELAY` | Margen de jitter | Para qué |
|---|---|---|
| 3 | ~43 ms | Cable, o dos DOSBox en el mismo PC |
| **5** (actual) | **~71 ms** | **Wifi normal** |
| 7 | ~100 ms | Wifi mala, o por internet |

Lo que cuesta en jugabilidad es poco: a `PIXEL_TO_MOVE 2`, cinco frames son el
equivalente a **10 píxeles** de retraso al arrancar, en una pantalla de 320
con tanques de 18. Esto no es un juego de lucha.

---

## 4. La redundancia: por qué sale gratis

`NET_REDUNDANCY` vale 8. Cada paquete lleva **los últimos 8 frames de teclas**,
no solo el actual.

La razón es puramente de tamaño: la cabecera IPX son 30 bytes. Mandar 1 byte
de datos útiles o mandar 8 cuesta **el mismo paquete**. Así que la redundancia
es gratis, y se es generoso.

A cambio, **no hace falta retransmisión, ni acuses de recibo, ni ordenación**.
Un paquete perdido o retrasado lo cubre el siguiente. Para que el juego se
pare de verdad tendrían que fallar **8 paquetes seguidos**, que en una wifi
normal casi no pasa.

Wifi, además, no pierde tanto como **retrasa**: reintenta a nivel de enlace,
así que el paquete llega, tarde. La redundancia cubre igual los dos casos,
porque el paquete siguiente trae el frame que faltaba.

---

## 5. Las tres capas de `net.c`

El fichero está partido en tres, de abajo a arriba. No sabe nada de tanques:
mueve un byte por frame y compara un número que le da otro.

### Capa 1 — hablar con el driver IPX

**`ipx_detect()`** (línea 266)
INT 2F es el "¿hay alguien ahí?" de DOS. `AX=7A00` pregunta por IPX: si está
cargado, AL vuelve con `0xFF` y **ES:DI** trae la dirección a la que llamar a
partir de entonces.

Es el camino documentado y el que usaba Doom. Funciona con `IPXODI` real, con
el cliente de Novell y con DOSBox, que responde a esta llamada igual que un
driver de verdad.

**`ipx_call()`** (línea 308)
La llamada al driver: `BX` = qué hacer, `ES:SI` = el ECB.

Tiene que ser ensamblador porque IPX quiere sus argumentos **en registros** y
se entra con una **llamada lejana** (`CALL FAR`), y ninguna de las dos cosas se
puede expresar en C.

La parte fea es llamar a una dirección que está en una variable: no existe una
instrucción "llama lejos a esta pareja de registros". Así que la dirección se
empuja a la pila y se llama desde ahí, que es lo que hace este baile:

```asm
    push  cx            ; segmento del driver
    push  dx            ; offset del driver
    mov   bp, sp
    call  dword ptr [bp]
    add   sp, 4
```

`BP` se restaura **antes** de volver a tocar nada relativo a `BP`, porque el
driver puede devolver `BP` apuntando a cualquier sitio. Y se salvan `DS` y `ES`
porque en modelo HUGE el compilador da por hecho que `DS` sigue apuntando a los
datos de este módulo al terminar el bloque, y el driver no promete eso.

> **Si TCC se queja de `call dword ptr [bp]`**, esa misma instrucción se puede
> escribir a mano como `db 0FFh, 05Eh, 000h`. Está anotado en el propio código.

**`ipx_socket_call()`** (línea 358)
Abrir y cerrar el socket no usan ECB: el número de socket va directo en `DX` y
el resultado vuelve en `AL`. Mismo truco de llamada lejana, solo que aquí el
resultado hay que recogerlo **después** de restaurar `BP`, o el
`mov result_code, ax` escribiría en mitad de la pila.

**`net_post_listen()`** (línea 430)
Le devuelve al driver uno de los buffers de recepción.

Un buffer **o es nuestro o es del driver, nunca las dos cosas**. Al entregarlo
pasa a ser del driver; vuelve a ser nuestro cuando `in_use` baja a 0.

Hay **cuatro** buffers en rotación (`NET_LISTEN_ECB_COUNT`) a propósito:
mientras estamos tratando un paquete, su buffer es nuestro, y un paquete que
llegase justo entonces se perdería si fuese el único.

> **`esr_address` va siempre a NULL.** IPX puede llamarnos a una rutina nuestra
> en cuanto llega un paquete, pero esa rutina se ejecutaría **en tiempo de
> interrupción**, en mitad de lo que estuviera haciendo el juego, con todos los
> problemas de reentrancia que eso trae. Mirar `in_use` una vez por frame es
> suficiente y no puede salir mal.

### Capa 2 — nuestro paquete encima de IPX

**`net_send_is_busy()`** (línea 460)
Hay que preguntarlo **antes** de construir el paquete, no solo antes de
enviarlo: mientras un envío está en vuelo el buffer es del driver, y montar la
carga siguiente encima reescribiría un paquete que ya va de camino.

**`net_transmit()`** (línea 483)
Rellena la cabecera IPX y el ECB, y suelta el paquete. **Nunca espera.** Si el
envío anterior no ha terminado, este paquete simplemente se tira: no se pierde
nada que importe, porque el siguiente lleva los últimos 8 frames igual.

La red de destino va a 0, que significa "esta misma". Cualquier otra cosa
necesitaría un router, y entre dos máquinas de la misma wifi no hay ninguno.

**`net_packet_is_valid()`** (línea 553)
Todo paquete nuestro empieza por `"CTRE"`. Lo que llegue al socket sin esa
marca es tráfico de otro y se tira. También se descarta el que traiga **nuestro
propio `instance_id`**: en Ethernet real una tarjeta no se oye a sí misma y eso
no pasa nunca, pero comprobarlo no cuesta nada.

**`net_handle_packet()`** (línea 654)
Reparte según el tipo: `HELLO`, `HELLO_ACK` o `INPUT`.

**`net_poll()`** (línea 728)
Recoge todo lo que el driver tenga y le devuelve los buffers. Es lo **único**
que mueve datos hacia dentro, así que **cualquier bucle que espere tiene que
llamarlo**, o no llega nada nunca.

### Capa 3 — lockstep

**`net_set_local_input()`** (línea 1046)
Todo el retardo de entrada cabe en una línea: nuestras teclas entran en
`simulation_frame + NET_INPUT_DELAY`.

**`net_send_input()`** (línea 1063)
Manda los últimos `NET_REDUNDANCY` frames, y el checksum cuando toca.

**`net_has_remote_input()`** (línea 1129)
¿Han llegado sus teclas para el frame que vamos a simular? Mientras esto sea 0,
**el juego no puede avanzar**.

Cada hueco del anillo recuerda **a qué frame pertenece**, no solo si está
lleno. Sin eso, una entrada vieja de una vuelta anterior del buffer se
confundiría con la que estamos esperando.

**`net_set_local_checksum()`** / **`net_advance_frame()`** (líneas 1175 y 1199)
Cierran el frame.

---

## 6. Cómo se encuentran las dos máquinas

`net_find_opponent()` (línea 878) se ejecuta **en modo texto, antes de cambiar
a VGA**. A propósito: en modo gráfico no hay dónde imprimir, y esta es
justamente la parte que necesita poder decir qué está pasando.

Las dos copias hacen exactamente lo mismo:

1. Gritan `HELLO` a todo el mundo (`FF:FF:FF:FF:FF:FF`) cada cuarto de segundo.
2. Contestan con `HELLO_ACK` a cualquier `HELLO` que oigan.
3. Se dan por emparejadas cuando **han oído un HELLO Y han recibido respuesta
   al suyo**.

**Nadie teclea una dirección en ningún sitio.**

### Por qué el apretón de manos es de dos sentidos

Emparejar con el primer `HELLO` a secas no vale: la máquina rápida se largaría
a empezar la partida mientras la lenta sigue esperando una respuesta que ya no
va a llegar, porque la otra ha dejado de mandar `HELLO`. Exigir las dos cosas
garantiza que cuando una empieza, **la otra ya sabe que la partida está en
marcha**.

### Quién es el jugador 1

Se resuelve **sin negociar nada**. Cada copia se saca un número al azar al
arrancar (`instance_id`, a partir del tick de la BIOS), los dos viajan en el
`HELLO`, y **el menor es el jugador 1**, el tanque de abajo. Las dos máquinas
comparan los mismos dos números y llegan a la misma conclusión por su cuenta.

No se usa la dirección de nodo para esto a propósito: si el driver contestara
mal a la llamada "dame mi dirección" (función 9), las dos máquinas se creerían
el jugador 1 y la partida sería un disparate. El `instance_id` no depende de
ninguna llamada al driver.

### Por qué no hay "pulsa una tecla para empezar"

Había una, y **era un error**. Si un jugador tarda en pulsar, su máquina no
atiende la red, sus buffers de recepción se llenan y la otra se queda esperando
el frame 0 hasta agotar el tiempo de espera.

En su lugar hay una pausa **acotada de dos segundos** que **sigue llamando a
`net_poll()`**. Las dos máquinas empiezan en el frame 0; la que llegue primero
espera a la otra, y el lockstep se encarga de eso solo mientras la espera sea
corta.

---

## 7. La detección de desincronización

Esto es lo que más tiempo ahorra cuando algo va mal, y por eso está desde el
principio y no como un añadido.

> En lockstep, cuando las dos máquinas dejan de coincidir, **no se nota nada**.
> Cada pantalla sigue mostrando una partida perfectamente coherente. Distinta,
> pero coherente. Puedes perseguir eso durante días.

Así que cada 30 frames (`NET_CHECKSUM_INTERVAL`) cada máquina manda un número
que resume **todo** su estado, y la otra lo compara con el suyo del mismo
frame. Si no coinciden, sale en `tanks.log`:

```
NET DESYNC at frame 1830: mine 41234 theirs 41199
```

El checksum lo calcula `compute_state_checksum()` en `main.c`, porque `net.c`
no sabe qué es un tanque. Entra todo lo que la simulación puede cambiar:
posiciones, direcciones, balas, marcador, explosiones y el contador de pausa.
Los multiplicadores están para que intercambiar dos valores (por ejemplo las X
de los dos tanques) siga dando un total distinto.

Se guardan los checksums de los últimos 64 frames, así que uno que llegue
tarde todavía encuentra el frame al que pertenece.

---

## 8. Montar las dos máquinas en DOSBox

**DOSBox no emula una tarjeta de red.** Emula **el IPX directamente**: la INT
2F, la llamada lejana, los ECB. Eso quiere decir que **no hay driver que
cargar**: ni `LSL.COM`, ni ODI, ni `IPXODI.COM`, ni `NET.CFG`, ni frame types.

### La forma fácil: los scripts

Para dos máquinas, lo práctico es no escribir ningún `.conf`:

```
./launch_game_server.sh                        <- arranca esta PRIMERO
./launch_game_client.sh <ip-del-servidor>      <- y luego esta
```

Estos scripts **averiguan solos dónde está el juego** a partir de dónde están
ellos mismos y generan el `.conf` en cada arranque, así que el proyecto puede
vivir en carpetas distintas en cada máquina. Las rutas absolutas escritas a
mano dentro de un `.conf` son la causa número uno de que esto falle al pasarlo
a la segunda máquina.

Antes de lanzar nada comprueban que DOSBox está instalado, que existen `bin/` y
`res/` (y te dicen **la fecha del ejecutable**, para que veas si estás corriendo
un binario viejo), que el puerto no es privilegiado ni está ocupado, y el
cliente hace ping al servidor. El servidor te imprime la línea exacta que hay
que ejecutar en la otra máquina, con la IP ya puesta.

Y "servidor" no significa que no juegue: las dos máquinas juegan igual. Lo
único que hace es levantar el túnel para que la otra pueda engancharse. Tampoco
tiene nada que ver con quién acaba siendo el jugador 1, que lo decide el
`instance_id`.

### A mano, si prefieres

En las dos máquinas, en `dosbox.conf`:

```ini
[ipx]
ipx=true
```

Máquina A, la que hace de concentrador del túnel (también juega):

```ini
[autoexec]
ipxnet startserver 5213
mount c d:\msdos
c:
cd \game\bin
game.exe /net
```

Máquina B:

```ini
[autoexec]
ipxnet connect 192.168.1.10 5213
mount c d:\msdos
c:
cd \game\bin
game.exe /net
```

Doble clic en cada una y el juego arranca ya conectado. La IP aparece **una
sola vez**, en un fichero de configuración, nunca en el código ni en una
pantalla del juego.

### El puerto: en Linux NO puede ser el 213

DOSBox tunela el IPX sobre UDP y por defecto usa el **puerto 213**, que es el
que IANA asigna a IPX. En **Linux eso no vale**: los puertos por debajo de 1024
son privilegiados y un usuario normal no puede abrirlos. `ipxnet startserver`
falla, y el unico sintoma que ves es que el otro lado dice
`Timeout connecting to server`.

Por eso se usa un puerto alto, el **5213**, en las dos maquinas:

```
ipxnet startserver 5213
ipxnet connect <ip> 5213
```

En Windows el 213 si funciona, pero mas vale usar el mismo puerto en todas
partes. Y si hay cortafuegos por medio, lo que hay que abrir es **UDP 5213**.

### Comprobaciones antes de culpar al código

```
ipxnet ping      (dentro de DOSBox: ¿contesta la otra máquina?)
ipxnet status
```

Y desde el sistema operativo, antes de nada:

```
ping -i 0.2 <ip de la otra máquina>
```

**Mira el máximo, no la media.** La media siempre sale preciosa.

### Los dos tropiezos de wifi

1. **Aislamiento de clientes (AP isolation).** Muchos routers lo traen puesto,
   y **siempre** en las redes de invitados: impide que dos dispositivos de la
   misma wifi se hablen. Internet funciona y `ipxnet connect` se queda colgado
   sin decir por qué. Si el `ping` entre las dos máquinas no va, es esto.
2. **Cortafuegos, UDP 5213.** La máquina que hace `startserver` tiene que
   aceptar entrante. Es el puerto que usamos, no el 213 de por defecto.

Si una de las dos puede ir por cable, ponla por cable: cada salto wifi añade
su propio jitter.

### Poner los mismos `cycles=`

El lockstep sincroniza **por frame, no por tiempo**, así que no os vais a
desincronizar por ir a velocidades distintas. Pero si una DOSBox va al doble
que la otra, se pasa media vida esperando y las dos acaban yendo a la
velocidad de la lenta.

---

## 9. En hardware real

El mismo `game.exe`, sin recompilar. Lo único que cambia es que ahí sí hay que
cargar el driver antes, en el `autoexec.bat`, y en este orden exacto:

```
LSL.COM          <- siempre primero
RTSODI.COM       <- el driver ODI de tu tarjeta
IPXODI.COM       <- e IPX encima
```

Y las dos máquinas tienen que usar **el mismo frame type** en `NET.CFG`
(`ETHERNET_802.3`, `ETHERNET_II`...). Si una está en uno y la otra en otro,
las dos cargan sin errores, las dos parecen funcionar, y no se oyen jamás. Es
el fallo número uno.

> El túnel IPX de DOSBox es un mundo cerrado entre DOSBox: **no habla con el
> IPX real de una LAN**. No se puede conectar un DOSBox con una máquina DOS de
> verdad.

Para comprobar el hardware sin depender de este código, la prueba honesta es
**arrancar Doom en `-net 2`** entre las dos máquinas. Usa el driver igual que
lo usa este juego. Si Doom se ve, este juego se ve.

---

## 10. Qué cambió en `main.c`

Muy poco, y esa es la buena noticia. **No se ha tocado nada** de las
colisiones, `update_bullet()`, `draw_to_buffer()`, `player_update_explosion()`,
`sound_update()` ni `restart_game()`.

El cambio de fondo es uno solo:

> `process_player_input()` ya no lee `keys[]`. Recibe **un byte con 5 bits** ya
> resueltos.

Así no puede saber si esas teclas vienen de este teclado o de la otra máquina,
y no necesita saberlo. Eso es lo que permitió meter la red sin tocar el juego.

| Añadido | Qué hace |
|---|---|
| `read_input_from_keys()` | Convierte este teclado en el byte de 5 bits |
| `compute_state_checksum()` | El número que resume el estado, para detectar desincronización |
| `set_text_mode()` | Vuelve a texto al salir de una partida en red, para poder leer el mensaje |
| Bloque en el bucle | Manda, espera y reparte las teclas de los dos tanques |

Un detalle que **no es opcional**: la entrada se intercambia en **todos** los
frames, incluida la pausa de la explosión. Las dos máquinas tienen que ir
pasando por los mismos números de frame aunque en pantalla no se mueva nada, o
una se quedaría esperando una entrada que la otra nunca mandó.

Y el bucle de espera llama a `sound_update()` dentro. La espera suele ser una
fracción de frame, pero un mal momento de la wifi haría que la tarjeta repitiera
la misma media buffer y el sonido chirriara.

En red **las dos máquinas se manejan con las flechas y el 5 del teclado
numérico**, dé igual qué tanque te haya tocado.

---

## 11. Constantes que se pueden tocar

Todas en `header/net.h`.

| Constante | Valor | Qué pasa si la subes / bajas |
|---|---|---|
| `NET_INPUT_DELAY` | 5 | **La importante.** Más = aguanta más jitter, responde más tarde |
| `NET_REDUNDANCY` | 8 | Más = aguanta más pérdidas seguidas. Casi no cuesta ancho de banda |
| `NET_INPUT_BUFFER_SIZE` | 64 | Frames guardados. **Tiene que ser potencia de dos** |
| `NET_CHECKSUM_INTERVAL` | 30 | Cada cuánto se comprueba la desincronización |
| `NET_TIMEOUT_SECONDS` | 10 | Silencio antes de dar por muerta a la otra máquina |
| `NET_DISCOVERY_SECONDS` | 30 | Cuánto busca antes de rendirse |
| `NET_SOCKET_NUMBER` | `0x869C` | En `net.c`. Tiene que ser **el mismo en las dos** |

---

## 12. Diagnóstico: qué mirar en `tanks.log`

| Línea | Qué significa |
|---|---|
| `NET sizes: ecb=42 header=30 packet=60` | Correcto. **Cualquier otro número** significa que el compilador ha rellenado las estructuras y IPX leerá todos los campos del sitio equivocado |
| `NET: no IPX driver found` | No hay driver. En DOSBox falta `ipx=true`; en DOS real falta cargarlo |
| `NET: IPX driver entry at XXXX:YYYY` | El driver está y sabemos dónde llamarle |
| `NET: could not open the socket` | Otra copia del juego lo tiene abierto, o no se cerró bien la vez anterior |
| `NET: node ... id ...` | Nuestra dirección y nuestro número al azar |
| `NET: paired. them id ... we are player N` | Emparejados, y qué tanque nos ha tocado |
| `NET: discovery timed out` | La otra máquina no contestó. Mira el `ping` y el aislamiento de clientes |
| `NET DESYNC at frame N` | Las dos máquinas han calculado estados distintos |
| `NET: connection lost at frame N` | 10 segundos sin recibir nada |
| `NET: sent X, received Y, waited Z frames` | Resumen al salir |

Ese último es el más útil de todos. **`waited`** cuenta cuántos frames se pasó
el juego parado esperando a la otra máquina. Tiene **dos causas distintas**, y
distinguirlas importa porque la solución no es la misma:

- **Cerca de 0** → el enlace va sobrado.
- **Unas decenas, a rachas, en una partida larga** → jitter de la red. Para eso
  está `NET_INPUT_DELAY`. Súbelo, o pon una máquina por cable.
- **Una fracción grande y sostenida de los frames** (por ejemplo la mitad) →
  las dos máquinas **no van a la misma velocidad**, y la rápida se está
  frenando al ritmo de la lenta. Esto no es jitter, y **subir
  `NET_INPUT_DELAY` no lo arregla**: solo mete latencia. Un buffer fijo absorbe
  variación, no una diferencia permanente de velocidad. Baja `cycles` en **las
  dos** máquinas a un valor que la lenta pueda sostener.

Para saber cuál es la lenta, compara el `waited` de los dos logs: la lenta lo
tendrá cerca de cero, porque nunca tiene que esperar a nadie.

---

## 13. El flujo completo, de principio a fin

### Arranque

```
game.exe /net
   |
   +-- main()                        main.c:166
   |     lee los argumentos, network_mode = 1
   |
   +-- net_init()                    net.c:775
   |     |
   |     +-- escribe los tamanos de las estructuras en el log
   |     |   (42/30/60: si sale otra cosa, el compilador ha rellenado
   |     |    las estructuras e IPX leeria todos los campos mal)
   |     |
   |     +-- ipx_detect()            net.c:266
   |     |     INT 2F con AX=7A00. Si AL vuelve 0xFF, IPX esta cargado
   |     |     y ES:DI es la puerta de entrada. Se guarda.
   |     |
   |     +-- ipx_socket_call(OPEN)   net.c:358
   |     |     abre el socket 0x869C. A partir de aqui el driver nos
   |     |     entregara lo que llegue a ese socket y nada mas.
   |     |
   |     +-- ipx_get_local_address() net.c:411
   |     |     nuestra direccion de nodo. Solo para el log: si sale
   |     |     todo ceros, esta maquina NO esta unida a ningun tunel.
   |     |
   |     +-- instance_id = numero al azar del tick de la BIOS
   |     |
   |     +-- net_post_listen() x4    net.c:430
   |           entrega 4 buffers al driver. A partir de ahora, todo lo
   |           que llegue cae en uno de ellos. Cuatro y no uno porque
   |           mientras tratamos un paquete su buffer es nuestro.
   |
   +-- net_find_opponent()           net.c:878     <-- modo TEXTO
   |     bucle hasta emparejar (abajo)
   |
   +-- pausa de 2 segundos llamando a net_poll()
   |
   +-- install_kbd() / setup_screen() / init_players() / init_graphics()
   +-- sound_init()
   |
   +-- bucle principal
```

### El emparejamiento

Las dos copias hacen literalmente lo mismo. No hay un "servidor" y un
"cliente" a nivel de juego: eso es solo del tunel de DOSBox.

```
      MAQUINA A                                MAQUINA B

  HELLO --> broadcast  ------------------------->  net_poll()
                                                   net_handle_packet()
                                                   guarda su nodo e id
                                                   marca bit 0x01
                                    <-- HELLO_ACK  responde directo
  net_poll()
  marca bit 0x02
                                                   HELLO --> broadcast
  net_handle_packet()  <---------------------------
  guarda su nodo e id
  marca bit 0x01
  HELLO_ACK -->  -------------------------------->  net_poll()
                                                    marca bit 0x02

  bits == 0x03  -> EMPAREJADA           bits == 0x03  -> EMPAREJADA
```

Hacen falta **los dos bits**: haber oido a la otra (`0x01`) y que la otra haya
contestado a lo nuestro (`0x02`). Con uno solo, la maquina rapida se largaria a
jugar mientras la lenta sigue esperando una respuesta que ya nadie va a mandar.

Al salir del bucle, las dos hacen lo mismo por su cuenta y llegan a la misma
conclusion:

```c
if (local_instance_id < remote_instance_id){ is_player1 = 1; }
```

Y ponen `simulation_frame = 0`, limpian los anillos y rellenan los primeros
`NET_INPUT_DELAY` frames con "ninguna tecla", porque nadie los ha mandado nunca.

### Cada frame del bucle principal

Esto es el corazon. `main.c:276` en adelante.

```
 1. read_input_from_keys()      main.c:563
       flechas + 5 del numerico  ->  un byte con 5 bits

 2. net_set_local_input(byte)   net.c:1046
       lo guarda en el frame  N + NET_INPUT_DELAY   <-- EL RETARDO
                                                        ESTA AQUI

 3. net_send_input()            net.c:1063
       un paquete con los frames  N-2 .. N+5  (8 frames, la redundancia)
       + el checksum del estado si toca mandarlo

 4. while (net_has_remote_input() == 0){        <-- LA ESPERA
        net_poll();          recoge lo que haya llegado
        sound_update();      el sonido NO se para mientras esperamos
        net_connection_lost() ?  -> salir
    }

 5. reparto segun quien somos:
       si soy jugador 1:  p1 = mi byte del frame N,  p2 = el suyo
       si no:             p1 = el suyo,              p2 = mi byte

 6. process_player_input(&player1, &player2, p1)    main.c:915
    process_player_input(&player2, &player1, p2)
       <-- desde aqui hacia abajo, NADA sabe que existe una red

 7. update_bullet() x2, explosiones, sonido, marcador
 8. update_game() / draw_to_buffer() / wait_retrace() / volcado a VGA

 9. net_set_local_checksum(compute_state_checksum())   main.c:611
       resume TODO el estado en un numero y lo guarda

10. net_advance_frame()         net.c:1199
       N = N + 1
```

Los pasos 6, 7 y 8 son **exactamente el codigo que ya existia**. No se ha
tocado ni una linea de colisiones, balas, explosiones ni dibujado.

### Lo que viaja por el cable

Un paquete son **60 bytes**, de los cuales 30 son la cabecera de IPX:

```
+--------------------------------+  30 bytes  cabecera IPX
| red / nodo / socket destino    |            (destino lo ponemos nosotros,
| red / nodo / socket origen     |             origen lo rellena el driver)
+--------------------------------+
| "CTRE"                         |   4        marca, para ignorar lo ajeno
| tipo (HELLO/ACK/INPUT)         |   1
| cuantos inputs van             |   1
| instance_id                    |   4        quien lo manda / quien es P1
| primer frame de inputs[]       |   4
| inputs[8]                      |   8        <-- LOS DATOS DE VERDAD
| hay checksum?                  |   1
| relleno                        |   1
| frame del checksum             |   4
| checksum                       |   2
+--------------------------------+  30 bytes  nuestra carga
```

Mira la proporcion: de 60 bytes, **8 son el juego**. La cabecera pesa cuatro
veces mas que los datos. Por eso la redundancia sale gratis y por eso se mandan
8 frames en vez de 1.

### La salida

```
ESC
 |
 +-- sound_shutdown()    para el DMA ANTES de soltar la memoria
 +-- net_shutdown()      cierra el socket, escribe el resumen en el log
 +-- player_free() / bmp_delete_buffers() / bmp_close_files()
 +-- uninstall_kbd()     devuelve el INT 9 original
 +-- set_text_mode()     main.c:545, para poder leer el mensaje final
```

**Salir cerrando la ventana no ejecuta nada de esto.** En DOSBox da igual, pero
en DOS real el socket IPX se queda abierto y la siguiente partida no puede
abrir el mismo, y el DMA de la Sound Blaster sigue leyendo memoria que ya no es
tuya.

---

## 14. Referencia de todas las funciones

### `src/net.c` — capa 1, el driver IPX

| Función | Línea | Qué hace |
|---|---|---|
| `net_swap16()` | 242 | Da la vuelta a los 2 bytes de un entero. IPX escribe sockets y longitudes con el byte alto delante, y el 8086 los guarda al revés |
| `ipx_detect()` | 266 | `INT 2F` con `AX=7A00`. Si `AL` vuelve `0xFF`, IPX está cargado y `ES:DI` es la puerta de entrada |
| `ipx_call()` | 308 | **La** llamada al driver: `BX` = qué hacer, `ES:SI` = el ECB. En ensamblador, porque IPX quiere los argumentos en registros y se entra con `CALL FAR` |
| `ipx_socket_call()` | 358 | Abrir y cerrar el socket. No usan ECB: el número va en `DX` y el resultado vuelve en `AL` |
| `ipx_get_local_address()` | 411 | Nuestra dirección de nodo. Solo para el log |
| `net_post_listen()` | 430 | Entrega un buffer de recepción al driver |

### `src/net.c` — capa 2, nuestro paquete

| Función | Línea | Qué hace |
|---|---|---|
| `net_send_is_busy()` | 460 | ¿El driver sigue con el paquete anterior? Hay que preguntarlo **antes de construir**, no solo antes de enviar |
| `net_transmit()` | 483 | Rellena la cabecera IPX y el ECB y suelta el paquete. Nunca espera |
| `net_build_header()` | 535 | La parte de la carga que llevan todos los paquetes: marca, tipo, id |
| `net_packet_is_valid()` | 553 | ¿Empieza por `"CTRE"` y no es nuestro propio eco? |
| `net_handle_packet()` | 654 | Reparte según el tipo: HELLO, HELLO_ACK o INPUT |
| `net_poll()` | 728 | Recoge todo lo que el driver tenga y le devuelve los buffers. **Lo único que mueve datos hacia dentro** |

### `src/net.c` — capa 3, lockstep

| Función | Línea | Qué hace |
|---|---|---|
| `net_store_remote_input()` | 594 | Archiva un frame de teclas de la otra máquina. Descarta lo ya simulado y lo que va demasiado por delante |
| `net_check_remote_checksum()` | 622 | Compara su checksum con el nuestro de ese frame. Si no cuadra, `NET DESYNC` al log |
| `net_set_local_input()` | 1046 | Nuestras teclas entran en `frame + NET_INPUT_DELAY`. **Todo el retardo de entrada es esta línea** |
| `net_send_input()` | 1063 | Manda los últimos 8 frames de teclas, y el checksum cuando toca |
| `net_has_remote_input()` | 1129 | ¿Han llegado sus teclas para el frame actual? Mientras sea 0, el juego no avanza |
| `net_get_remote_input()` | 1148 | Sus teclas para este frame |
| `net_get_local_input()` | 1159 | Las nuestras para este frame. **Del anillo, no del teclado**: por eso se aplican igual de tarde que las suyas |
| `net_set_local_checksum()` | 1175 | Guarda el resumen del estado y, cada 30 frames, lo aparta para que viaje |
| `net_advance_frame()` | 1199 | Frame siguiente |

### `src/net.c` — el ciclo de vida y los avisos

| Función | Línea | Qué hace |
|---|---|---|
| `net_init()` | 775 | Busca el driver, abre el socket, deja los 4 buffers escuchando |
| `net_find_opponent()` | 878 | El HELLO / HELLO_ACK hasta emparejar. En modo texto |
| `net_is_player1()` | 1024 | Qué tanque nos ha tocado |
| `net_get_frame()` | 1031 | El frame que se está simulando |
| `net_connection_lost()` | 1213 | 1 cuando llevamos 10 segundos sin recibir nada |
| `net_desync_detected()` | 1243 | 1 cuando las dos máquinas han calculado estados distintos |
| `net_count_wait()` | 1254 | Cuenta un frame parado esperando, para el resumen del log |
| `net_shutdown()` | 848 | Cierra el socket y escribe el resumen |

### `src/main.c` — lo que se añadió

| Función | Línea | Qué hace |
|---|---|---|
| `read_input_from_keys()` | 563 | Convierte este teclado en el byte de 5 bits. **La única que sigue leyendo `keys[]`** |
| `compute_state_checksum()` | 611 | Resume todo el estado del juego en un número, para detectar desincronización |
| `set_text_mode()` | 545 | Vuelve al modo texto al salir, para poder leer el mensaje final |
| `process_player_input()` | 915 | **Modificada.** Antes leía `keys[]`; ahora recibe el byte ya resuelto. Ese es el único cambio de fondo que hizo falta en todo el juego |

### Y lo que **no** se tocó

`is_blocked_by_wall()`, `is_blocked_by_tank()`, `is_move_blocked()`,
`bullet_has_hit_tank()`, `update_bullet()`, `move_sprite()`,
`update_player_animation()`, `draw_to_buffer()`, `draw_explosion()`,
`restart_game()`, `update_game()`, todo `players.c`, todo `sound.c` y todo
`bmp.c`.

Eso no es casualidad ni suerte: es la consecuencia de haber elegido lockstep.
Como las dos máquinas ejecutan el juego entero y solo se intercambian teclas,
no hay nada del juego que necesite enterarse de que existe una red.
