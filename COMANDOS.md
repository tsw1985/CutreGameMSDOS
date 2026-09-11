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

## Opciones

```
-b            mapa grande 640x400 con cámara   (sin esto, 320x200)
-t sky|war|neon   el aspecto                    (necesita -b)
-l 1..5       la geometría del nivel            (necesita -b)
-T sky|war|neon   solo en 'both': tema de la 2ª ventana
-p PUERTO     puerto UDP. Por defecto 5213
-y CICLOS     cycles= de DOSBox. Por defecto "fixed 30000"
```

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

**Si acabaron en mundos distintos:**

```
NET DESYNC at frame 412: mine 51230 theirs 51237
```

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
