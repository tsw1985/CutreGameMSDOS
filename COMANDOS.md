# Comandos

**Un solo script: `play.sh`.** El primer argumento es el modo.

```bash
./play.sh          # la ayuda, si se te olvida algo
```

---

## 1. Local — dos personas en un teclado

```bash
./play.sh local
```

Una ventana, mapa pequeño, sin red.

| | |
|---|---|
| Jugador 1 | flechas + `5` del teclado numérico |
| Jugador 2 | W A S D + `G` |

No admite tema ni nivel: eso es solo del mapa grande.

---

## 2. Los dos en tu ordenador — dos ventanas

```bash
./play.sh both                      # mapa pequeño
./play.sh both -b                   # mapa grande
./play.sh both -b -t neon           # ... con tema
./play.sh both -b -t neon -l 3      # ... con tema y nivel
```

Es el modo para probar: abre dos ventanas de DOSBox conectadas entre sí, sin
segunda máquina y sin escribir ninguna IP.

**Un tema distinto en cada ventana:**

```bash
./play.sh both -b -t war -T neon -l 4
```

`-T` viste solo la segunda ventana. Mismo combate, dos pinturas.

---

## 3. Dos ordenadores de verdad

```bash
# ordenador 1  (ARRANCA PRIMERO)
./play.sh server -b -t war -l 3

# ordenador 2  (la IP te la dice el servidor al arrancar)
./play.sh client 192.168.1.45 -b -t neon -l 3
```

El servidor te imprime la orden exacta del cliente, con las opciones ya
puestas. Cópiala.

---

## 4. La intro

```bash
./play.sh local -demo
./play.sh both -b -t neon -l 3 -demo
```

Enseña `res/demo/demo01.bmp` … `demo15.bmp`, **cada una con un efecto
distinto**, ocho segundos cada una y con la música sonando. `ESC` se la salta y
empieza la partida.

Vale `-demo`, `-d` o `/demo`, lo que te salga.

### En red: la pone el SERVIDOR

Funciona en los cuatro modos, y en red **la intro la ve una sola máquina: el
servidor**. Siempre. Los comandos exactos:

```bash
# ordenador 1, el que ve la intro   (arranca este primero)
./play.sh server -b -t war -l 3 -demo

# ordenador 2, sin -demo            (la IP te la dice el servidor)
./play.sh client 192.168.1.45 -b -t war -l 3
```

El cliente puedes lanzarlo **cuando te dé la gana**: mientras el servidor está
con la intro, espera y te lo dice.

Y `-demo` en el cliente **da error a propósito**:

```
ERROR: The client does not show the intro, the server does.
```

No es capricho. Antes lo decidían las dos máquinas hablando entre ellas, y con
`-demo` en las dos desempataba el jugador 1 — que sale de comparar dos números
aleatorios que **pueden salir iguales**, y entonces las dos se creían el
jugador 2 y las dos se quedaban esperando una intro que no ponía nadie.

Ahora lo decide el rol de la línea de órdenes, que no puede salir igual en las
dos.

La que espera te lo dice en pantalla:

```
The other machine is showing the intro.
Press ESC to skip it on both.
```

**`ESC` funciona desde los dos teclados**, y eso importa: quien mira una
pantalla que pone *"esperando"* es justo el que más ganas tiene de saltársela.
Le da al ESC y la corta en la otra máquina.

Y las dos **salen a la vez**. No es un detalle: la música va en streaming y
nadie la sincroniza, así que una máquina que empezara la partida un segundo
antes se quedaría un segundo por delante hasta la última nota.

En **`both`** solo la ventana 1 recibe `-demo`. En un solo PC, ver la misma
intro en una ventana y un *"esperando"* en la otra no tendría ningún sentido.

### Los diez efectos

| | |
|---|---|
| **rotozoom** | Gira y hace zoom a la vez, sobre negro |
| **zoom** | Se acerca y se aleja respirando |
| **wobble** | Ondula como una bandera |
| **ripple** | Ondas que salen del centro y se apagan al alejarse |
| **scroll** | Desplazamiento diagonal infinito que da la vuelta |
| **stripes** | Bandas horizontales a velocidades distintas |
| **blinds** | Persiana veneciana que se abre y se cierra |
| **mosaic** | Pixelado que se abre y se cierra |
| **bounce** | La foto se pasea por la pantalla |
| **cycle** | La imagen quieta y la **paleta** girando |

Con 15 imágenes y 10 efectos, la lista vuelve a empezar. Es a propósito.

### Tus propias imágenes

Machaca las de `res/demo/` con las tuyas, con esos mismos nombres. Requisitos:

| | |
|---|---|
| Tamaño | **320x200 exactos** |
| Colores | **256** (8 bits por píxel), BMP sin comprimir |
| Peso | **65.078 bytes exactos** |
| Color 0 | negro |

Lo de los 65.078 bytes no es manía. Un BMP de 256 colores que use **menos**
colores lo guardan algunos editores con la paleta corta, y entonces los píxeles
no empiezan en el byte 1078, que es donde el juego los busca a pelo: la imagen
sale **descuadrada en diagonal** en vez de dar un error. `play.sh` te avisa
antes de abrir DOSBox:

```
AVISO: estas no miden 65078 bytes y saldran torcidas:
      demo07.bmp(64000)
```

Y el color 0 tiene que ser negro porque los efectos que recortan (`rotozoom`,
`zoom`, `bounce`) rellenan con él lo que queda fuera de la imagen. Si no, te
sale un marco de un color raro.

Una imagen que falte **se salta** y la presentación sigue, así que puedes irlas
poniendo de una en una.

Para cambiar la canción, `DEMO_SONG` en `demos/demos.h`. Tiene que ser un WAV
de 8 bits, mono y 44100 Hz exactos.

---

## Opciones

```
-b            mapa grande 640x400 con cámara   (sin esto, 320x200)
-t sky|war|neon   el aspecto                    (necesita -b)
-l 1..5       la geometría del nivel            (necesita -b)
-T sky|war|neon   solo en 'both': tema de la 2ª ventana
-d, -demo     la intro antes de jugar
-p PUERTO     puerto UDP. Por defecto 5213
-y CICLOS     cycles= de DOSBox. Por defecto "max"
-c NUCLEO     core= de DOSBox.   Por defecto "dynamic"
```

### Sobre `-y` y `-c`: la potencia de DOSBox

Por defecto DOSBox va a `core=dynamic` y `cycles=max`, o sea **a todo lo que dé
tu PC**. Antes iba a `fixed 30000`, que es un 386DX-40 flojo, y con eso los
efectos de la intro que van píxel a píxel se arrastraban.

**Nada puede ir "demasiado rápido".** El juego y las demos esperan al retrazo
vertical, así que están topados a unos 70 fotogramas por segundo pase lo que
pase: más potencia solo hace que a cada fotograma le **sobre** tiempo en vez de
faltarle.

| Si… | |
|---|---|
| el **sonido** da tirones | `./play.sh local -demo -y "fixed 100000"` |
| algo va **raro** | `./play.sh local -c auto -y "fixed 30000"` (el comportamiento de siempre) |

---

## Qué tiene que coincidir entre las dos máquinas

| | ¿Coincidir? | Qué pasa si no |
|---|---|---|
| **`-b`** | **Sí** | Mapas de distinto tamaño. El juego avisa de desincronización |
| **`-l`** | **Sí** | Lo negocian, pero gana el jugador 1 — y es un sorteo |
| **`-t`** | **No** | Nada. Cada uno ve el suyo y la partida es la misma |

### Por qué el nivel sí, y con cuidado

Al emparejarse, cada máquina sacó un **número aleatorio** y **el más bajo es el
jugador 1**:

```c
if (net_get_local_id() < net_get_remote_id()){
    is_player1 = 1;
}
```

Eso **no tiene nada que ver con quién lanzó el servidor**. Es una moneda al aire
en cada partida.

Y como el jugador 1 es quien elige el nivel, si lo pones solo en un lado:

- Si esa máquina sale jugador 1 → se juega tu nivel ✔
- Si sale jugador 2 → manda la otra, que no puso nivel → mapa original ✘

**Pon el mismo `-l` en las dos.** Si se te olvida no se rompe nada: lo negocian,
gana el jugador 1, y al otro se lo dice por pantalla:

```
Player 1 chose level 5, so that is what we play.
```

Lo que nunca pasa es que cada uno cargue un mapa distinto.

### Por qué el tema no importa

Los tres temas comparten el mapa de colisiones **byte a byte**: son el mismo
mundo repintado. El tema no decide nada del juego, igual que la cámara, así que
tampoco entra en el checksum.

---

## Los cinco niveles

| | | |
|---|---|---|
| `-l 1` | Campo abierto | Arena amplia, coberturas separadas |
| `-l 2` | Cruce de fuego | Cuatro sectores con puertas amplias |
| `-l 3` | Emboscada | Nueve salas, rutas circulares |
| `-l 4` | Cerco | Doce sectores, cuellos de botella |
| `-l 5` | La ratonera | Veinte celdas en circuito |

Los niveles solo existen vestidos, así que `-l 3` sin `-t` usa `sky` y te lo
dice.

---

## Controles

| | |
|---|---|
| **Local** | J1: flechas + `5` numérico · J2: W A S D + `G` |
| **En red** | Los dos: flechas + `5` numérico |
| **Salir** | `ESC`. No cierres la ventana, o el log queda a medias |

---

## Si algo falla

```bash
cat runserv/GAME.LOG     # la ventana servidor
cat runcli/GAME.LOG      # la ventana cliente
cat runlocal/GAME.LOG    # modo local
```

En `both`, el script te enseña las líneas importantes de los dos al terminar.

**La línea que resume qué cargó:**

```
Map 640x400 theme 3 level 2  mem: near NNNNN far NNNNN
```

**Si usaste `-demo`, estas dos tienen que dar el MISMO número:**

```
Demo: starting, memory free NNNNNN
Demo: finished, memory free NNNNNN
```

Si el segundo es menor, la intro se quedó algo, y lo que se quedó está entre el
juego y los 256.000 bytes contiguos que necesita el mapa.

**Y si la intro no aparece:**

```
Radar: the original look has no numbers, no radar
```

(esa es del radar, no de la intro) — para la intro, mira que `res/demo/` tenga
las imágenes; el juego se salta en silencio la que no encuentre.

**Si acabaron en mundos distintos:**

```
NET DESYNC at frame 412: mine 51230 theirs 51237
```

**Si en una máquina DOS de verdad no se ve el mapa, o los colores salen mal:**
prueba desde **`COMMAND.COM`**, no desde una ventana de MS-DOS de Windows 98.
Windows virtualiza el vídeo y la paleta no llega entera a la tarjeta. En DOSBox
esto no pasa nunca, y por eso despista tanto.

**Si el cliente se queda en "Timeout connecting to server":** el servidor no
está arrancado todavía, o su cortafuegos bloquea el puerto
(`sudo ufw allow 5213/udp`).

---

## Por qué un solo script

Antes eran cinco (`launch_game_local/both/server/client/common.sh`), 905 líneas.
Los cuatro modos compartían casi todo: las mismas comprobaciones, el mismo
`.conf`, el mismo directorio de trabajo. Lo único que cambiaba era la línea de
`ipxnet` y cuántas ventanas se abren.

Ahora es `play.sh` y el modo es la primera palabra. Si buscas los antiguos en el
historial de git, están hasta el commit *"Added basic themes"*.
