# CutreGameMSDOS

![Portada](portada.png)

A two player tank game for MS-DOS, written in Borland Turbo C++ 3.0.

*Un juego de tanques para dos jugadores en MS-DOS, escrito en Borland Turbo C++ 3.0.*

---

## English

This is a small game, made in Turbo C++ 3.0 for MS-DOS. To build it you need
the MS-DOS version of NASM installed in `c:\nasm` and added to the system
PATH. This code was written on a real Pentium III running Windows 98.

This game is a port of another game I made for the Commodore 64 in assembly.
That one I did 100% with my own hands, and here is the repository:

**[Commodore 64 version](https://github.com/tsw1985/CutreGameC64)**

I leaned on a YouTube course I had taken beforehand.

For this version I reused a small program I wrote years ago to draw a BMP file
into video memory. With AI I adapted it as I needed changes, but the base was
made by me.

As for the game mechanics, I also built a solid base myself, and then used AI
to help me move the development forward, but it is the same logic I applied
for the Commodore 64 version.

As for the sound, here I did lean 100% on AI to develop it. I had the concepts
of DMA and so on in my head, but I admit it is very technical and complex for
me to have developed on my own. It would have taken me a long time. This has
been a hobby. In this project I wanted to use AI to learn, and to have it
teach me all these technical things that on my own would have taken me a very
long time to work out and understand. With AI I have that teacher on hand 24
hours a day.

I hope you like the game.

### Controls

| | Player 1 | Player 2 |
|---|---|---|
| Move | Arrow keys | W / A / S / D |
| Fire | Keypad 5 | G |
| Quit | Esc | |

### Building

```
make
```

Requires Borland Turbo C++ 3.0 (`tcc`) and NASM for MS-DOS in `c:\nasm`, both
on the PATH. The executable is built into `bin\game.exe` and must be run from
that directory, since it looks for its resources in `..\res\`.

Sound is optional: if no Sound Blaster is found the game runs exactly the
same, in silence. It reads the `BLASTER` environment variable to locate the
card, and falls back to A220 I5 D1.

### The three modes

| Mode | Command line | What it is |
|---|---|---|
| **local** | `game.exe` | Two players on one keyboard, one screen, the 320x200 map |
| **net** | `game.exe /net` | Two machines, the same 320x200 map, one tank each |
| **supernet** | `game.exe /net /bigmap` | Two machines on the 640x400 map, each with its own scrolling camera |

**One script, `play.sh`**, and the mode is the first word. No path ever needs
editing by hand:

```bash
./play.sh local              # local

./play.sh both               # net,      both windows on THIS machine
./play.sh both -b            # supernet, both windows on THIS machine

./play.sh server             # net,      two real machines
./play.sh client <ip>

./play.sh server -b          # supernet, two real machines
./play.sh client <ip> -b
```

Every command, case by case, is in [`COMANDOS.md`](COMANDOS.md). And
`./play.sh` on its own prints the help.

`play.sh both` is the one to use for testing: it starts two DOSBox
windows side by side, joined over the loopback, so you get both tanks without
a second machine and without typing an IP anywhere.

#### Big map themes

**supernet** can be dressed four ways:

```bash
./play.sh both -b            # the original
./play.sh both -b -t sky     # map_sky.bmp  + spr_sky.bmp
./play.sh both -b -t war     # map_war.bmp  + spr_war.bmp
./play.sh both -b -t neon    # map_neon.bmp + spr_neon.bmp
```

Or in the game directly: `game.exe /net /bigmap -sky`.

A theme changes **the map picture and the sprite sheet, and nothing else**. The
walls always come from `bigcol.bmp`, whatever the theme.

Which means **the two machines do not have to use the same theme**: one on
`-neon` and one on `-war` play exactly the same match and stay in sync, they
just see two different paint jobs of it. The map size does have to match, and
the checksum takes care of that.

You can see it for yourself on one machine:

```bash
./play.sh both -b -t war -T neon
```

`-T` sets a different theme **for the client window only**. Both tanks move
identically, frame for frame, looking completely different.

#### The 15 levels

Each theme ships **five different geometries**, under `res/15Level/`:

```bash
./play.sh both -b -t neon -l 3     # NEON, level 3
./play.sh server -b -t war -l 5    # MILITAR, level 5
./play.sh both -b                  # the original map
```

Or in the game: `game.exe /net /bigmap -neon -level3`.

| | |
|---|---|
| `-l 1` Open field | A wide arena with scattered cover |
| `-l 2` Crossfire | Four sectors with wide doorways |
| `-l 3` Ambush | Nine rooms and circular routes |
| `-l 4` Encirclement | Twelve sectors, bottlenecks |
| `-l 5` The mousetrap | Twenty cells wired into a circuit |

The levels only come dressed, so `-l 3` with no theme uses `-sky` and says so.

**And here is the difference from the theme:** a level **does** change the walls.
If the two machines loaded different ones, the tanks would walk through each
other's walls.

**Pass the same `-l` on both machines.** If they differ the game negotiates it
rather than desyncing, but **player 1 wins** — and player 1 is whoever drew the
lower random id at startup, **not whoever started the server**. Passing it on one
side only is a coin flip.

If they cannot agree at all, the game **refuses to start** rather than beginning
a broken match.

| | Must it match? | Who settles it |
|---|---|---|
| **Theme** | No | Nobody: each machine sees its own |
| **Level** | **Yes** | Pass the same on both. Otherwise, a coin flip |

**In net and supernet, both machines have to be started the same way.** One on
`/bigmap` and the other not is two different maps, with the walls in different
places, and the two simulations come apart. The game catches it (the size of
the map is part of the state checksum, so it reports a desync rather than
going quietly wrong) but catching it is not the same as not doing it.

**supernet is the only mode with a camera**, and that is not an oversight. A
camera can only follow one tank. Over the network that is exactly right, each
machine follows its own and hunting for the other one is the game. On one
keyboard it would leave the second player driving blind, so `/bigmap` without
`/net` is refused and the game falls back to the normal map.

#### The proximity radar

The camera solved one problem and created another: in a world four screens wide
the other tank is almost always off screen, and finding him was wandering at
random. So supernet shows a number at the bottom of the screen, **000% to
100%**, telling you how close he is.

It is drawn with **sprites**, not text — in mode 13h there is no text — cut from
`res/Numbers/<THEME>/numbers.bmp`. It only appears **with a theme**: those
figures are painted in their theme's palette, and against the undressed
`big.bmp` 254 of the 256 palette entries differ, so they would come out in
random colours.

The distance is worked out with integers only, `bigger + smaller/2`, an
approximation of the real one that lands within 11% and needs no square root
and no floating point. It is **decoration**: both machines work out the same
number from positions they already both simulate, nothing crosses the wire, and
it never enters the state checksum. Same rule as the camera.

#### The intro

```bash
./play.sh local -demo
game.exe /demo
```

Fifteen pictures from `res/demo/`, **each one through a different 90s screen
effect** — rotozoom, wobble, ripple, palette cycling, venetian blinds and six
more — eight seconds each, with the music playing. **ESC** skips it and the
match begins.

The whole thing lives in `demos/`, one effect per folder, and it borrows exactly
eleven functions from `bmp.c` and `sound.c`: it includes no header of the game,
so the folder can be lifted into another project as it is.

Over a network **the server shows it and the client waits**, and that is
settled by the role on the command line — `play.sh` passes `/server` or
`/client`, because it is the only thing that knows which end it is launching.

It is deliberately not negotiated between the two machines. The version that
was broke in a way worth remembering: with both ends carrying `/demo` the tie
went to player 1, and player 1 comes from comparing two ids seeded with the
BIOS tick. Two machines started within an eighteenth of a second of each other
draw the *same* id, `lower than` is false on both, both believe they are player
2 — and both sat waiting for an intro nobody was showing.

While it runs, the machine playing it sends a heartbeat every two seconds,
because ten seconds of silence is all it takes for the other end to decide the
connection is gone. And **ESC skips it from either keyboard** — the person
looking at a "waiting" message is the one most likely to want out. Both
machines then leave at the same moment, which matters because the music is
streamed and nobody ever resynchronises it.

**Without `/demo` it costs nothing**: not a byte of memory, not a file opened.
With it, everything is reserved when it runs and handed back before the game
starts — which it has to be, because the map needs 256000 *contiguous* bytes and
a block freed in the middle of the heap leaves a hole that a request that size
cannot use.

Pictures must be **320x200, 256 colours, 65078 bytes exactly**. See
[`COMANDOS.md`](COMANDOS.md) for why the byte count matters.

### Network play

Two machines can play each other over IPX. One acts as the **server** and the
other as the **client**, but that is only about bringing up the connection:
**both machines play exactly the same game.**

Each machine needs its own copy of `bin\` and `res\`. The project can live in
a different folder on each one.

#### Step 1 — on the server machine

Start this one **first**:

```
./play.sh server
```

It prints the exact line to run on the other machine, with the IP already
filled in:

```
  SERVER ready. On the OTHER machine run:

      ./play.sh client 192.168.1.45 -p 5213
```

If it warns you about the firewall, open the port:

```
sudo ufw allow 5213/udp
```

#### Step 2 — on the client machine

Copy the line the server printed:

```
./play.sh client 192.168.1.45
```

Both games show a text screen, find each other, say which tank you got, and
start.

There is **nothing to configure and no path to edit**: the scripts work out
where the game is from where they themselves are, and generate the DOSBox
`.conf` on every launch. They also check DOSBox is installed, that `bin/` and
`res/` are there, that the port is free, and the client pings the server first.

#### Controls over the network

**Both players use the cursor keys and keypad 5**, whichever tank they got.
The WASD keys are not used in network mode.

Which tank you get is decided at random when the two copies pair up, so the
machine running the server may well end up as player 2. The game tells you
before the round starts.

#### If they do not connect

| What you see | What it is |
|---|---|
| `Timeout connecting to server` | The server is not running yet, or its firewall is blocking `UDP 5213` |
| `NET: discovery timed out` in the log | The tunnel is up but the games did not find each other |
| `NET: node 000000000000` in the log | This machine is not joined to any tunnel |

The log for each machine is at `runserv/GAME.LOG` and `runcli/GAME.LOG`
(`runlocal/GAME.LOG` in local mode), on that machine: the game writes it with a
RELATIVE path, so it lands in the directory `play.sh` ran it from. Leave the game with **ESC**
(not by closing the window) so the summary line gets written.

#### On real DOS, and by hand

The same `game.exe`, started as `game.exe /net`. On real hardware you have to
load `LSL`, your card's ODI driver and `IPXODI` first, and both machines must
use the same frame type in `NET.CFG`.

In DOSBox without the scripts: put `ipx=true` in `dosbox.conf`, then
`ipxnet startserver 5213` on one machine and `ipxnet connect <its ip> 5213` on
the other. On Linux the port must be above 1024: DOSBox's default of 213 is
privileged and will not open.

#### How it works

No positions are ever sent. Both machines run the whole game, both tanks
included, and the only thing that travels is **one byte of pressed keys per
player per frame**. That works because the game is deterministic: same inputs,
same pixels.

- [`doc/EN/NETWORK-MANUAL.md`](doc/EN/NETWORK-MANUAL.md) — the manual, from
  nothing to networked play. Start here if you want to understand it.
- [`doc/EN/NETWORK.md`](doc/EN/NETWORK.md) — the reference.

---

## Español

Este es un pequeño juego, hecho en Turbo C++ 3.0 para MSDOS. Para poder
compilarlo necesitas tener el NASM versión MSDOS en la ruta `c:\nasm` y
añadirlo en el path del sistema. Este código ha sido hecho usando un equipo
Pentium 3 con Windows 98 real.

Este juego es un port de otro juego que he hecho para Commodore 64 en
ensamblador. Ese sí lo hice yo 100% con mis manos, aquí tienes el repositorio:

**[Versión de Commodore 64](https://github.com/tsw1985/CutreGameC64)**

Apoyándome en un curso que hice en YouTube previamente.

Para esta versión, reusé un pequeño programa que hice hace años para dibujar
en pantalla en memoria de vídeo un fichero BMP. Con la IA lo adapté, según iba
necesitando cambios, pero la base ha sido hecha por mí.

Respecto a la mecánica del juego, también yo hice una buena base, luego con la
IA me ayudé para avanzar el desarrollo, pero es la misma lógica que apliqué
para la versión de Commodore 64.

Respecto al sonido, aquí sí me he apoyado 100% en la IA para desarrollar.
Tenía conceptos en mi cabeza de lo que es DMA, etc., pero reconozco que es muy
técnico y complejo para haberlo desarrollado yo. Hubiera pasado mucho tiempo.
Esto ha sido un hobby. En este proyecto he querido usar la IA para aprender y
que me enseñara todas estas cosas técnicas que por mí mismo hubiera tardado
muchísimo en resolver y aprender. Con la IA tengo ese profesor a mano 24
horas.

Espero que te guste el juego.

### Controles

| | Jugador 1 | Jugador 2 |
|---|---|---|
| Mover | Flechas | W / A / S / D |
| Disparar | 5 del teclado numérico | G |
| Salir | Esc | |

### Compilación

```
make
```

Necesita Borland Turbo C++ 3.0 (`tcc`) y NASM para MSDOS en `c:\nasm`, los dos
en el PATH. El ejecutable se genera en `bin\game.exe` y hay que ejecutarlo
desde ahí, porque busca los recursos en `..\res\`.

El sonido es opcional: si no encuentra una Sound Blaster el juego funciona
exactamente igual, en silencio. Lee la variable de entorno `BLASTER` para
localizar la tarjeta, y si no está asume A220 I5 D1.

### Los tres modos

| Modo | Línea de órdenes | Qué es |
|---|---|---|
| **local** | `game.exe` | Dos jugadores en el mismo teclado, una pantalla, el mapa de 320x200 |
| **net** | `game.exe /net` | Dos máquinas, el mismo mapa de 320x200, un tanque cada una |
| **supernet** | `game.exe /net /bigmap` | Dos máquinas en el mapa de 640x400, cada una con su cámara |

**Un solo script, `play.sh`**, y el modo es la primera palabra. Ninguna ruta
hay que tocarla a mano:

```bash
./play.sh local              # local

./play.sh both               # net,      las dos ventanas en ESTA máquina
./play.sh both -b            # supernet, las dos ventanas en ESTA máquina

./play.sh server             # net,      dos máquinas de verdad
./play.sh client <ip>

./play.sh server -b          # supernet, dos máquinas de verdad
./play.sh client <ip> -b
```

Todos los comandos, caso a caso, en [`COMANDOS.md`](COMANDOS.md). Y
`./play.sh` a secas te imprime la ayuda.

`play.sh both` es el que conviene para probar: levanta dos ventanas de
DOSBox una al lado de la otra, unidas por el loopback, así tienes los dos
tanques sin segunda máquina y sin escribir ninguna IP.

#### Los temas del mapa grande

**supernet** se puede vestir de cuatro formas:

```bash
./play.sh both -b            # el original
./play.sh both -b -t sky     # map_sky.bmp  + spr_sky.bmp
./play.sh both -b -t war     # map_war.bmp  + spr_war.bmp
./play.sh both -b -t neon    # map_neon.bmp + spr_neon.bmp
```

O en el juego directamente: `game.exe /net /bigmap -sky`.

Un tema cambia **el dibujo del mapa y la hoja de sprites, y nada más**. Los
muros salen siempre de `bigcol.bmp`, sea cual sea el tema.

Lo que significa que **las dos máquinas no tienen que ir con el mismo tema**:
una con `-neon` y otra con `-war` juegan exactamente la misma partida y siguen
sincronizadas, solo que ven dos pinturas distintas de ella. El tamaño del mapa
sí tiene que coincidir, y de eso se encarga el checksum.

Puedes verlo tú mismo en una sola máquina:

```bash
./play.sh both -b -t war -T neon
```

`-T` pone un tema distinto **solo en la ventana del cliente**. Los dos tanques
se mueven igual, frame a frame, con dos aspectos distintos.

#### Los 15 niveles

Cada tema trae **cinco geometrías** distintas, en `res/15Level/`:

```bash
./play.sh both -b -t neon -l 3     # NEON, nivel 3
./play.sh server -b -t war -l 5    # MILITAR, nivel 5
./play.sh both -b                  # el mapa original
```

O en el juego: `game.exe /net /bigmap -neon -level3`.

| | |
|---|---|
| `-l 1` Campo abierto | Arena amplia, coberturas separadas |
| `-l 2` Cruce de fuego | Cuatro sectores con puertas amplias |
| `-l 3` Emboscada | Nueve salas y rutas circulares |
| `-l 4` Cerco | Doce sectores, cuellos de botella |
| `-l 5` La ratonera | Veinte celdas en circuito |

Los niveles solo existen vestidos, así que `-l 3` sin tema usa `-sky` y te lo
dice.

**Y aquí está la diferencia con el tema:** un nivel **sí** cambia los muros. Si
las dos máquinas cargaran niveles distintos, los tanques atravesarían las
paredes del otro.

**Pon el mismo `-l` en las dos máquinas.** Si difieren el juego lo negocia en
lugar de desincronizarse, pero **gana el jugador 1** — y el jugador 1 es quien
sacó el número aleatorio más bajo al arrancar, **no quien lanzó el servidor**.
Pasarlo en un solo lado es echarlo a suertes.

Si no consiguen acordarlo, el juego **no arranca** en lugar de empezar una
partida rota.

Todos los comandos, caso a caso, en [`COMANDOS.md`](COMANDOS.md).

| | ¿Tiene que coincidir? | Quién lo resuelve |
|---|---|---|
| **Tema** | No | Nadie: cada máquina ve lo suyo |
| **Nivel** | **Sí** | Ponlo igual en las dos. Si no, sorteo |

**En net y supernet las dos máquinas tienen que arrancarse igual.** Una con
`/bigmap` y la otra sin él son dos mapas distintos, con los muros en sitios
distintos, y las dos simulaciones se separan. El juego lo detecta (el tamaño
del mapa entra en el checksum de estado, así que avisa de desincronización en
vez de volverse loco en silencio), pero detectarlo no es lo mismo que no
hacerlo.

**supernet es el único modo con cámara**, y no es un olvido. Una cámara solo
puede seguir a un tanque. En red eso es justo lo correcto: cada máquina sigue
al suyo, y buscar al otro es el juego. En un solo teclado dejaría al segundo
jugador conduciendo a ciegas, así que `/bigmap` sin `/net` se rechaza y el
juego arranca en el mapa normal.

#### El radar de cercanía

La cámara resolvió un problema y creó otro: en un mundo de cuatro pantallas el
otro tanque está casi siempre fuera de la vista, y encontrarlo era dar vueltas
al azar. Así que supernet enseña un número abajo, de **000% a 100%**, que dice
cómo de cerca está.

Está dibujado con **sprites**, no con texto — en el modo 13h no hay texto —,
recortados de `res/Numbers/<TEMA>/numbers.bmp`. Solo aparece **con tema**: esas
cifras están pintadas con la paleta de su tema, y contra el `big.bmp` sin vestir
difieren 254 de las 256 entradas, así que saldrían de colores al azar.

La distancia se calcula solo con enteros, `mayor + menor/2`, una aproximación de
la de verdad que se queda a un 11% y no necesita raíz cuadrada ni coma flotante.
Es **decoración**: las dos máquinas sacan el mismo número de unas posiciones que
ya simulan las dos, no cruza nada por el cable, y no entra en el checksum de
estado. La misma regla que la cámara.

#### La intro

```bash
./play.sh local -demo
game.exe /demo
```

Quince imágenes de `res/demo/`, **cada una con un efecto de pantalla noventero
distinto** — rotozoom, ondulado, ondas, ciclado de paleta, persiana veneciana y
seis más —, ocho segundos cada una y con la música sonando. **ESC** se la salta
y empieza la partida.

Todo vive en `demos/`, un efecto por carpeta, y toma prestadas exactamente once
funciones de `bmp.c` y `sound.c`: no incluye ni un header del juego, así que la
carpeta se lleva a otro proyecto tal cual.

En red **la pone el servidor y el cliente espera**, y lo decide el rol de la
línea de órdenes: `play.sh` pasa `/server` o `/client`, porque es lo único que
sabe qué extremo está lanzando.

No lo negocian las dos máquinas, y a propósito. La versión que sí lo hacía se
rompía de una forma que merece la pena recordar: con `/demo` en las dos, el
desempate era el jugador 1, y el jugador 1 sale de comparar dos ids sembrados
con el tick de la BIOS. Dos máquinas arrancadas con menos de 1/18 de segundo de
diferencia sacan el *mismo* id, `menor que` es falso en las dos, las dos se
creen el jugador 2 — y las dos se quedaban esperando una intro que no ponía
nadie.

Mientras corre, la que la reproduce manda un latido cada dos segundos, porque
diez segundos de silencio bastan para que la otra dé la conexión por perdida.
Y **ESC la salta desde cualquiera de los dos teclados** — quien está mirando un
*"esperando"* es justo el que más ganas tiene. Luego las dos salen en el mismo
instante, que importa porque la música va en streaming y nadie la sincroniza.

**Sin `/demo` no cuesta nada**: ni un byte de memoria, ni un fichero abierto. Con
él, todo se reserva al ejecutarse y se devuelve antes de que arranque el juego
— y tiene que ser así, porque el mapa necesita 256.000 bytes *contiguos* y un
bloque liberado en mitad del montón deja un agujero que una petición de ese
tamaño no puede usar.

Las imágenes tienen que ser **320x200, 256 colores y 65.078 bytes exactos**. En
[`COMANDOS.md`](COMANDOS.md) está por qué importa el número de bytes.

### Juego en red

Dos máquinas pueden jugar entre ellas por IPX. Una hace de **servidor** y la
otra de **cliente**, pero eso es solo para levantar la conexión: **las dos
máquinas juegan exactamente igual.**

Cada máquina necesita su copia de `bin\` y `res\`. El proyecto puede estar en
carpetas distintas en cada una.

#### Paso 1 — en la máquina servidor

Arranca esta **primero**:

```
./play.sh server
```

Te imprime la línea exacta que hay que ejecutar en la otra máquina, con la IP
ya puesta:

```
  SERVER ready. On the OTHER machine run:

      ./play.sh client 192.168.1.45 -p 5213
```

Si te avisa del cortafuegos, abre el puerto:

```
sudo ufw allow 5213/udp
```

#### Paso 2 — en la máquina cliente

Copia la línea que te dio el servidor:

```
./play.sh client 192.168.1.45
```

Los dos juegos muestran una pantalla de texto, se encuentran, te dicen qué
tanque te ha tocado, y empiezan.

**No hay nada que configurar ni ninguna ruta que editar**: los scripts
averiguan solos dónde está el juego a partir de dónde están ellos mismos, y
generan el `.conf` de DOSBox en cada arranque. Además comprueban que DOSBox
está instalado, que existen `bin/` y `res/`, que el puerto está libre, y el
cliente hace ping al servidor antes de nada.

#### Controles en red

**Los dos jugadores usan las flechas y el 5 del teclado numérico**, dé igual
qué tanque les haya tocado. Las teclas WASD no se usan en modo red.

Qué tanque te toca se decide al azar cuando las dos copias se emparejan, así
que la máquina que hace de servidor puede acabar siendo el jugador 2. El juego
te lo dice antes de empezar la partida.

#### Si no conectan

| Lo que ves | Qué es |
|---|---|
| `Timeout connecting to server` | El servidor no está arrancado todavía, o su cortafuegos bloquea el `UDP 5213` |
| `NET: discovery timed out` en el log | El túnel está montado pero los juegos no se han encontrado |
| `NET: node 000000000000` en el log | Esta máquina no está unida a ningún túnel |

El log de cada máquina queda en `runserv/GAME.LOG` y `runcli/GAME.LOG`
(`runlocal/GAME.LOG` en modo local), en esa misma máquina: el juego lo escribe
con una ruta RELATIVA, así que cae en el directorio desde el que lo lanzó
`play.sh`. Sal del juego con
**ESC** (no cerrando la ventana) para que se escriba la línea de resumen.

#### En DOS real, y a mano

El mismo `game.exe`, arrancado como `game.exe /net`. En hardware real hay que
cargar antes `LSL`, el driver ODI de tu tarjeta e `IPXODI`, y las dos máquinas
tienen que usar el mismo frame type en `NET.CFG`.

En DOSBox sin los scripts: pon `ipx=true` en `dosbox.conf`, y luego
`ipxnet startserver 5213` en una máquina e `ipxnet connect <su ip> 5213` en la
otra. En Linux el puerto tiene que ser mayor que 1024: el 213 que DOSBox usa
por defecto es privilegiado y no se puede abrir.

#### Cómo funciona

No se manda ninguna posición. Las dos máquinas ejecutan el juego entero, los
dos tanques incluidos, y lo único que viaja es **un byte de teclas pulsadas por
jugador y por frame**. Funciona porque el juego es determinista: mismas
entradas, mismos píxeles.

- [`doc/ES/MANUAL-RED.md`](doc/ES/MANUAL-RED.md) — el manual, de cero a jugar
  en red. Empieza por aquí si quieres entenderlo.
- [`doc/ES/NETWORK.md`](doc/ES/NETWORK.md) — la referencia.

---

## Documentación técnica / Technical documentation

Todo en [`doc/`](doc/), en español e inglés. / All under [`doc/`](doc/), in
Spanish and English.

| | Español | English |
|---|---|---|
| Sonido / Sound | [`doc/ES/SOUND.md`](doc/ES/SOUND.md) | [`doc/EN/SOUND.md`](doc/EN/SOUND.md) |
| **CURSO paso a paso / Step-by-step COURSE** | [`tutorial/README.md`](tutorial/README.md) | [`tutorial/README.md`](tutorial/README.md) |
| **Manual de cámara / Camera manual** | [`doc/ES/MANUAL-CAMARA.md`](doc/ES/MANUAL-CAMARA.md) | [`doc/EN/CAMERA-MANUAL.md`](doc/EN/CAMERA-MANUAL.md) |
| **Manual de red / Network manual** | [`doc/ES/MANUAL-RED.md`](doc/ES/MANUAL-RED.md) | [`doc/EN/NETWORK-MANUAL.md`](doc/EN/NETWORK-MANUAL.md) |
| Red, referencia / Network, reference | [`doc/ES/NETWORK.md`](doc/ES/NETWORK.md) | [`doc/EN/NETWORK.md`](doc/EN/NETWORK.md) |
| Probar la red / Testing the network | [`doc/ES/NETWORK-TESTING.md`](doc/ES/NETWORK-TESTING.md) | [`doc/EN/NETWORK-TESTING.md`](doc/EN/NETWORK-TESTING.md) |
| El reproductor WAV original / The original WAV player | [`doc/ES/SBWAV8-FLOW.md`](doc/ES/SBWAV8-FLOW.md) | [`doc/EN/SBWAV8-FLOW.md`](doc/EN/SBWAV8-FLOW.md) |

**Sonido** — DMA, doble buffer, el mezclador por software y el porqué de cada
decisión. / DMA, double buffering, the software mixer and the reasoning behind
every decision.

**Red** — IPX, lockstep, el retardo de entrada y la detección de
desincronización. / IPX, lockstep, the input delay and desync detection.

El **manual** va desde cero y está pensado para aprender; el otro es la
referencia. / The **manual** starts from nothing and is written for learning;
the other one is the reference.
