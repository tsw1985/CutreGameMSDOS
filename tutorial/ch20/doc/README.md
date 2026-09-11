# Capítulo 20 — El mundo deja de ser la pantalla

*[English version](README-EN.md)*

**Este capítulo está hecho para que algo salga mal.** A propósito.

```
make
chap20
```

El mapa pasa a ser de 640x400. La pantalla **no se mueve**. En cuanto te alejas
pierdes el tanque de vista y conduces a ciegas.

Es incómodo, y ese es el objetivo: **hasta que no sufres el problema, una cámara
parece un adorno.**

---

## 1. Lo único que cambia

```c
	bmp_init_buffers(640, 400);        /* antes: (WIDTH, HEIGHT) */
```

Eso es todo. A partir de ahí:

| | Vale | Qué es |
|---|---|---|
| `WIDTH` / `HEIGHT` | 320 / 200 | **La pantalla.** No cambian nunca |
| `map_width` / `map_height` | 640 / 400 | **El mundo** |

Antes coincidían y por eso daba igual cuál usaras. **Ahora no**, y ahí empiezan
los problemas.

## 2. Los dos sistemas de coordenadas

Esta es la idea central del bloque. Si te llevas una sola cosa, que sea esta.

| | Qué es | Rango | Quién lo usa |
|---|---|---|---|
| **MUNDO** | Dónde están las cosas de verdad | 0..639, 0..399 | **Todo el juego**: posiciones, balas, muros, colisiones |
| **PANTALLA** | Dónde se pinta algo este frame | 0..319, 0..199 | **Solo el código que dibuja** |

```
   EL MUNDO, 640x400
   +--------------------------------------+
   |                                      |
   |   +--------------+                   |
   |   |   PANTALLA   |                   |
   |   |              |         T         |  <- el tanque esta aqui,
   |   |              |                   |     vivo y moviendose,
   |   +--------------+                   |     y no lo ves
   |                                      |
   +--------------------------------------+
```

El mismo tanque tiene **a la vez** dos coordenadas:

- una de **mundo**, que no cambia aunque muevas la ventana
- una de **pantalla**, que cambia con la ventana aunque el tanque no se mueva

### Un ejemplo con números

| | |
|---|---|
| El tanque está, en el mundo, en | **(500, 150)** |
| La ventana empieza en | **(320, 100)** |
| Luego en pantalla se pinta en | (500−320, 150−100) = **(180, 50)** |

Y ahora **mueve la ventana a (400, 100) sin tocar el tanque**:

| | |
|---|---|
| El tanque sigue, en el mundo, en | **(500, 150)** |
| La ventana ahora está en | **(400, 100)** |
| En pantalla se pinta en | (500−400, 150−100) = **(100, 50)** |

El tanque **no se ha movido ni un píxel**, y se dibuja 80 píxeles más a la
izquierda. Eso es scroll, y es todo lo que es.

En este capítulo la ventana está clavada en **(0, 0)**, así que mundo y pantalla
coinciden… **mientras el tanque esté en el primer cuadrante**. En cuanto pasa de
x=320, se pinta fuera y desaparece.

## 3. Qué pasa si mezclas los dos sistemas

*"Es el error número uno"* no sirve de nada si no sabes **a qué se parece**. Los
síntomas concretos:

**Usar coordenadas de pantalla para la colisión**
El tanque choca con paredes que no están ahí y atraviesa las que sí. Y lo
desconcertante: **funciona perfectamente mientras estés cerca del origen**,
porque ahí los dos sistemas coinciden. El bug solo aparece cuando te alejas.

**Olvidar restar la cámara al pintar UN objeto**
Ese objeto se queda **pegado a la pantalla** mientras todo lo demás se desliza.
Una bala que te sigue a todas partes en vez de quedarse en el mundo. Es muy
visual y muy fácil de reconocer una vez lo has visto.

**Restar la cámara dos veces**
El objeto se mueve **al doble de velocidad** y en sentido contrario al esperado.

**Cambiar `WIDTH` por `map_width` donde no toca**
Si lo haces en el volcado a la VGA, vuelcas 256.000 bytes a una pantalla de
64.000 y machacas memoria. Si lo haces en el recorte de sprites, dejan de
recortarse en el borde derecho.

> `WIDTH` significa **pantalla** para siempre. En el volcado a la VGA, en el
> buffer de imagen y en el recorte de sprites, **no se toca**.

## 4. El juego funciona. Lo que no funciona es mirarlo

Fíjate en que el movimiento y las colisiones **no cambian nada**:

```c
		moved = tut_try_move(&tank, MOVE_UP);
```

`tut_try_move()` pregunta a `bmp_is_wall()`, que ahora conoce un mundo de
640x400 porque la máscara se cargó de `bigcol.bmp`. **Todo va bien**: te mueves
por las cuatro salas, chocas con los muros correctos, cruzas las puertas.

Lo único que falta es decidir **qué trozo mirar**. Y eso es lo que hace la
partida frustrante en vez de rota.

## 5. Lo que cuesta ahora en memoria

Multiplicas el mundo por 4, así que la pregunta es inmediata:

| | 320x200 | 640x400 |
|---|---:|---:|
| Dibujo del mapa | 64.000 | **256.000** |
| Máscara de colisión | 8.000 | **32.000** |
| Buffer de pantalla | 64.000 | 64.000 (no cambia) |

El dibujo pasa de caber en un `malloc()` normal a **no caber ni de lejos**: el
máximo de `malloc` en Turbo C son 65.535 bytes.

Por eso el mapa se pide con `farmalloc()` y su puntero está declarado `huge`. El
**capítulo 23** explica los dos y por qué esos 256.000 bytes casi tumban el
proyecto.

De momento quédate con que el mapa grande **no es gratis**, y que en una máquina
de 640 KB eso se nota.

## 6. El límite dejó de ser código

Aquí ya no hay ningún recorte contra `WIDTH` ni `HEIGHT` en el movimiento. Lo
que para al tanque es el **muro pintado en el mapa**.

> **El límite dejó de ser código y pasó a ser dato.**

Es más elegante y más flexible: cambias el mapa y cambian los límites, sin
recompilar.

Con dos avisos:

**Las guardas de desbordamiento se quedan.** Los `if (position >= PASO)` antes
de una resta **no son límites de pantalla**: evitan que un `unsigned` dé la
vuelta a 65.535. Con un borde bien pintado nunca se disparan, pero están para el
día que dibujes un mapa mal.

**Borde grueso**, 8 píxeles mínimo. El tanque avanza de 2 en 2, así que solo
ocupa posiciones pares desde su origen: un muro de 1 píxel puede caer justo en
una coordenada que nunca pisa y **lo atraviesa**. El mapa grande tiene de 16 a
33 píxeles de borde.

## 7. Los spawns hay que revisarlos

```c
	player_reset(&tank, BIG_PLAYER1_START_X, BIG_PLAYER1_START_Y, ...);
```

El spawn de siempre del jugador 2, `PLAYER2_START_Y = 16`, **cae dentro del muro
superior** del mapa grande, que tiene 17 píxeles de grosor.

El tanque nacería **atrapado**: todas las direcciones bloqueadas desde el primer
frame, y sin ningún error. Parecería que el teclado no funciona.

Por eso `header/players.h` tiene un juego de spawns aparte para el mapa grande.
**Al cambiar de mundo, revisa los puntos de partida.**

## 8. Por qué no revienta

El tanque se pinta en su coordenada de mundo tal cual, sin restar nada. En
cuanto pasa de x=320 se pinta fuera de la pantalla.

Y **no revienta** porque `draw_sprite_to_buffer()` recorta, desde el capítulo 4.

Sin ese recorte, la coordenada 400 con parámetros `unsigned` estaría escribiendo
a 80 píxeles más allá del final de cada fila, y el programa corrompería memoria
ajena ahora mismo. El síntoma no sería un error: sería que el juego se cuelga
cinco minutos después, en otro sitio.

Guarda esa idea: **el capítulo 4 parecía prudencia excesiva y aquí es lo que te
salva.**

## 9. Experimentos

1. **Vete a la esquina de abajo a la derecha** y mira lo que imprime al salir.
   Coordenadas mayores que 320 y 200: estabas vivo en un sitio invisible.
2. **Cuenta los pasos** desde el spawn hasta perder el tanque de vista. Con
   `PIXEL_TO_MOVE = 2` y el spawn en x=166, son (320−166−18)/2 = **68 frames**.
3. **Vuelve a `bmp_init_buffers(WIDTH, HEIGHT)`** y carga `cutre.bmp`. Todo
   funciona otra vez, porque el mundo vuelve a ser la pantalla. **El código no
   ha cambiado**: solo los dos números.
4. **Usa `PLAYER2_START_Y`** (el de 16) en vez del grande. El tanque nace dentro
   del muro y no se puede mover. Sin ningún mensaje.
5. **Comenta el recorte** de `draw_sprite_to_buffer()` en `src/bmp.c`, sal del
   primer cuadrante, y prepárate para reiniciar DOSBox. *(Acuérdate de
   deshacerlo.)*

## 10. Lo que hay que llevarse

| | |
|---|---|
| **MUNDO vs PANTALLA** | La idea central del bloque |
| `WIDTH`/`HEIGHT` son la pantalla **para siempre** | `map_width`/`map_height` el mundo |
| `pantalla = mundo − cámara` | Una resta, y ya está |
| Mezclarlos **funciona cerca del origen** y falla lejos | Por eso cuesta tanto de encontrar |
| El juego funciona sin cámara; **verlo, no** | |
| El mapa grande cuesta 256.000 bytes | Capítulo 23 |
| Al cambiar de mundo, revisa los spawns | |

---

**Anterior:** [Capítulo 19](../../ch19/doc/README.md) ·
**Siguiente:** [Capítulo 21 — La ventana](../../ch21/doc/README.md)
