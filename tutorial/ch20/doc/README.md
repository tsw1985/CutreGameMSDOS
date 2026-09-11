# Capítulo 20 — El mundo deja de ser la pantalla

**Este capítulo está hecho para que algo salga mal.** A propósito.

```
make
chap20
```

El mapa pasa a ser de 640x400. El tanque se mueve por él, la pantalla **no se
mueve**, y en cuanto te alejas lo pierdes de vista y conduces a ciegas.

Es incómodo, y ese es el objetivo: **hasta que no sufres el problema, una cámara
parece un adorno.**

---

## 1. Lo único que cambia

```c
	bmp_init_buffers(640, 400);
```

Eso es todo. En vez de `(WIDTH, HEIGHT)`.

A partir de ahí `map_width` vale 640 y `map_height` 400, mientras que `WIDTH` y
`HEIGHT` **siguen valiendo 320 y 200** porque esa es la pantalla.

## 2. Los dos sistemas de coordenadas

Esta es la idea central de todo el bloque, y si te llevas una sola cosa, que sea
esta.

| | Qué es | Rango | Quién lo usa |
|---|---|---|---|
| **MUNDO** | Dónde están las cosas de verdad | 0..639, 0..399 | **Todo el juego**: posiciones, balas, muros |
| **PANTALLA** | Dónde se pinta algo este frame | 0..319, 0..199 | **Solo el código que dibuja** |

```
   MUNDO 640x400
   +--------------------------------+
   |                                |
   |      +--------------+          |
   |      |   PANTALLA   |          |
   |      |              |   T      |  <- el tanque existe y se mueve,
   |      |              |          |     pero no se ve
   |      +--------------+          |
   |                                |
   +--------------------------------+
```

El mismo tanque tiene **a la vez** una posición de mundo (que no cambia aunque
muevas la cámara) y una de pantalla (que cambia con la cámara aunque el tanque
no se mueva).

**Mezclarlas es el error número uno de este bloque.** Y `WIDTH` significa
pantalla para siempre: no lo cambies por `map_width` por inercia en el volcado a
la VGA ni en el recorte de sprites.

## 3. El juego funciona. Lo que no funciona es mirarlo

Fíjate en que el movimiento y las colisiones **no cambian nada**:

```c
		moved = tut_try_move(&tank, MOVE_UP);
```

`tut_try_move()` pregunta a `bmp_is_wall()`, que ahora conoce un mundo de
640x400 porque la máscara se cargó de `bigcol.bmp`. Todo va bien.

Lo que falta es decidir **qué trozo mirar**.

## 4. El límite dejó de ser código

Aquí ya no hay ningún recorte contra `WIDTH` ni `HEIGHT`. Lo que para al tanque
es el **muro pintado en el mapa**.

> **El límite dejó de ser código y pasó a ser dato.**

Es más elegante y más flexible: cambias el mapa y cambian los límites, sin
recompilar.

Con dos avisos:

- **Las guardas de desbordamiento se quedan.** Los `if (position >= PASO)` antes
  de una resta no son límites de pantalla: evitan que un `unsigned` dé la vuelta
  a 65535. Con un borde bien pintado nunca se disparan, pero están para el día
  que dibujes un mapa mal.
- **Borde grueso**, mínimo 8 píxeles. El del mapa grande tiene de 16 a 33.

## 5. Los spawns hay que revisarlos

```c
	player_reset(&tank, BIG_PLAYER1_START_X, BIG_PLAYER1_START_Y, ...);
```

El spawn de siempre del jugador 2, `PLAYER2_START_Y = 16`, **cae dentro del muro
del borde** del mapa grande, que tiene 17 píxeles de grosor. El tanque nacería
atrapado: todas las direcciones bloqueadas desde el primer frame.

Por eso `header/players.h` tiene un juego de spawns aparte para el mapa grande.
**Al cambiar de mundo, revisa los puntos de partida.**

## 6. Por qué no revienta

El tanque se pinta en su coordenada de mundo tal cual, sin restar nada. En
cuanto pasa de x=320, se pinta fuera de la pantalla.

Y **no revienta** porque `draw_sprite_to_buffer()` recorta, desde el capítulo 4.

Sin ese recorte, esto estaría machacando memoria ajena ahora mismo, y el juego
se colgaría minutos después en cualquier otro sitio. Guarda esa idea: el
capítulo 4 parecía prudencia excesiva y aquí es lo que te salva.

## 7. Experimentos

1. **Vete a la esquina de abajo a la derecha** y mira lo que imprime al salir.
   Coordenadas mayores que 320 y 200: estabas vivo en un sitio invisible.
2. **Cambia a `bmp_init_buffers(WIDTH, HEIGHT)`** y carga `cutre.bmp`. Todo
   vuelve a funcionar porque el mundo vuelve a ser la pantalla.
3. **Usa `PLAYER2_START_Y`** en vez del grande. El tanque nace dentro del muro y
   no se puede mover.
4. **Comenta el recorte** en `draw_sprite_to_buffer()` de `src/bmp.c`, sal del
   primer cuadrante y prepárate para reiniciar DOSBox. *(Y acuérdate de
   deshacerlo.)*

## 8. Lo que hay que llevarse

| | |
|---|---|
| **MUNDO vs PANTALLA** | La idea central del bloque |
| `WIDTH`/`HEIGHT` son la pantalla, para siempre | `map_width`/`map_height` el mundo |
| El juego funciona sin cámara; **verlo, no** | |
| El límite pasó de código a dato | Con borde grueso y guardas de desbordamiento |
| Al cambiar de mundo, revisa los spawns | |

---

**Anterior:** [Capítulo 19](../../ch19/doc/README.md) ·
**Siguiente:** [Capítulo 21 — La cámara](../../ch21/doc/README.md)
