# Manual: cómo se hizo que el juego funcione en red

De cero a dos tanques peleando entre dos ordenadores.

---

## Para quién es esto

Para ti dentro de seis meses, cuando abras `net.c` y no te acuerdes de nada.

Doy por sabido que dominas C, que te manejas en DOS y que **entiendes tu propio
juego**: el bucle principal, las colisiones, cómo se dibuja, cómo se mueve un
tanque.

Doy por no sabido **absolutamente nada de redes**. Ni qué es un socket, ni qué
es un paquete, ni qué es un protocolo. Todo eso se explica aquí desde el
principio.

### Cómo leerlo

Está en **orden de aprendizaje**, no en orden de fichero. Cada parte se apoya
en la anterior:

| Parte | De qué va |
|---|---|
| **1** | El problema. Qué significa de verdad "jugar en red" |
| **2** | La idea que lo resuelve: determinismo y lockstep |
| **3** | Cómo se manda un byte en DOS: IPX |
| **4** | El código, leído en orden de ejecución |
| **5** | **Cómo se enchufa todo esto a tu juego** |
| **6** | Qué pasa cuando algo falla |
| **7** | Experimentos para trastear, y glosario |

**No te saltes la parte 2.** Es el 80% de la comprensión. La parte 3 y la 4 son
mecánica; sin la 2 son ruido.

Cuando termines, [`NETWORK.md`](NETWORK.md) es la referencia para consultar
cosas sueltas.

---

# PARTE 1 — EL PROBLEMA

## 1. Dos ordenadores, una partida

Ahora mismo tu juego funciona así:

```
   +-------------------------------------------+
   |              UN ORDENADOR                 |
   |                                           |
   |   teclado ---> keys[] ---> juego ---> VGA |
   |                                           |
   |   los DOS jugadores comparten todo:       |
   |   el mismo teclado, la misma memoria,     |
   |   la misma pantalla, el mismo bucle       |
   +-------------------------------------------+
```

Los dos tanques viven en las mismas variables (`player1` y `player2`), se
mueven en el mismo `while`, y se dibujan en el mismo buffer. **No hay nada que
coordinar**, porque no hay dos de nada.

Jugar en red significa esto:

```
   +---------------------+          +---------------------+
   |   ORDENADOR A       |          |   ORDENADOR B       |
   |                     |          |                     |
   |  teclado A          |          |          teclado B  |
   |     |               |          |               |     |
   |     v               |          |               v     |
   |  player1, player2   | <------> |  player1, player2   |
   |     |               |   ???    |               |     |
   |     v               |          |               v     |
   |  pantalla A         |          |         pantalla B  |
   +---------------------+          +---------------------+
```

Fíjate bien, porque aquí está todo el problema:

> **Hay DOS copias de `player1` y `player2`.** Una en cada ordenador. Son
> variables distintas, en memorias distintas, en máquinas distintas.

Y las dos tienen que contar **la misma historia**. Si en tu pantalla tu tanque
está en la posición (110, 80) y en la del otro está en (110, 84), estáis
jugando a dos partidas diferentes que parecen la misma. Tú disparas y le das;
él ve que la bala pasa de largo.

**Ese es el problema entero.** Todo lo demás de este manual es cómo se resuelve.

---

## 2. La idea ingenua, y por qué no funciona

Lo primero que se le ocurre a cualquiera es esto:

> "Fácil: cada ordenador manda dónde está su tanque, y el otro lo pinta ahí."

Vamos a intentarlo mentalmente. Cada frame, el ordenador A manda:

```
mi tanque está en (110, 80), mirando arriba, sin bala
```

Y B lo recibe y pone su copia de `player1` en (110, 80).

Parece que funciona. **No funciona**, y por cuatro razones que conviene ver
ahora porque explican todas las decisiones que vienen después.

### Problema 1: se manda muchísimo más de lo que parece

No es solo la posición. Piensa en todo lo que hay que mandar para que las dos
pantallas se vean iguales:

- posición X e Y del tanque
- dirección a la que mira
- qué frame de la animación de orugas toca
- posición de la bala
- dirección de la bala
- si la bala está volando o no
- si el tanque está explotando
- por qué frame va la explosión
- el marcador

Y eso **cada frame, 70 veces por segundo**. Son unos 20-30 bytes por máquina y
frame en vez de 1. No es dramático, pero es 30 veces más, y esto es una red de
1994.

### Problema 2: llegas siempre tarde

Los paquetes tardan. Aunque sea 1 milisegundo, tardan. Así que cuando B recibe
"el tanque de A está en (110, 80)", **A ya no está ahí**. B está pintando el
pasado.

Con 5 ms de retraso y un tanque a 2 píxeles por frame, B pinta el tanque de A
unos 5 píxeles por detrás de donde está de verdad. Tú ves tu tanque en un sitio
y el otro lo ve en otro. ¿Quién tiene razón cuando disparas?

### Problema 3: ¿quién decide?

Imagina que los dos tanques van uno hacia el otro y chocan.

- El ordenador A calcula la colisión y dice: "el tanque 2 se para aquí".
- El ordenador B calcula la misma colisión con datos un poco más viejos y dice:
  "el tanque 2 se para dos píxeles más allá".

¿Quién gana? No hay respuesta. Necesitarías nombrar a uno de los dos "el que
manda" (un **servidor autoritativo**), y entonces el otro jugador tiene que
tragar con lo que diga el primero, incluso sobre su propio tanque. Eso es lo
que produce esa sensación de goma elástica en los juegos online: tu personaje
avanza y de pronto lo tiran para atrás porque el servidor no estaba de acuerdo.

### Problema 4: hay que tocar todo el juego

Y este es el que de verdad duele. Para mandar el estado hay que:

- escribir código que empaquete `struct player` en bytes
- escribir código que lo desempaquete
- acordarse de actualizarlo **cada vez** que añadas un campo al struct
- decidir qué hacer cuando un paquete llega tarde o desordenado
- escribir interpolación para que el tanque del otro no vaya a saltos

**Tu juego dejaría de ser tu juego.** Cada función que toca `player1` tendría
que enterarse de que existe una red.

### Entonces, ¿qué?

Guarda esos cuatro problemas. En el capítulo 5 se resuelven **los cuatro de
golpe**, con una idea que parece un truco de magia.

---

## 3. Las dos preguntas de todo juego en red

Todo lo que hemos visto se reduce a dos preguntas. Cualquier juego en red del
mundo, de 1994 o de hoy, contesta a estas dos:

> **Pregunta 1: ¿QUÉ mando?**
>
> **Pregunta 2: ¿Cómo consigo que las dos máquinas estén de acuerdo?**

La idea ingenua contesta:

1. Mando **el estado** (dónde está todo).
2. Nombro a uno el jefe y el otro obedece.

Nosotros vamos a contestar de otra manera **completamente distinta**:

1. Mando **las teclas pulsadas**. Nada más.
2. No hace falta ningún jefe: las dos máquinas calculan lo mismo por su cuenta.

Suena imposible. El capítulo siguiente explica por qué es posible.

---

# PARTE 2 — LA IDEA QUE LO RESUELVE

## 4. Determinismo: por qué tu juego puede hacer trampa

Esta es la idea central de todo el manual. Si te llevas una sola cosa, que sea
esta.

**Determinista** significa: *dadas las mismas entradas, produce siempre
exactamente el mismo resultado*.

Míralo en tu propio código. Coge `move_sprite()`:

```c
if (direction == MOVE_UP){
    if (_player->position_y >= PIXEL_TO_MOVE){
        _player->position_y = _player->position_y - PIXEL_TO_MOVE;
    }else{
        _player->position_y = 0;
    }
}
```

Si `position_y` vale 80 y `direction` es `MOVE_UP`, el resultado es **78**.
Siempre 78. En tu Pentium III, en un 486, en un Core i9, en DOSBox, hoy y
dentro de veinte años. No hay ninguna forma de que salga otra cosa.

Ahora mira tu juego entero con esos ojos:

| | ¿Lo hay en tu juego? | Por qué importaría |
|---|---|---|
| Números decimales (`float`) | **No** | Dos CPUs pueden redondear distinto en el último bit. Ese bit se propaga y a los mil frames los tanques están en sitios diferentes |
| `rand()` | **No** | Cada máquina sacaría números distintos |
| Leer el reloj | **No** | Los relojes de dos máquinas nunca coinciden |
| Punteros como valores | **No** | `farmalloc()` puede devolver direcciones distintas en cada máquina |
| Memoria sin inicializar | **No** | Basura distinta en cada máquina |

Tu juego es **entero puro**: `unsigned int` para todo, sumas, restas y
comparaciones. Y eso, que hiciste porque era lo natural en Turbo C, resulta que
te regala la propiedad más valiosa que puede tener un juego para jugarlo en red.

> **Si las dos máquinas empiezan en el mismo estado y reciben la misma secuencia
> de teclas, calculan exactamente los mismos píxeles hasta el final de la
> partida.**
>
> No "parecidos". **Los mismos.** Bit a bit.

Léelo otra vez, porque es contraintuitivo y es de donde sale todo lo demás.

### Un experimento mental

Imagina que grabas en un fichero todas las teclas que se pulsaron durante una
partida, frame a frame:

```
frame 0: nada
frame 1: nada
frame 2: jugador1=ARRIBA, jugador2=nada
frame 3: jugador1=ARRIBA, jugador2=IZQUIERDA
...
frame 4210: jugador1=DISPARO, jugador2=ARRIBA
```

Si mañana arrancas el juego y en vez de leer el teclado lees ese fichero, verás
**exactamente la misma partida**. Los mismos movimientos, los mismos disparos,
la misma explosión en el mismo píxel, el mismo ganador.

Eso es lo que significa ser determinista. Y fíjate en el tamaño de ese fichero:
un byte por jugador y frame. Una partida de cinco minutos son 21.000 frames,
o sea **42 KB**. Cabe en un disquete una partida entera.

---

## 5. Lockstep: no mandes posiciones, manda teclas

Ahora junta las dos ideas:

1. El juego es determinista.
2. Un fichero de teclas reproduce la partida entera.

> **¿Y si en vez de guardar las teclas en un fichero, se las mando a la otra
> máquina en tiempo real?**

Eso es **lockstep**. Y esto es lo que cambia:

```
   +---------------------+          +---------------------+
   |   ORDENADOR A       |          |   ORDENADOR B       |
   |                     |          |                     |
   |  teclado A          |          |          teclado B  |
   |     |               |          |               |     |
   |     +---> 1 byte -------------------> (recibe)  |    |
   |     |               |          |               |     |
   |     |    (recibe) <-------------------- 1 byte -+    |
   |     v               |          |               v     |
   |  SIMULA LOS DOS     |          |  SIMULA LOS DOS     |
   |  TANQUES ENTEROS    |          |  TANQUES ENTEROS    |
   |     |               |          |               |     |
   |     v               |          |               v     |
   |  pantalla A         |          |         pantalla B  |
   +---------------------+          +---------------------+
```

Las dos máquinas ejecutan **el juego completo**. Los dos tanques. Las dos
balas. Las colisiones de los dos. Las explosiones de los dos. El marcador.
Todo.

Ninguna máquina es cliente de la otra. **Las dos son el juego entero.**

Y lo único que cruza el cable es:

```
un byte por jugador y por frame
```

Ese byte son cinco bits:

```c
#define NET_INPUT_UP		0x01     // bit 0
#define NET_INPUT_DOWN		0x02     // bit 1
#define NET_INPUT_LEFT		0x04     // bit 2
#define NET_INPUT_RIGHT		0x08     // bit 3
#define NET_INPUT_FIRE		0x10     // bit 4
```

Si el jugador tiene pulsada ARRIBA y DISPARO a la vez, el byte vale `0x11`.

### Mira cómo se resuelven los cuatro problemas del capítulo 2

Esto es lo bonito:

| Problema de la idea ingenua | Cómo lo mata el lockstep |
|---|---|
| **1. Se manda muchísimo** | 1 byte en vez de 30. Y da igual cuántas cosas añadas al juego: **sigue siendo 1 byte**. Puedes meter diez tanques, minas y power-ups, y lo que viaja no crece |
| **2. Llegas tarde** | No pintas el pasado: pintas exactamente lo mismo que el otro, en el mismo frame. Las dos pantallas son idénticas siempre |
| **3. ¿Quién decide?** | Nadie. No hay jefe. Las dos máquinas calculan la colisión con **los mismos datos** y llegan al mismo resultado. No hay nada que negociar |
| **4. Hay que tocar todo el juego** | **No se toca nada.** Ni colisiones, ni balas, ni explosiones, ni dibujado, ni sonido. Ni una línea |

Ese último es el importante y lo veremos en detalle en la parte 5.

### El bucle, en tres líneas

```
frame N:  1. mandar mis teclas
          2. esperar las teclas del otro para el frame N
          3. simular el frame N con LAS DOS
```

Y ya está. Ese es el juego en red entero, conceptualmente. Todo lo demás son
detalles de cómo hacer que funcione de verdad.

### El precio

Lockstep tiene un coste y hay que conocerlo:

> **Si un paquete no llega, las DOS máquinas se paran.**

No es que una vaya peor. Es que la que espera no puede avanzar, porque no sabe
qué teclas aplicar, y si avanzase inventándoselas ya no estaría calculando la
misma partida. Así que espera. Y la otra, al frame siguiente, también espera.

Los capítulos 6 y 7 son las dos cosas que hacemos para que eso casi nunca pase.

---

## 6. El error que desincroniza: el retardo de entrada

Este capítulo explica **el fallo que comete todo el mundo la primera vez**.
Incluido yo si no tuviera cuidado.

### El intento evidente

Lo natural sería programarlo así:

```
frame N:  mis teclas -> aplicar YA a mi tanque         <-- se siente genial
          las teclas del otro que hayan llegado -> aplicar a su tanque
```

Tu tanque responde al instante. Perfecto. **Y te desincronizas en menos de un
segundo.**

### Por qué falla

Vamos a trazarlo. Tú pulsas ARRIBA en el frame 100. Tu paquete tarda 3 frames
en llegar a la otra máquina.

```
                MÁQUINA A (tú)              MÁQUINA B (el otro)
frame 100   tu tanque sube 2 px         tu tanque NO se mueve
                                        (el paquete va de camino)
frame 101   tu tanque sube 2 px         tu tanque NO se mueve
frame 102   tu tanque sube 2 px         tu tanque NO se mueve
frame 103   tu tanque sube 2 px         llega el paquete: sube 2 px
```

En el frame 103, tu tanque está en `y = 80 - 8` en tu pantalla y en `y = 80 - 2`
en la suya. **Seis píxeles de diferencia.**

Y esto no se arregla solo, se hace más grande. Peor todavía: en cuanto haya una
colisión, las dos máquinas calcularán cosas distintas, y a partir de ahí las
partidas divergen sin remedio. Uno ve que la bala impacta, el otro que falla.

El origen del fallo es tratar tus teclas y las del otro **de forma distinta**.
Una se aplica en el frame 100 y la otra en el 103.

### La solución: retrasar también las tuyas

```
frame N:  mis teclas -> guardarlas para el frame N + 5
          simular el frame N con las teclas que se guardaron para el frame N
                                (las mías Y las suyas)
```

O sea: **tus propias teclas también esperan.** Exactamente lo mismo que las del
otro.

```
                MÁQUINA A (tú)              MÁQUINA B (el otro)
frame 100   guardo ARRIBA para el 105    (el paquete va de camino)
frame 101   tanque quieto                llega: guarda ARRIBA para el 105
frame 102   tanque quieto                tanque quieto
frame 103   tanque quieto                tanque quieto
frame 104   tanque quieto                tanque quieto
frame 105   ARRIBA: sube 2 px            ARRIBA: sube 2 px      <-- IGUALES
```

En el frame 105 **las dos máquinas hacen lo mismo**. Y en el 106, y en el 4210.
No pueden separarse.

Eso es `NET_INPUT_DELAY`, y vale 5:

```c
#define NET_INPUT_DELAY 	5
```

### ¿Cuánto se nota?

El bucle va a 70 fps, así que un frame son **14,3 ms**. Cinco frames son
**71 ms** entre que pulsas y tu tanque se mueve.

¿Es mucho? Depende del juego:

- En un juego de lucha sería inaceptable.
- Aquí, con `PIXEL_TO_MOVE 2`, esos cinco frames son **10 píxeles** de retraso
  en arrancar, en una pantalla de 320 con tanques de 18 píxeles y con la
  inercia que tiene un tanque. **No se nota.**

### Por qué 5 y no 1

Porque el retardo es el **colchón** que absorbe los caprichos de la red. Si el
paquete tarda 4 frames en llegar y el retardo es de 5, llega a tiempo y no
notas nada. Si tarda 6, el juego se para un frame.

| `NET_INPUT_DELAY` | Colchón | Cuándo usarlo |
|---|---|---|
| 3 | ~43 ms | Cable, o dos DOSBox en el mismo PC |
| **5** | **~71 ms** | **Lo que hay puesto. Wifi normal** |
| 7 | ~100 ms | Wifi mala, o jugar por internet |

Cuanto más alto, más aguanta la red y más tarda tu tanque en responder. Es un
intercambio, y no hay un valor "correcto": depende de tu red.

> **Regla de oro del lockstep, y la única que no puedes romper:**
>
> Las dos entradas se aplican en el mismo frame en las dos máquinas. Siempre.
> En cuanto haces una excepción "solo para que se sienta mejor", se acabó.

---

## 7. Qué pasa si se pierde un paquete: la redundancia

Ya sabemos qué mandar y cuándo aplicarlo. Falta un problema: **los paquetes se
pierden**.

En una red no hay ninguna garantía. Un paquete puede perderse, puede llegar
tarde, o pueden llegar dos desordenados. Es normal y hay que contar con ello.

### Lo que haría un programa normal

Un programa serio pediría el paquete otra vez:

```
B: "oye, no me ha llegado el frame 105, mándamelo"
A: "toma"
```

Eso se llama **retransmisión**, y es lo que hace TCP por ti. Y aquí sería
**terrible**: para cuando ha ido la petición y vuelto la respuesta han pasado
dos viajes completos de red. El juego lleva parado todo ese rato.

### Lo que hacemos: mandarlo antes de que lo pidan

Mira el tamaño real de un paquete nuestro:

```
+--------------------------------+
|      cabecera IPX: 30 bytes    |   <-- obligatoria, la pone el protocolo
+--------------------------------+
|      nuestros datos: 30 bytes  |   <-- de los cuales 8 son teclas
+--------------------------------+
   total: 60 bytes
```

Aquí está la observación clave:

> **Mandar 1 byte de teclas o mandar 8 cuesta el mismo paquete.**

La cabecera pesa 30 bytes hagas lo que hagas. Los 7 bytes extra son ruido
comparados con eso. **La redundancia es gratis.**

Así que cada paquete lleva los **últimos 8 frames** de teclas, no solo el
actual:

```c
#define NET_REDUNDANCY 		8
```

### Cómo se ve

Si estás en el frame 100 y el retardo es 5, el paquete que sale lleva las
teclas de los frames **98 a 105**:

```
paquete del frame 100:  [98][99][100][101][102][103][104][105]
paquete del frame 101:  [99][100][101][102][103][104][105][106]
paquete del frame 102:  [100][101][102][103][104][105][106][107]
```

Cada frame viaja **ocho veces**, en ocho paquetes distintos.

Si se pierde el paquete del frame 100, no pasa absolutamente nada: el frame 105
también venía en el paquete del 101, y en el del 102, y en seis más. El de
detrás lo cubre.

**Para que el juego se pare de verdad tendrían que perderse 8 paquetes
seguidos.** En una red normal eso no pasa casi nunca.

### El detalle bonito

Fíjate en lo que hemos conseguido:

- **Sin retransmisiones.** Nadie pide nada.
- **Sin acuses de recibo.** Nadie confirma nada.
- **Sin ordenación.** Si los paquetes llegan desordenados da igual: cada uno
  dice a qué frames pertenece lo que trae, y se archiva en su sitio.
- **Sin conexión.** No hay nada que "establecer" ni que mantener vivo.

Todo eso es complejidad que **no existe** en este código, y no existe porque
sobra. Es la razón de que `net.c` quepa en 1200 líneas comentadas.

Y es también la razón de elegir un protocolo tonto en vez de uno listo: TCP
haría por su cuenta todo lo que acabamos de decidir no hacer, y nos metería sus
tirones. En la parte 3 se ve qué protocolo usamos y por qué.

---

# PARTE 3 — CÓMO SE MANDA UN BYTE EN DOS

Ya sabemos **qué** hay que mandar (un byte de teclas) y **cuándo** aplicarlo
(cinco frames después). Falta lo más terrenal: cómo se manda de verdad.

## 8. Qué es "la red" en DOS

Si vienes de programar en cualquier sistema moderno, tienes una idea de red que
aquí **no existe**. En Linux o Windows haces:

```c
socket();  connect();  send();
```

y el sistema operativo se encarga de todo. En DOS eso no está. Y no está por
una razón de fondo:

> **DOS no es un sistema operativo multitarea. Es un cargador de programas con
> un sistema de ficheros.** No hay drivers, no hay pila de red, no hay nada
> corriendo de fondo. Cuando tu programa arranca, tu programa **es** la máquina.

Turbo C++ 3.0 tampoco tiene nada. Su librería es DOS puro: `fopen`, `int86`,
`inp`, `outp`. No hay `<sys/socket.h>`. No existe.

### Entonces, ¿cómo hablaban por red los programas de DOS?

Con un **TSR**: *Terminate and Stay Resident*. Un programa que arrancas una vez,
se queda en memoria, y se va. Como tu propio manejador de INT 9, pero en un
`.COM` aparte.

```
autoexec.bat:
    LSL.COM          <- se queda en memoria
    RTSODI.COM       <- se queda en memoria
    IPXODI.COM       <- se queda en memoria
    ...
    game.exe         <- tu juego, que les habla
```

Ese TSR es "la red". Sabe hablar con la tarjeta, sabe mandar y recibir, y deja
**una puerta de entrada** para que otros programas le pidan cosas.

### ¿Y cómo se le habla a algo que ya está en memoria?

Igual que le hablas a la BIOS: **por interrupción**. Cargas unos registros,
haces `int` de un número concreto, y el TSR hace su trabajo.

Es exactamente el mismo mecanismo que ya usas en el juego. `set_vga_320_200_mode()`
hace `int 0x10` para hablar con la BIOS de vídeo. Aquí haremos algo muy
parecido para hablar con el driver de red.

**Esa es toda la magia.** No hay nada más misterioso: la red en DOS es un
programa que ya está en memoria y al que le haces llamadas.

---

## 9. Qué es IPX

**IPX** son las siglas de *Internetwork Packet eXchange*. Es el protocolo de
red de **Novell NetWare**, de mediados de los 80.

Un poco de contexto histórico, porque ayuda a entenderlo: en los 80 y primeros
90, "red de oficina" significaba NetWare. TCP/IP era cosa de universidades y de
Unix. Si tenías PCs conectados en una empresa, casi seguro que hablaban IPX. Y
por eso **todos** los juegos multijugador de DOS lo usaban: Doom, Duke Nukem 3D,
Warcraft II, Command & Conquer, Descent...

### Comparado con lo que conoces hoy

| | TCP/IP (hoy) | IPX |
|---|---|---|
| Dirección de máquina | IP: `192.168.1.45` | **Nodo**: `00:1A:2B:3C:4D:5E` (¡la MAC!) |
| Puerto / canal | Puerto: `8080` | **Socket**: `0x869C` |
| Red | Máscara, gateway, DHCP | **Número de red** de 4 bytes, normalmente 0 |
| Nombres | DNS | No hay. No hacen falta |
| Versión "sin garantías" | UDP | **IPX** ← la que usamos |
| Versión "con garantías" | TCP | SPX ← no la usamos |

### Las tres cosas que lo hacen ideal aquí

**1. La dirección es la MAC de la tarjeta.**

Esto es más importante de lo que parece. En TCP/IP, una IP es un número que
*alguien* tiene que asignar: o lo configuras a mano o montas un DHCP. En IPX, la
dirección **ya viene de fábrica grabada en la tarjeta**. No hay nada que
configurar. Enchufas dos máquinas y ya se pueden hablar.

Y hay una dirección especial:

```
FF:FF:FF:FF:FF:FF   =  "todos los de esta red"
```

Eso se llama **broadcast**, y es lo que permite que las dos copias del juego se
encuentren solas sin que nadie teclee una dirección. Volveremos a ello en el
capítulo 13.

**2. No hay pila que enlazar.**

Esta es la decisiva. Si quisiéramos TCP/IP tendríamos que meter una biblioteca
(WATTCP) dentro del ejecutable, con su ARP, su IP, su UDP y su gestión de
buffers. Y **recompilarla para el modelo HUGE**, porque las versiones
precompiladas de la época venían para LARGE.

Con IPX no se enlaza nada. Ni un `.LIB`. El driver ya está en memoria y solo
hay que llamarlo.

**3. Es tonto, y eso es lo que queremos.**

IPX no reintenta, no ordena, no garantiza entrega, no controla la congestión.
Mandas un paquete y se va. Si llega, llega.

Suena a defecto y es **justo lo que necesita el capítulo 7**. Ya decidimos que
no queremos retransmisiones. TCP nos las daría queramos o no.

### Lo que IPX no te da, y ponemos nosotros

| Falta | Dónde lo resolvemos |
|---|---|
| Fiabilidad | Redundancia de 8 frames (cap. 7) |
| Saber quién está al otro lado | El HELLO por broadcast (cap. 13) |
| Que las dos partidas cuadren | Lockstep (cap. 5) |

---

## 10. El ECB: el formulario que se le rellena al driver

Ahora lo práctico: ¿cómo le dices al driver "manda esto"?

No le pasas argumentos como a una función de C. Le rellenas **una estructura en
memoria** y le das su dirección. Esa estructura se llama **ECB**: *Event
Control Block*.

Piénsalo como un formulario de correos:

```
   +---------------------------------------------+
   |  ECB - "formulario de envío"                |
   +---------------------------------------------+
   |  ¿A qué socket?          0x869C             |
   |  ¿A qué MAC?             FF:FF:FF:FF:FF:FF  |
   |  ¿Dónde están los datos? -----> [paquete]   |
   |  ¿Cuántos bytes?         60                 |
   |                                             |
   |  in_use:  [ ] <-- el driver lo marca        |
   +---------------------------------------------+
```

Rellenas el formulario, se lo das, y el driver hace el trabajo. La estructura en
`net.c` es literalmente eso:

```c
struct ipx_ecb {
    void far      *link_address;         // uso interno del driver
    void far      (*esr_address)();      // SIEMPRE NULL aquí, ver abajo
    unsigned char  in_use;               // el driver lo marca mientras trabaja
    unsigned char  completion_code;      // 0 = salió bien
    unsigned int   socket_number;
    unsigned char  ipx_workspace[4];     // uso interno del driver
    unsigned char  driver_workspace[12]; // uso interno del driver
    unsigned char  immediate_address[6]; // la MAC de destino
    unsigned int   fragment_count;       // siempre 1 aquí
    void far      *fragment_address;     // dónde está el paquete
    unsigned int   fragment_size;        // cuánto ocupa
};
```

### `in_use`: quién es el dueño del buffer

Este campo es el corazón del asunto y merece pararse.

> Un buffer **o es tuyo o es del driver, nunca las dos cosas a la vez.**

- Le das el ECB → el buffer pasa a ser **del driver**. `in_use` se pone a algo
  distinto de cero.
- El driver termina → pone `in_use` a **0**. El buffer vuelve a ser **tuyo**.

Si escribes en el buffer mientras `in_use` no es cero, estás modificando un
paquete que el driver puede estar mandando en ese instante. Eso es exactamente
el bug que había en la primera versión de `net_transmit()` y por eso existe
`net_send_is_busy()`.

### `esr_address`: la trampa que no pisamos

IPX te ofrece algo tentador: puedes darle la dirección de una función tuya, y
te llama **en cuanto llega un paquete**. Automático, sin tener que preguntar.

**Aquí va siempre a NULL, a propósito.**

Porque esa función se ejecutaría **en tiempo de interrupción**: en medio de lo
que estuviera haciendo el juego. Podría interrumpir a `draw_to_buffer()` a
mitad, o al mezclador de sonido, o a tu propio manejador de teclado. Todo lo
que toque tendría que ser reentrante.

Ya sabes lo que eso significa, porque tu ISR de sonido tiene el mismo cuidado:
`sound_isr()` solo levanta una bandera y se va.

Aquí hacemos lo mismo pero aún más simple: **no hay ISR de red**. Una vez por
frame miramos `in_use` y ya está. Se llama **sondeo** (*polling*), es lo más
aburrido que hay, y no puede salir mal.

### La cabecera IPX

Además del ECB, el paquete en sí lleva delante una cabecera de 30 bytes que
define el protocolo:

```c
struct ipx_header {
    unsigned int   checksum;                // 0xFFFF (IPX nunca lo usó de verdad)
    unsigned int   length;                  // tamaño total
    unsigned char  transport_control;       // saltos de router. 0 aquí
    unsigned char  packet_type;             // 4 = datagrama normal
    unsigned char  destination_network[4];  // 0 = "esta misma red"
    unsigned char  destination_node[6];     // la MAC de destino
    unsigned char  destination_socket[2];
    unsigned char  source_network[4];       // \
    unsigned char  source_node[6];          //  > esto lo rellena el driver
    unsigned char  source_socket[2];        // /
};
```

Fíjate en la última parte: **el remitente lo pone el driver**, no nosotros. Por
eso, cuando recibimos un paquete, sabemos automáticamente de quién viene sin
que nadie nos lo tenga que decir. Eso es lo que hace posible el descubrimiento
del capítulo 13.

### Un detalle que confunde: el orden de los bytes

IPX escribe los números **con el byte gordo delante** (*big endian*):

```
socket 0x869C  se escribe en memoria como:   86 9C
```

Pero el 8086 guarda los `unsigned int` **al revés** (*little endian*):

```
unsigned int x = 0x869C;  se guarda en memoria como:   9C 86
```

Así que para que en memoria quede `86 9C`, hay que meter en la variable el
valor `0x9C86`. Por eso existe esta función:

```c
static unsigned int net_swap16(unsigned int value){
    unsigned int high_byte;
    unsigned int low_byte;

    high_byte = (value >> 8) & 0x00FF;
    low_byte  = value & 0x00FF;

    return (low_byte << 8) | high_byte;
}
```

`net_swap16(0x869C)` devuelve `0x9C86`, que en memoria queda como `86 9C`, que
es lo que IPX quiere leer. Suena a lío y es solo esto: **darle la vuelta a dos
bytes**.

---

## 11. La llamada al driver, y por qué es ensamblador

Ya tenemos el formulario relleno. ¿Cómo se lo damos al driver?

### Encontrar la puerta

Primero hay que saber si el driver está y dónde está su entrada. Se pregunta con
la interrupción `2F`, que es el "¿hay alguien ahí?" de DOS:

```c
static int ipx_detect(void){
    union  REGS  regs;
    struct SREGS sregs;

    segread(&sregs);

    regs.x.ax = 0x7A00;          // 7A00 = "¿estás, IPX?"
    int86x(0x2F, &regs, &regs, &sregs);

    if (regs.h.al != 0xFF){      // 0xFF = "sí, aquí estoy"
        return 0;                // cualquier otra cosa: no hay driver
    }

    ipx_entry_segment = sregs.es;   // y me deja su dirección en ES:DI
    ipx_entry_offset  = regs.x.di;

    return 1;
}
```

Esto es idéntico en espíritu a lo que ya haces con la BIOS de vídeo. Cargas
`AX`, haces la interrupción, y miras lo que vuelve.

Si `AL` vuelve con `0xFF`, hay IPX, y `ES:DI` es **la dirección a la que hay
que llamar** de ahora en adelante. Se guarda en dos variables globales.

Este es el camino documentado por Novell y el que usaba Doom. Funciona con un
`IPXODI` de verdad, con el cliente de Novell, y con DOSBox, que responde a esta
llamada exactamente igual que un driver real. **Por eso el mismo `.EXE` sirve
para las dos cosas.**

### Llamar a la puerta

Y aquí viene la única parte fea de todo el módulo. IPX no se llama como una
función de C. Quiere:

- El número de la operación en el registro **BX**
- La dirección del ECB en **ES:SI**
- Y se entra con una **llamada lejana** (`CALL FAR`), no con una interrupción

**Ninguna de esas tres cosas se puede expresar en C.** C no te deja decir "pon
esto en BX". De ahí el ensamblador.

El problema gordo es el tercero. Piénsalo: la dirección a la que hay que llamar
está **en una variable** (la guardamos antes). Y en el 8086:

> **No existe una instrucción "llama lejos a la dirección que hay en estos dos
> registros".** No está. Puedes llamar lejos a una dirección constante, o a una
> que esté en memoria, pero no a una que esté en registros.

La solución es empujar la dirección a la pila y llamar **desde la pila**:

```asm
    push    cx              ; el segmento del driver
    push    dx              ; el offset del driver
    mov     bp, sp          ; BP apunta ahora a esos 4 bytes
    call    dword ptr [bp]  ; llamada lejana a la dirección que hay en [BP]
    add     sp, 4           ; y limpiamos lo que empujamos
```

`call dword ptr [bp]` significa: "lee 4 bytes de la posición a la que apunta BP,
interprétalos como segmento:offset, y llama ahí". Como acabamos de dejar la
dirección justo ahí, funciona.

### Lo que se salva y por qué

El bloque completo salva cinco registros:

```asm
    push si
    push di
    push ds
    push es
    push bp
    ...
    pop  bp
    pop  es
    pop  ds
    pop  di
    pop  si
```

Dos merecen explicación:

**`BP`** — el compilador lo usa para acceder a variables locales y parámetros.
Nosotros se lo pisamos con `mov bp, sp` para hacer la llamada, y encima el
driver puede devolverlo apuntando a cualquier sitio. Por eso se restaura
**antes** de volver a tocar nada relativo a BP. Si te fijas en `ipx_socket_call()`,
el `mov result_code, ax` está **después** del `pop bp`, y no es casualidad: si
estuviera antes, escribiría en mitad de la pila.

**`DS`** — en modelo HUGE el compilador da por hecho que DS sigue apuntando a
los datos de este módulo cuando acaba el bloque de ensamblador. El driver no
promete nada de eso. Si lo cambia y no lo restauramos, **todas las variables
globales de `net.c` apuntarían a otro sitio** a partir de ahí. Es un fallo que
se manifestaría de formas incomprensibles muy lejos de donde está la causa.

> **Nota práctica:** si algún día TCC se queja de `call dword ptr [bp]`, esa
> instrucción se puede escribir a mano en bytes: `db 0FFh, 05Eh, 000h`. Está
> anotado en el propio `net.c`.

---

# PARTE 4 — EL CÓDIGO, EN ORDEN DE EJECUCIÓN

Ya tienes todas las piezas. Ahora leemos `net.c` **en el orden en que se
ejecuta**, no en el orden en que está escrito.

## 12. Arranque: `net_init()`

Se llama una vez, desde `main()`, cuando arrancas con `/net`. Hace seis cosas:

### 1. Comprobar que las estructuras miden lo que deben

```c
sprintf(net_log_text, "NET sizes: ecb=%u header=%u packet=%u (want 42/30/60)", ...);
tanks_log(net_log_text);
```

Esto parece una tontería y es de lo más útil que hay en el fichero.

El ECB tiene un tamaño **fijado por Novell byte a byte**. Si el compilador
decidiera meter relleno entre los campos para alinearlos (cosa que muchos
compiladores hacen), el driver leería **todos los campos del sitio equivocado**
y no funcionaría nada, sin ningún mensaje de error que lo explicase.

Turbo C alinea a byte por defecto (`-a-`), así que sale bien. Pero escribirlo en
el log convierte un fallo desconcertante en una línea obvia. Si algún día ves
otro número ahí, ya sabes exactamente qué pasa.

### 2. Encontrar el driver

`ipx_detect()`, lo del capítulo 11.

### 3. Abrir el socket

```c
ipx_socket_call(IPX_FUNCTION_OPEN_SOCKET, net_swap16(NET_SOCKET_NUMBER));
```

Un **socket** en IPX es como un puerto en TCP/IP: un número que identifica "esta
conversación" dentro de la máquina. Le decimos al driver "todo lo que llegue al
socket `0x869C`, dámelo a mí".

Las dos máquinas usan el mismo número, que está fijo en el código. Es lo que
separa nuestros paquetes de cualquier otra cosa que ande por la red.

### 4. Preguntar nuestra propia dirección

```c
ipx_get_local_address();
```

Nos da la MAC de nuestra tarjeta. **Solo se usa para el log**, pero es un
diagnóstico de oro: si sale toda a ceros, significa que ningún túnel nos ha
asignado dirección, o sea que no estamos conectados a nada. Es exactamente el
síntoma que apareció la primera vez que se probó esto.

### 5. Sacarse un número al azar

```c
srand((unsigned int)biostime(0, 0L));
local_instance_id = ((unsigned long)rand() << 16) | (unsigned long)rand();
```

Un número distinto en cada máquina, sacado del tick de la BIOS (que nunca
coincide entre dos ordenadores). Hace dos trabajos:

- Distinguir nuestros propios paquetes de los del otro.
- **Decidir quién es el jugador 1**, en el capítulo 13.

### 6. Dejar cuatro buzones puestos

```c
index = 0;
while (index < NET_LISTEN_ECB_COUNT){
    listen_is_posted[index] = 0;
    net_post_listen(index);
    index = index + 1;
}
```

Aquí hay algo que sorprende al principio:

> **Para recibir un paquete hay que haberle dado antes al driver un sitio donde
> ponerlo.**

No es como leer un fichero, donde pides datos cuando te apetece. El driver
necesita **por adelantado** un buffer libre. Si llega un paquete y no hay
ninguno esperando, se pierde.

Por eso se le dan **cuatro** (`NET_LISTEN_ECB_COUNT`), y no uno. Piensa por qué:
mientras estamos procesando un paquete, ese buffer es nuestro, no del driver. Si
solo hubiera uno, cualquier paquete que llegase en ese instante se perdería.
Con cuatro en rotación siempre hay alguno libre.

---

## 13. Encontrarse: `net_find_opponent()`

Ahora hay que encontrar a la otra máquina. Y aquí hay una decisión de diseño
que se nota mucho al usarlo:

> **Nadie teclea una dirección en ningún sitio.**

Ni una IP, ni una MAC, ni un nombre. Las dos copias se encuentran solas.

### Cómo

Con **broadcast**. Recuerda: la dirección `FF:FF:FF:FF:FF:FF` significa "todos
los de esta red". Así que las dos copias hacen lo mismo:

1. Gritan `HELLO` a todo el mundo, cada cuarto de segundo.
2. Contestan `HELLO_ACK` a cualquier `HELLO` que oigan.
3. Se dan por emparejadas cuando **han oído un HELLO Y les han contestado al
   suyo**.

```
      MÁQUINA A                              MÁQUINA B

  HELLO --> a todos  --------------------->  lo oye
                                             apunta su dirección y su id
                                             marca "le he oído"  (bit 0x01)
                              <-- HELLO_ACK  le contesta a él directamente
  lo recibe
  marca "me han contestado" (bit 0x02)
                                             HELLO --> a todos
  lo oye  <-----------------------------------
  apunta su dirección y su id
  marca "le he oído"  (bit 0x01)
  HELLO_ACK -->  --------------------------> lo recibe
                                             marca "me han contestado" (0x02)

  tiene los dos bits -> EMPAREJADA           tiene los dos bits -> EMPAREJADA
```

### Por qué hacen falta LOS DOS bits

Esta parte es sutil y es un fallo que estuvo en el código antes de corregirlo.

Lo evidente sería emparejar en cuanto oyes un `HELLO`. **No vale.** Mira lo que
pasaría:

- A oye el HELLO de B, se da por emparejada y **se va a jugar**.
- Al irse, A deja de mandar HELLOs.
- B nunca llegó a oír ninguno de A, y ahora ya no va a oír ninguno.
- B se queda buscando hasta que se agota el tiempo.

Exigir los dos bits garantiza que **cuando una empieza a jugar, la otra ya sabe
que la partida está en marcha**. Es un apretón de manos de dos sentidos, y es el
mínimo que funciona.

### Quién es el jugador 1

Se resuelve **sin negociar absolutamente nada**:

```c
if (local_instance_id < remote_instance_id){
    is_player1 = 1;
}else{
    is_player1 = 0;
}
```

Cada máquina tiene los dos números (el suyo y el del otro, que vino en el
HELLO). Compara. **El menor es el jugador 1**, el tanque de abajo.

Las dos máquinas hacen la misma comparación con los mismos dos números, así que
llegan a la misma conclusión por su cuenta. No hace falta que nadie decida ni
que nadie avise.

> **¿Por qué no usar la dirección MAC, que también es única?**
>
> Porque la MAC nos la da el driver, y si el driver contestara mal a esa llamada
> (devolviendo ceros, por ejemplo), **las dos máquinas se creerían el jugador 1**
> y la partida sería un disparate. El `instance_id` no depende de ninguna llamada
> al driver: nos lo inventamos nosotros. Es más robusto justo donde importa.

### Poner el marcador a cero

Antes de salir, las dos máquinas ponen `simulation_frame = 0` y limpian los
anillos. Y hay un detalle:

```c
index = 0;
while (index < NET_INPUT_DELAY){
    local_input_value[index]  = 0;
    remote_input_frame[index] = (unsigned long)index;
    remote_input_value[index] = 0;
    remote_input_valid[index] = 1;      // <-- marcados como YA recibidos
    index = index + 1;
}
```

Los primeros 5 frames se rellenan a mano con "ninguna tecla" y se marcan como
recibidos. ¿Por qué?

Porque con retardo 5, en el frame 0 mandas las teclas del frame 5. **Nadie manda
nunca las de los frames 0 a 4**, porque son anteriores al arranque. Sin este
relleno, las dos máquinas se quedarían esperando eternamente una entrada para el
frame 0 que no existe.

### Por qué no hay "pulsa una tecla para empezar"

La hubo, y **era un error**. Si un jugador tarda en pulsar, su máquina no está
atendiendo la red: sus cuatro buzones se llenan, deja de recoger paquetes, y la
otra máquina se queda esperando el frame 0 hasta agotar el tiempo.

En su lugar hay una pausa **acotada de dos segundos** que sigue llamando a
`net_poll()`. Las dos empiezan en el frame 0; la que llegue antes espera a la
otra, y el lockstep se encarga solo mientras la espera sea corta.

---

## 14. El anillo de entradas

Antes de ver el envío y la recepción, hay que entender dónde se guardan las
teclas. Son tres arrays:

```c
static unsigned char local_input_value[NET_INPUT_BUFFER_SIZE];    // las mías

static unsigned long remote_input_frame[NET_INPUT_BUFFER_SIZE];   // las suyas
static unsigned char remote_input_value[NET_INPUT_BUFFER_SIZE];
static unsigned char remote_input_valid[NET_INPUT_BUFFER_SIZE];
```

`NET_INPUT_BUFFER_SIZE` vale **64**. Y aquí está el truco: el número de frame
crece sin parar (0, 1, 2... 21000...), pero el array solo tiene 64 huecos. Así
que se le da la vuelta:

```c
index = (unsigned int)(frame & (NET_INPUT_BUFFER_SIZE - 1));
```

`frame & 63` es lo mismo que `frame % 64`, pero con un AND en vez de una
división, que en un 486 importa. Por eso el tamaño **tiene que ser potencia de
dos**.

```
  frame:   0   1   2  ...  63  64  65  ... 127 128
  índice:  0   1   2  ...  63   0   1  ...  63   0
                                 ^
                                 aquí da la vuelta y pisa el hueco del frame 0
```

Es un **buffer circular**: guarda los últimos 64 frames y va machacando los
viejos. Con 64 huecos y un retardo de 5, hay muchísimo margen.

### Por qué el remoto guarda además el número de frame

Fíjate en que las teclas del otro llevan **tres** arrays y las mías solo uno.
La razón es esta:

Si en el hueco 10 hay un `0x01`, ¿es del frame 10, del 74, o del 138? Los tres
caen en el mismo hueco. Si diéramos por bueno lo que hay sin comprobar, una
entrada vieja de una vuelta anterior se confundiría con la que estamos
esperando, y **te desincronizarías sin enterarte**.

Por eso cada hueco guarda a qué frame pertenece, y al leerlo se comprueba:

```c
if (remote_input_valid[index] == 0){ return 0; }
if (remote_input_frame[index] != simulation_frame){ return 0; }
return 1;
```

Las mías no lo necesitan porque las escribo yo mismo 5 frames antes de leerlas,
y 5 es mucho menor que 64: nunca puede haber confusión.

---

## 15. Enviar: `net_send_input()`

Se llama una vez por frame. Prepara un paquete con los últimos 8 frames de mis
teclas.

```c
newest_frame = simulation_frame + NET_INPUT_DELAY;
```

Si estoy simulando el frame 100, el más nuevo que tengo guardado es el 105
(porque acabo de guardar ahí las teclas de ahora mismo).

```c
if (newest_frame + 1 < NET_REDUNDANCY){
    count = (unsigned int)(newest_frame + 1);
}else{
    count = NET_REDUNDANCY;
}

send_packet.payload.base_frame = newest_frame - (unsigned long)count + 1;
```

Normalmente `count` es 8 y `base_frame` es `105 - 8 + 1 = 98`. O sea: el paquete
lleva los frames **98 a 105**.

El `if` es para los primerísimos frames de la partida, cuando todavía no hay 8
frames de historia hacia atrás. En el frame 0, `newest_frame` es 5, así que
manda 6 frames (0 a 5) en vez de 8.

Y luego los copia del anillo:

```c
entry = 0;
while (entry < count){
    frame = send_packet.payload.base_frame + (unsigned long)entry;
    index = (unsigned int)(frame & (NET_INPUT_BUFFER_SIZE - 1));
    send_packet.payload.inputs[entry] = local_input_value[index];
    entry = entry + 1;
}
```

### Nunca espera

```c
if (net_send_is_busy() == 1){
    return;
}
```

Si el driver todavía está con el paquete anterior, **este frame no se manda
nada**. Ni se espera ni se encola.

¿No se pierde información? No: el paquete del frame siguiente llevará los
frames 99 a 106, que incluye todo lo que iba en el que no se mandó. **La
redundancia lo cubre.** Es la misma idea del capítulo 7 aplicada a nuestro
propio lado.

Y fíjate en que la comprobación está **antes de construir el paquete**, no solo
antes de mandarlo. Es importante: mientras el envío está en vuelo, el buffer es
del driver, y escribir la carga siguiente encima reescribiría un paquete que ya
va de camino.

---

## 16. Recibir: `net_poll()`

Recoge todo lo que haya llegado. Es **lo único que mueve datos hacia dentro**,
así que cualquier bucle que espere tiene que llamarla o no llegará nada nunca.

```c
index = 0;
while (index < NET_LISTEN_ECB_COUNT){

    if (listen_is_posted[index] == 1){

        if (listen_ecb[index].in_use == 0){       // <-- el driver ha terminado

            listen_is_posted[index] = 0;

            if (listen_ecb[index].completion_code == 0){
                if (net_packet_is_valid(&listen_packet[index]) == 1){
                    net_handle_packet(&listen_packet[index]);
                }
            }

            net_post_listen(index);                // <-- devolver el buzón
        }
    }

    index = index + 1;
}
```

Recorre los cuatro buzones. En cada uno:

1. ¿Está puesto? (`listen_is_posted`)
2. ¿Ha terminado el driver con él? (`in_use == 0`)
3. Si sí: procesar lo que trae y **devolvérselo inmediatamente al driver**.

Ese último paso es fácil de olvidar y catastrófico: si no devuelves los buzones,
te quedas sin ninguno y dejas de recibir para siempre.

### `net_packet_is_valid()`: el filtro

```c
if (packet->payload.magic[0] != 'C'){ return 0; }
...
if (packet->payload.instance_id == local_instance_id){ return 0; }
```

Dos filtros:

**La marca `"CTRE"`.** Todos nuestros paquetes empiezan con esos cuatro bytes.
Lo que llegue a nuestro socket sin esa marca es tráfico de otro programa y se
tira. Es barato y evita sorpresas.

**Nuestro propio `instance_id`.** Si nuestro propio broadcast nos volviera, lo
ignoramos. En Ethernet real una tarjeta no se oye a sí misma, así que esto casi
nunca hace falta, pero comprobarlo no cuesta nada y evitaría un fallo muy
confuso: emparejarte contigo mismo.

### `net_handle_packet()`: archivar

Para un paquete de tipo `INPUT`, guarda cada uno de los frames que trae:

```c
entry = 0;
while (entry < packet->payload.count){
    if (entry >= NET_REDUNDANCY){ break; }
    frame = packet->payload.base_frame + (unsigned long)entry;
    net_store_remote_input(frame, packet->payload.inputs[entry]);
    entry = entry + 1;
}
```

Y `net_store_remote_input()` descarta lo que no sirve:

```c
if (frame < simulation_frame){ return; }                           // ya pasó
if (frame >= simulation_frame + NET_INPUT_BUFFER_SIZE){ return; }  // demasiado adelante
```

- Un frame **ya simulado** no sirve de nada: ya no vamos a volver atrás.
- Un frame **demasiado adelantado** pisaría el hueco de uno que todavía
  necesitamos, por lo del buffer circular.

Casi siempre estará archivando datos repetidos (por la redundancia), y eso está
bien: escribir el mismo valor otra vez no hace daño.

---

## 17. Esperar: `net_has_remote_input()`

```c
int net_has_remote_input(void){
    unsigned int index;
    index = (unsigned int)(simulation_frame & (NET_INPUT_BUFFER_SIZE - 1));

    if (remote_input_valid[index] == 0){ return 0; }
    if (remote_input_frame[index] != simulation_frame){ return 0; }
    return 1;
}
```

"¿Tengo ya las teclas del otro para el frame que voy a simular?"

Mientras esto devuelva 0, **el juego no puede avanzar**. Ese es el "lock" de
"lockstep": las dos máquinas están encadenadas al mismo número de frame.

En el bucle principal se usa así:

```c
while (net_has_remote_input() == 0){
    net_poll();
    sound_update();

    if (net_connection_lost() == 1){
        connection_was_lost = 1;
        break;
    }
}
```

Tres cosas de este bucle:

**`net_poll()` dentro.** Obvio pero fundamental: si no lo llamas, nunca llegará
nada y esperarás para siempre.

**`sound_update()` dentro.** Menos obvio. Si el juego se para aquí más de un
frame, la tarjeta de sonido se queda sin datos nuevos y repite la última mitad
del buffer, que se oye como un tartamudeo. Es exactamente la misma razón por la
que `sound_update()` está fuera del `if` de la pausa de explosión: **el sonido
tiene que seguir corriendo pase lo que pase**.

**La salida de emergencia.** Si la otra máquina se ha muerto, este bucle no
tendría forma de salir. `net_connection_lost()` mira si han pasado 10 segundos
sin recibir nada y corta.

---

## 18. Cerrar el frame

Al final del bucle principal:

```c
if (network_mode == 1){
    net_set_local_checksum(compute_state_checksum());
    net_advance_frame();
}
```

`net_advance_frame()` es una línea: `simulation_frame + 1`. Lo del checksum se
explica en el capítulo 24.

Lo importante es **dónde** está: al final del todo, después de simular y de
dibujar. Las dos máquinas llegan a esta línea habiendo calculado exactamente lo
mismo, y solo entonces avanzan juntas al frame siguiente.

---

# PARTE 5 — CÓMO SE ENCHUFA AL JUEGO

Esta es la parte que importa. Ya sabes cómo funciona la red por dentro; ahora
vamos a ver cómo se conecta con el juego que ya tenías.

## 19. El único cambio de fondo

De todo el juego, **una sola cosa cambió de verdad**. Esta:

```c
// ANTES
void process_player_input(struct player *_player,
                          struct player *_other,
                          unsigned char key_up_code,
                          unsigned char key_down_code,
                          unsigned char key_left_code,
                          unsigned char key_right_code,
                          unsigned char key_fire_code){

    if (keys[key_up_code]){            // <-- lee el teclado directamente
        ...
```

```c
// AHORA
void process_player_input(struct player *_player,
                          struct player *_other,
                          unsigned char input_bits){

    if (input_bits & NET_INPUT_UP){    // <-- lee un byte que le dan
        ...
```

Eso es todo. La función ya no lee el teclado: **le dan las teclas ya
resueltas**.

Y esa diferencia de nada lo cambia todo, porque ahora la función **no puede
saber de dónde vienen esas teclas**. Le da exactamente igual:

```
                      +---------------------------+
   tu teclado ----->  |                           |
                      |  process_player_input()   |  ---> el tanque se mueve
   la red      ----->  |                           |
                      +---------------------------+
                         no sabe cuál de los dos
                         ha sido, y no lo necesita
```

En jerga esto se llama **desacoplar**: la función dependía del teclado y ahora
depende de un byte. Es un cambio pequeño en el código y enorme en lo que
permite.

Para que el modo local siga funcionando se añadió una función que hace lo que
antes hacía la propia `process_player_input()`:

```c
unsigned char read_input_from_keys(unsigned char key_up_code, ...){

    unsigned char input_bits;
    input_bits = 0;

    if (keys[key_up_code]){    input_bits = input_bits | NET_INPUT_UP;    }
    if (keys[key_down_code]){  input_bits = input_bits | NET_INPUT_DOWN;  }
    if (keys[key_left_code]){  input_bits = input_bits | NET_INPUT_LEFT;  }
    if (keys[key_right_code]){ input_bits = input_bits | NET_INPUT_RIGHT; }
    if (keys[key_fire_code]){  input_bits = input_bits | NET_INPUT_FIRE;  }

    return input_bits;
}
```

Es la **única** función del juego que sigue tocando `keys[]`. Todo lo demás
trabaja ya con el byte.

Y el bucle principal se convierte en esto:

```c
if (network_mode == 1){
    ... conseguir los dos bytes: uno mío, otro de la red ...
}else{
    player1_input = read_input_from_keys(KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_NUMPAD_5);
    player2_input = read_input_from_keys(KEY_W,  KEY_S,    KEY_A,    KEY_D,     KEY_G);
}

process_player_input(&player1, &player2, player1_input);
process_player_input(&player2, &player1, player2_input);
```

Las dos últimas líneas son **idénticas** en los dos modos. A partir de ahí, el
juego es el juego de siempre.

---

## 20. El viaje de una tecla, paso a paso

Este capítulo es el corazón del manual. Vamos a seguir **una sola pulsación**
desde que aprietas la tecla hasta que el tanque se mueve **en las dos
pantallas**.

**Escenario:** estás en la máquina A, te ha tocado ser el jugador 1 (el tanque
de abajo), y pulsas la **flecha ARRIBA** justo cuando el juego va por el
**frame 100**.

### Paso 1 — El hardware

Aprietas la tecla. El teclado manda el scan code `0x48` y dispara la
interrupción 9. Tu manejador de siempre:

```c
void interrupt far new_kbd_handler() {
    ...
    keys[scancode] = 1;      // keys[0x48] = 1
    ...
}
```

Hasta aquí **no ha cambiado nada**. Esto es exactamente lo que ya hacía tu
juego.

### Paso 2 — El bucle lee el teclado (frame 100, máquina A)

```c
local_input = read_input_from_keys(KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_NUMPAD_5);
```

`keys[0x48]` vale 1, así que `local_input` sale valiendo **`0x01`**
(`NET_INPUT_UP`).

### Paso 3 — Se archiva PARA EL FUTURO

```c
net_set_local_input(local_input);
```

Y dentro:

```c
target_frame = simulation_frame + NET_INPUT_DELAY;    //  100 + 5 = 105
index = (unsigned int)(target_frame & 63);            //  105 & 63 = 41
local_input_value[index] = input_bits;                //  local_input_value[41] = 0x01
```

> **Fíjate bien: tu tecla NO se aplica ahora.** Se guarda en el hueco del frame
> **105**. Tu tanque no se va a mover todavía.
>
> Esto es lo del capítulo 6. Si se aplicara ya, te desincronizarías.

### Paso 4 — Sale por el cable

```c
net_send_input();
```

Construye un paquete que lleva los frames **98 a 105**:

```
   frame:  98    99   100   101   102   103   104   105
   valor: 0x00  0x00  0x00  0x00  0x00  0x00  0x00  0x01
                                                      ^
                                            tu pulsación va aquí
```

Los otros siete van a cero porque no habías pulsado nada. El paquete completo
son 60 bytes y sale hacia la otra máquina.

### Paso 5 — Llega a la máquina B

Un milisegundo después (por cable), B llama a `net_poll()` en su bucle,
encuentra el paquete en uno de sus cuatro buzones, y lo archiva:

```c
net_store_remote_input(105, 0x01);
```

Que hace:

```c
index = (unsigned int)(105 & 63);         //  41   <-- el MISMO hueco que en A
remote_input_frame[41] = 105;
remote_input_value[41] = 0x01;
remote_input_valid[41] = 1;
```

**Las dos máquinas tienen ahora el mismo byte en el hueco 41.** Una en su array
"local", la otra en su array "remoto". Pero el mismo byte, para el mismo frame.

### Paso 6 — Cinco frames sin que pase nada

Frames 100, 101, 102, 103, 104. En **las dos** máquinas:

```c
player1_input = ... el byte del frame actual ...     // vale 0x00
process_player_input(&player1, &player2, 0x00);      // ninguna tecla: no se mueve
```

Tu tanque está quieto en tu pantalla. Has pulsado y no pasa nada.

**Esos son los 71 ms.** Cinco frames × 14,3 ms. Es el precio, y en el capítulo 6
vimos por qué hay que pagarlo.

### Paso 7 — Frame 105: las dos máquinas, a la vez

Aquí está lo bonito. Las dos llegan al frame 105 y hacen esto:

```
MÁQUINA A (tú, jugador 1)              MÁQUINA B (el otro, jugador 2)
-------------------------              -----------------------------
index = 105 & 63 = 41                  index = 105 & 63 = 41

net_get_local_input()                  net_get_remote_input()
  -> local_input_value[41]               -> remote_input_value[41]
  -> 0x01                                -> 0x01

local_player_is_1 == 1, así que:       local_player_is_1 == 0, así que:
  player1_input = 0x01   <-- mío         player1_input = 0x01   <-- de la red
  player2_input = (suyo)                 player2_input = (suyo)
```

**Las dos han llegado al mismo `player1_input = 0x01`.** Una lo sacó de su
teclado hace cinco frames; la otra lo recibió por el cable. Da igual: es el
mismo byte, en el mismo frame.

### Paso 8 — El juego de siempre

Y ahora, en las dos máquinas, se ejecuta exactamente el mismo código:

```c
process_player_input(&player1, &player2, 0x01);
```

Dentro:

```c
if (input_bits & NET_INPUT_UP){                              // 0x01 & 0x01 -> sí

    is_driving = 1;
    player_update_future_collision_points(_player, MOVE_UP); // ¿dónde caería?

    if (is_move_blocked(_player, _other) == 0){              // ¿hay pared o tanque?
        _player->is_moving = 1;
        move_sprite(_player, MOVE_UP);                       // position_y -= 2
    }
}
```

Y aquí está la clave de todo el diseño:

> **`is_move_blocked()` lee el mismo mapa de colisiones, con el tanque en la
> misma posición, en las dos máquinas. Así que devuelve lo mismo. Y
> `move_sprite()` resta los mismos 2 píxeles.**

Si `position_y` valía 164, en las dos máquinas pasa a valer **162**. No
"aproximadamente 162". **162.**

### El viaje completo, de un vistazo

```
   TÚ                                                    EL OTRO
   ==                                                    =======

   tecla ARRIBA
      |
      v
   INT 9 -> keys[0x48] = 1
      |
      v
   frame 100: read_input_from_keys() -> 0x01
      |
      +--> local_input_value[41] = 0x01      (para el frame 105)
      |
      +--> paquete con los frames 98..105 ------> (1 ms) ---> net_poll()
                                                                 |
                                                                 v
                                                  remote_input_value[41] = 0x01
   frames 100-104: quieto                        frames 100-104: quieto
      |                                                          |
      v                                                          v
   frame 105                                     frame 105
   net_get_local_input() -> 0x01                 net_get_remote_input() -> 0x01
      |                                                          |
      +---------------> player1_input = 0x01 <-------------------+
                                |
                                v
              process_player_input(&player1, &player2, 0x01)
                                |
                                v
                   move_sprite(&player1, MOVE_UP)
                                |
                                v
                    player1.position_y: 164 -> 162
                        EN LAS DOS MÁQUINAS
```

Si has entendido este diagrama, has entendido el juego en red. Todo lo demás es
plomería.

---

## 21. Quién controla qué tanque

Aquí hay una confusión muy fácil de tener, así que vamos despacio.

Hay **dos parejas de conceptos** que no son la misma:

| | |
|---|---|
| **`player1` / `player2`** | Los dos tanques **de la simulación**. Existen igual en las dos máquinas. `player1` es siempre el de abajo y `player2` siempre el de arriba, en los dos ordenadores |
| **local / remoto** | De **qué teclado** viene un byte. "Local" es el que está delante de mí; "remoto" el que llega por el cable |

La pregunta es: ¿qué tanque mueve mi teclado? Y la respuesta la da `is_player1`,
que se decidió en el emparejamiento:

```c
if (local_player_is_1 == 1){
    player1_input = net_get_local_input();     // mi teclado mueve el de abajo
    player2_input = net_get_remote_input();
}else{
    player1_input = net_get_remote_input();
    player2_input = net_get_local_input();     // mi teclado mueve el de arriba
}
```

Dicho en dibujo:

```
        MÁQUINA A (le tocó jugador 1)      MÁQUINA B (le tocó jugador 2)

  mi teclado  ---------> player1            mi teclado  ---------> player2
                                    (el de abajo)                          (el de arriba)

  la red      ---------> player2            la red      ---------> player1


        Las DOS simulan player1 Y player2. Lo único que cambia es cuál
        de los dos alimenta cada máquina con su propio teclado.
```

**El cruce es todo el truco.** Lo que en una máquina es "lo mío" en la otra es
"lo suyo", y viceversa. Pero los dos tanques se simulan enteros en las dos.

### Tres consecuencias prácticas

**1. En red, los dos jugáis con las flechas.**

En el modo local, el jugador 1 va con las flechas y el jugador 2 con WASD,
porque comparten teclado. En red cada uno tiene el suyo entero, así que los dos
usan lo mismo:

```c
local_input = read_input_from_keys(KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_NUMPAD_5);
```

Las teclas WASD **no se leen en modo red**. Da igual qué tanque te haya tocado:
tú siempre usas las flechas y el 5 del numérico.

**2. Puede tocarte el tanque de arriba.**

Se decide al azar (con el `instance_id`), así que no tienes forma de elegir. El
juego te lo dice en la pantalla de texto antes de empezar:

```
You are PLAYER 1, the tank at the bottom.
```

**3. "Servidor" no tiene nada que ver con esto.**

El que ejecuta `launch_game_server.sh` puede acabar siendo el jugador 2
perfectamente. Lo de servidor/cliente es solo para montar el túnel de DOSBox;
una vez conectados, las dos máquinas son idénticas.

---

## 22. Todo lo que NO hubo que tocar

Merece la pena verlo escrito, porque es la recompensa de haber elegido lockstep:

```
is_blocked_by_wall()          update_bullet()           draw_to_buffer()
is_blocked_by_tank()          bullet_has_hit_tank()     draw_explosion()
is_move_blocked()             move_sprite()             update_game()
restart_game()                update_player_animation()

...y TODO players.c, TODO sound.c, TODO bmp.c
```

Ni una línea.

¿Por qué? Porque **esas funciones nunca supieron de dónde venían las teclas**.
Solo movían un tanque, comprobaban una colisión o pintaban un sprite. Y siguen
haciendo exactamente eso.

Comparado con la idea ingenua del capítulo 2, donde había que serializar el
`struct player` entero y acordarse de actualizar el empaquetador cada vez que se
añade un campo, la diferencia es abismal.

> **La moraleja, que sirve para cualquier proyecto:** el trabajo lo hizo la
> arquitectura, no el código de red. `net.c` mueve un byte; lo que hace que
> funcione es que el juego fuera determinista y que la entrada se pudiera
> desacoplar del teclado con un cambio de una línea.

---

## 23. Dos detalles que no son opcionales

Dos cosas del enchufado que parecen menores y no lo son.

### La entrada se intercambia SIEMPRE, también durante la explosión

Mira dónde está el bloque de red en el bucle:

```c
do {
    // 0. LA RED, SIEMPRE
    if (network_mode == 1){
        ... mandar, esperar, repartir ...
    }else{
        ... leer el teclado ...
    }

    // 1. y AHORA los dos estados de la ronda
    if (explosion_pause_counter == 0){
        ... jugando: teclas, balas, colisiones ...
    }else{
        ... ardiendo: solo avanza la explosión ...
    }
    ...
} while(!keys[KEY_ESC]);
```

El bloque de red está **fuera y antes** del `if` de los dos estados. Tiene que
ser así.

Durante el medio segundo que dura la explosión, en pantalla no se mueve nada
salvo el fuego. Pero **el número de frame sigue avanzando**. Si dejáramos de
mandar y recibir durante ese rato, la otra máquina se quedaría esperando una
entrada para un frame que nadie le mandó, y se pararía hasta agotar el tiempo
de espera.

Dicho de otra forma: **el lockstep no se puede pausar**. Aunque el juego esté
"parado", las dos máquinas tienen que seguir pasando por los mismos números de
frame, aunque lo que manden sea "no he pulsado nada".

### El sonido no necesita red (y esto es precioso)

No se manda ni un byte sobre sonido. Ni "he disparado", ni "reproduce la
explosión". Nada.

Y sin embargo suena bien en las dos máquinas, a la vez. ¿Por qué?

Porque **el sonido lo dispara la simulación, y las dos máquinas simulan lo
mismo**:

```c
if (player_fire_bullet(_player) == 1){
    sound_play(_player->sound_fire_voice, SOUND_SAMPLE_FIRE, SOUND_VOLUME_FIRE);
}
```

En el frame 105, las dos máquinas ejecutan `player_fire_bullet()` con el mismo
byte de entrada y el mismo estado. Las dos devuelven 1. Las dos llaman a
`sound_play()`. **En el mismo frame, por su cuenta, sin haberlo hablado.**

Lo mismo con la explosión, con el motor y con el impacto.

Esto es una consecuencia gratuita del determinismo, y es un buen ejemplo de por
qué esta arquitectura es tan cómoda: **cualquier cosa que dependa solo del
estado del juego se sincroniza sola.** Si mañana añades un power-up que hace un
sonido, tampoco habrá que mandar nada.

Lo único que sí hubo que añadir es la llamada a `sound_update()` dentro del
bucle de espera (capítulo 17), y no por sincronizar nada: por no dejar la
tarjeta sin datos mientras el juego está parado.

---

# PARTE 6 — CUANDO ALGO FALLA

## 24. Desincronización: el fallo invisible

Este es **el** fallo característico del lockstep, y hay que entenderlo bien
porque es traicionero.

### Qué es

Las dos máquinas dejan de calcular lo mismo. A partir de ese momento, cada una
sigue jugando **su propia partida**.

### Por qué es tan malo

Porque **no se nota**. Piénsalo: cada pantalla sigue mostrando una partida
perfectamente coherente. Los tanques se mueven bien, las balas vuelan bien, las
colisiones funcionan. Simplemente **son partidas distintas**.

Tú ves que le has dado. Él ve que has fallado. Y no hay ningún mensaje de error,
ningún cuelgue, nada. Puedes perseguir eso durante días sin saber ni por dónde
empezar.

### Qué lo causaría

Cualquier cosa que rompa el determinismo del capítulo 4:

- meter un `float` en la simulación
- llamar a `rand()`
- que algo dependa del reloj o de la velocidad de la máquina
- leer memoria sin inicializar
- **aplicar una entrada en frames distintos** en cada máquina (el error del
  capítulo 6)
- un fallo en el buffer circular que confunda un frame viejo con uno nuevo

### Cómo lo detectamos

Cada 30 frames, cada máquina calcula un número que resume **todo** su estado:

```c
unsigned int compute_state_checksum(){

    unsigned int checksum;
    checksum = 0;

    checksum = checksum + (player1.position_x * 3);
    checksum = checksum + (player1.position_y * 5);
    checksum = checksum + (player1.current_direction * 7);
    checksum = checksum + (player1.bullet_position_x * 11);
    ...
    checksum = checksum + (explosion_pause_counter * 71);

    return checksum;
}
```

Se llama **checksum** o suma de comprobación: un número corto que representa un
montón de datos. Si dos checksums son distintos, los datos son distintos, seguro.

Ese número viaja en el siguiente paquete, junto al frame al que pertenece. La
otra máquina lo compara con el suyo del mismo frame, y si no coinciden:

```
NET DESYNC at frame 1830: mine 41234 theirs 41199
```

**Y ahí tienes el frame exacto en el que se rompió.** Eso convierte un misterio
de días en un problema acotado.

### Los detalles del checksum

**¿Por qué los multiplicadores 3, 5, 7, 11...?** Para que no se puedan cancelar
cosas. Si sumáramos los valores a pelo, intercambiar las X de los dos tanques
daría **el mismo total** y no detectaríamos el fallo. Con multiplicadores
distintos, cada campo aporta de forma distinta.

**¿No se desborda?** Sí, `unsigned int` se da la vuelta constantemente. **Y da
igual**: se da la vuelta exactamente igual en las dos máquinas, porque el
desbordamiento de `unsigned` está definido en C y es determinista. Lo único que
necesitamos es que dos estados distintos den números distintos casi siempre.

**¿Por qué está en `main.c` y no en `net.c`?** Porque `net.c` no sabe qué es un
tanque, y no debe saberlo. `main.c` calcula el número y se lo entrega.

**¿Por qué se guardan 64?** Porque el checksum viaja en un paquete que puede
tardar, y cuando llega hay que compararlo con **el nuestro de ese mismo frame**,
no con el actual. Se guardan los últimos 64 en un anillo, igual que las entradas.

---

## 25. Desconexión

El otro fallo posible: que la otra máquina desaparezca. Se cierra el DOSBox, se
cae la wifi, se desenchufa el cable.

Sin protección, el bucle del capítulo 17 esperaría **para siempre**, con el
juego congelado y sin explicación.

```c
int net_connection_lost(void){

    long now_tick;
    ...
    now_tick = biostime(0, 0L);

    if (now_tick - last_packet_tick > NET_TIMEOUT_SECONDS * NET_TICKS_PER_SECOND){
        sprintf(net_log_text, "NET: connection lost at frame %lu", simulation_frame);
        tanks_log(net_log_text);
        connection_lost = 1;
        return 1;
    }

    return 0;
}
```

`last_packet_tick` se actualiza cada vez que llega **cualquier** paquete válido.
Si pasan 10 segundos sin ninguno, se da por muerta la conexión y el juego sale
ordenadamente: vuelve a modo texto y te dice qué ha pasado.

Diez segundos es mucho a propósito. Un tirón de red de dos o tres segundos es
molesto pero recuperable; no queremos cortar una partida por eso.

---

## 26. Qué mirar en el log

Todo esto va a `k:\game.log`, porque en modo gráfico no se puede imprimir nada.

| Línea | Qué significa |
|---|---|
| `NET sizes: ecb=42 header=30 packet=60` | Correcto. **Cualquier otro número** = el compilador ha rellenado las estructuras e IPX lee todo mal |
| `NET: no IPX driver found` | No hay driver. En DOSBox falta `ipx=true` |
| `NET: IPX driver entry at F000:1680` | El driver está y sabemos dónde llamarle |
| `NET: node 000000000000` | **Mala señal.** Nodo todo ceros = ningún túnel nos ha dado dirección: no estamos conectados |
| `NET: node 7F000001AF6E id 304509028` | Bien. Ahí está nuestra dirección y nuestro número al azar |
| `NET: paired. them id ... we are player 1` | Emparejados, y qué tanque nos tocó |
| `NET: discovery timed out` | Nadie contestó en 30 segundos |
| `NET DESYNC at frame N` | Capítulo 24 |
| `NET: connection lost at frame N` | Capítulo 25 |
| `NET: sent 21007, received 20984, waited 11403 frames` | Resumen al salir |

### Cómo leer el resumen final

Es la línea más útil de todas.

**`sent` / `received`.** Una diferencia pequeña (menos del 1%) es normal: son
los que quedaron en el aire al salir, más algún perdido. La redundancia los
tapa. Si `received` fuera **0**, no está llegando nada: mira lo del nodo a ceros.

**`waited`.** Los frames que el juego pasó **parado** esperando a la otra
máquina. Tiene **dos causas distintas** y confundirlas te hace tocar el
parámetro equivocado:

| Lo que ves | Qué es | Qué hacer |
|---|---|---|
| Cerca de 0 | Todo va sobrado | Nada |
| Unas decenas, a rachas | **Jitter** de la red: los paquetes llegan a tiempos irregulares | Subir `NET_INPUT_DELAY` |
| Una fracción grande y constante (la mitad, por ejemplo) | Las dos máquinas **no van a la misma velocidad**, y la rápida se frena al ritmo de la lenta | Bajar `cycles` en **las dos** |

El segundo caso es importante y poco intuitivo: **subir `NET_INPUT_DELAY` no lo
arregla**. El retardo es un colchón que absorbe *variación*; no puede absorber
una diferencia permanente de velocidad, igual que un depósito no arregla un
grifo que da menos caudal del que consumes. Solo añadiría latencia.

Para saber cuál es la lenta, compara el `waited` de los dos logs: **la lenta lo
tendrá cerca de cero**, porque nunca tiene que esperar a nadie.

---

# PARTE 7 — APRENDER TOCANDO

## 27. Experimentos

La mejor forma de entender esto es romperlo a propósito. Todos estos son
seguros: se deshacen cambiando el número de vuelta.

### Experimento 1: ver el retardo de entrada

En `header/net.h`, pon:

```c
#define NET_INPUT_DELAY 	30
```

Recompila y juega. **Medio segundo entre que pulsas y el tanque se mueve.** Es
grotesco, pero te hace *sentir* qué es el retardo de entrada y por qué 5 es un
número elegido y no arbitrario.

Ahora prueba `1`. Va como un guante... hasta que la red tenga un mal momento y
el juego pegue un tirón. Sube `waited` en el log.

### Experimento 2: romper el determinismo a propósito

**Este es el más instructivo de todos.** En `move_sprite()`, mete esto:

```c
if (rand() % 200 == 0){
    _player->position_x = _player->position_x + 1;
}
```

Un píxel de más, muy de vez en cuando, al azar.

Juega en red y mira el log. En pocos segundos:

```
NET DESYNC at frame 312: mine 28471 theirs 28399
```

Las dos máquinas sacan números distintos de `rand()`, así que un tanque se
desplaza un píxel en una máquina y no en la otra. **Y a partir de ahí las dos
partidas divergen sin remedio.**

Lo interesante: mira las dos pantallas mientras pasa. **Las dos siguen pareciendo
correctas.** Nada se rompe, nada peta. Solo son partidas distintas. Eso es
exactamente lo que el capítulo 24 quería que entendieras, y verlo con tus ojos
vale más que leerlo.

Luego quita el `rand()` y comprueba que el `DESYNC` desaparece.

### Experimento 3: quitar la redundancia

```c
#define NET_REDUNDANCY 		1
```

Ahora cada paquete lleva solo el frame actual, sin repuesto. En cable no notarás
nada (casi no se pierde nada). Por wifi, verás tirones y `waited` subirá
bastante. Es la demostración de por qué la redundancia estaba bien invertida.

### Experimento 4: cambiar el socket en una sola máquina

En `net.c`, en una de las dos:

```c
#define NET_SOCKET_NUMBER 		0x869D      // una unidad más
```

Las dos arrancan bien, el túnel se monta bien, `ipxnet ping` funciona... y **no
se encuentran nunca**. `NET: discovery timed out`.

Sirve para entender qué es un socket: el driver entrega cada paquete solo a
quien esté escuchando en ese número exacto. Es el equivalente de un puerto.

### Experimento 5: ver el lockstep con tus propios ojos

Arranca las dos instancias y **para una en seco**: en DOSBox, pulsa `Ctrl+F11`
varias veces para bajarle los ciclos a casi nada.

Verás que **la otra también se ralentiza**. No se adelanta, no sigue por su
cuenta: espera. Eso es literalmente el "lock" de lockstep, y verlo pasar en
directo lo explica mejor que cualquier párrafo.

Con `Ctrl+F12` los subes otra vez y las dos recuperan.

### Experimento 6: mirar el log en directo

En otra terminal, mientras juegas:

```
tail -f net-test/log-server/GAME.LOG | grep --line-buffered "NET\|Tank hit"
```

Verás el emparejamiento y los impactos según ocurren.

---

## 28. Glosario

Términos que aparecen en el manual y en el código, en el orden en que hacen
falta.

**Paquete** — Un bloque de bytes que se manda de una vez por la red. El nuestro
son 60 bytes: 30 de cabecera IPX y 30 nuestros.

**Protocolo** — El acuerdo sobre qué significan esos bytes. IPX es un protocolo;
"los primeros cuatro bytes son `CTRE`" es parte del nuestro.

**Datagrama** — Un paquete suelto, sin conexión ni garantías. Se manda y ya. Lo
contrario sería un flujo (como TCP), donde el protocolo se encarga de que todo
llegue y en orden.

**Nodo** — La dirección de una máquina en IPX: 6 bytes, que son la MAC de la
tarjeta.

**MAC** — El número único que cada tarjeta de red trae grabado de fábrica.

**Broadcast** — Mandar a todas las máquinas de la red a la vez, usando la
dirección `FF:FF:FF:FF:FF:FF`.

**Socket** — Un número que identifica una conversación dentro de una máquina. El
equivalente del puerto en TCP/IP. Nosotros usamos el `0x869C`.

**ECB** (*Event Control Block*) — La estructura que se le rellena al driver IPX
para pedirle que mande o reciba. El "formulario" del capítulo 10.

**TSR** (*Terminate and Stay Resident*) — Un programa de DOS que se queda en
memoria después de terminar, para que otros lo llamen. El driver IPX es uno.

**Latencia** — Lo que tarda un paquete en llegar. Por cable, décimas de
milisegundo; por wifi, unos milisegundos.

**Jitter** — La *variación* de la latencia. Es peor que la latencia en sí: una
latencia alta pero constante se compensa con el retardo de entrada; una latencia
que salta de 2 ms a 90 ms no.

**Determinista** — Que con las mismas entradas produce siempre exactamente el
mismo resultado. La propiedad de tu juego que hace posible todo esto.

**Lockstep** — La arquitectura de este juego: las dos máquinas simulan todo y
solo intercambian entradas, avanzando frame a frame juntas.

**Retardo de entrada** (*input delay*) — Los frames que se retienen las teclas
propias antes de aplicarlas, para que se apliquen a la vez que las del otro.
Aquí, 5.

**Redundancia** — Mandar los últimos N frames de teclas en cada paquete, para
que una pérdida no pare el juego. Aquí, 8.

**Buffer circular** (*anillo*) — Un array que se reutiliza dando la vuelta,
usando `frame & 63` como índice. Guarda los últimos 64 frames.

**Sondeo** (*polling*) — Preguntar de vez en cuando "¿hay algo?", en vez de que
te avisen con una interrupción. Es lo que hace `net_poll()`, y es a propósito.

**Checksum** — Un número corto que resume muchos datos. Si dos checksums son
distintos, los datos son distintos.

**Desincronización** (*desync*) — Cuando las dos máquinas dejan de calcular lo
mismo. El fallo característico del lockstep.

**Big endian / little endian** — En qué orden se guardan los bytes de un número.
IPX usa big endian (byte gordo primero), el 8086 little endian. De ahí
`net_swap16()`.

---

## Y ya está

Si has llegado hasta aquí, ya sabes:

- por qué mandar posiciones es mala idea (cap. 2)
- por qué tu juego puede hacer lockstep y muchos no pueden (cap. 4)
- por qué tus propias teclas van con retraso (cap. 6)
- por qué la redundancia sale gratis (cap. 7)
- qué es IPX y por qué no TCP/IP (cap. 9)
- cómo se le habla al driver (cap. 10-11)
- qué hace cada función y en qué orden (cap. 12-18)
- **cómo viaja una tecla desde tu dedo hasta las dos pantallas (cap. 20)**
- cómo se detecta y se diagnostica lo que salga mal (cap. 24-26)

Para consultar cosas sueltas, [`NETWORK.md`](NETWORK.md) tiene la referencia de
todas las funciones con su número de línea. Para montar las dos máquinas,
[`NETWORK-TESTING.md`](NETWORK-TESTING.md).
