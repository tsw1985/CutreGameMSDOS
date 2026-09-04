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

### Network play

Two machines can play each other over IPX. One acts as the **server** and the
other as the **client**, but that is only about bringing up the connection:
**both machines play exactly the same game.**

Each machine needs its own copy of `bin\` and `res\`. The project can live in
a different folder on each one.

#### Step 1 — on the server machine

Start this one **first**:

```
./launch_game_server.sh
```

It prints the exact line to run on the other machine, with the IP already
filled in:

```
  SERVER ready. On the OTHER machine run:

      ./launch_game_client.sh 192.168.1.45 -p 5213
```

If it warns you about the firewall, open the port:

```
sudo ufw allow 5213/udp
```

#### Step 2 — on the client machine

Copy the line the server printed:

```
./launch_game_client.sh 192.168.1.45
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

The log for each machine is at `net-test/log-server/GAME.LOG` and
`net-test/log-client/GAME.LOG`, on that machine. Leave the game with **ESC**
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

### Juego en red

Dos máquinas pueden jugar entre ellas por IPX. Una hace de **servidor** y la
otra de **cliente**, pero eso es solo para levantar la conexión: **las dos
máquinas juegan exactamente igual.**

Cada máquina necesita su copia de `bin\` y `res\`. El proyecto puede estar en
carpetas distintas en cada una.

#### Paso 1 — en la máquina servidor

Arranca esta **primero**:

```
./launch_game_server.sh
```

Te imprime la línea exacta que hay que ejecutar en la otra máquina, con la IP
ya puesta:

```
  SERVER ready. On the OTHER machine run:

      ./launch_game_client.sh 192.168.1.45 -p 5213
```

Si te avisa del cortafuegos, abre el puerto:

```
sudo ufw allow 5213/udp
```

#### Paso 2 — en la máquina cliente

Copia la línea que te dio el servidor:

```
./launch_game_client.sh 192.168.1.45
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

El log de cada máquina queda en `net-test/log-server/GAME.LOG` y
`net-test/log-client/GAME.LOG`, en esa misma máquina. Sal del juego con
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
