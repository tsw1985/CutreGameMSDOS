# Capítulo 6 — Animación: que las orugas se muevan

**Qué vas a conseguir:** el tanque del capítulo 5, pero con las orugas girando
mientras avanza y paradas cuando no.

**Qué código real se usa:** `src/bmp.c`, `src/players.c`, `header/players.h`.

```
make
chap06
```

---

## 1. Una animación son dos dibujos y un contador

Nada más. En `sprites.bmp` cada dirección del tanque tiene **dos** versiones,
con las orugas en posiciones distintas:

```
   +-----------------+-----------------+
   | arriba, frame 0 | arriba, frame 1 |
   |     (2, 5)      |     (23, 5)     |
   +-----------------+-----------------+
```

Alternando entre las dos, el ojo ve una oruga girando. Ocho sprites en total:
cuatro direcciones por dos fotogramas.

## 2. Por qué no puedes cambiar en cada frame

Aquí está la lección.

El bucle gira a **70 vueltas por segundo** (lo fija el retrazo, capítulo 3). Si
cambiaras de dibujo en cada vuelta, las orugas girarían 70 veces por segundo.

Eso no se ve como movimiento: se ve como un **borrón**. Demasiado rápido para
que el ojo distinga los fotogramas.

La solución es contar:

```c
	tank.speed_counter = tank.speed_counter + 1;

	if (tank.speed_counter >= tank.speed_total){

		tank.speed_counter = 0;

		tank.current_frame = tank.current_frame + 1;

		if (tank.current_frame >= tank.total_frames){
			tank.current_frame = 0;
		}

	}
```

Con `speed_total = 5`, el dibujo cambia una vez cada 5 frames: **14 cambios por
segundo**. Eso sí se lee como movimiento.

| `speed_total` | Cambios por segundo | Cómo se ve |
|---|---|---|
| 1 | 70 | Un borrón |
| 3 | 23 | Frenético |
| **5** | **14** | Lo que usa el juego |
| 15 | 4,6 | A cámara lenta |

Ese contador es un patrón que vas a usar siempre: **separar la velocidad del
juego de la velocidad de la animación**.

## 3. Solo gira si el tanque avanza

El segundo detalle, y es el que hace que se vea bien:

```c
		if (is_moving == 1){
			update_animation();
		}
```

Un tanque parado tiene las orugas quietas. Suena obvio, pero si llamas a
`update_animation()` siempre, el tanque quieto sigue moviendo las orugas como
si patinara en el sitio, y queda fatal.

En el capítulo 7 esto se afina todavía más: las orugas solo girarán si el
tanque **avanzó de verdad**, no si lo intentó y chocó contra una pared.

## 4. Aparece `struct player`

Este es el primer capítulo que usa la estructura real del juego, de
`header/players.h`:

```c
struct player tank;
```

Dentro está todo lo que un tanque tiene: dónde está, hacia dónde mira, sus
punteros a sprites, su bala, su explosión y su marcador.

Y dos funciones de `src/players.c`:

| | |
|---|---|
| `player_init(&tank)` | Reserva los buffers de los sprites y pone los valores de animación por defecto |
| `player_reset(&tank, x, y, dir)` | Coloca el tanque para empezar una ronda |

Que sea una `struct` y no un montón de variables sueltas es lo que va a
permitir, en el capítulo 8, tener **dos** tanques sin duplicar ni una línea.

## 5. Elegir el dibujo son dos preguntas

```c
	if (tank.current_direction == MOVE_UP){
		if (tank.current_frame == 0){ return tank.sprite_tank_up; }
		return tank.sprite_tank_up_2;
	}
```

Primero hacia dónde mira, después qué fotograma toca. Dos datos independientes:
`current_direction` la cambia el teclado, `current_frame` la cambia el contador.

## 6. Experimentos

1. **Pon `speed_total` a 1** justo después de `player_reset()`:
   `tank.speed_total = 1;` Mira el borrón.
2. **Ponlo a 20.** Cámara lenta.
3. **Llama a `update_animation()` siempre**, fuera del `if`. El tanque quieto
   patina.
4. **Quita el `if (tank.current_frame >= tank.total_frames)`.** El contador
   crece sin parar y `pick_sprite()` devuelve siempre el fotograma 1: la
   animación se congela en la segunda pose.
5. **Cambia las coordenadas de un sprite** para que los dos fotogramas sean el
   mismo. El tanque se mueve pero las orugas no. Así ves cuánto aporta.

## 7. Lo que hay que llevarse

| | |
|---|---|
| Una animación = N dibujos + un contador | No hace falta nada más |
| **No cambies de dibujo en cada frame** | 70 por segundo es un borrón |
| El contador separa velocidad de juego de velocidad de animación | Patrón reutilizable |
| La animación solo avanza si hay movimiento | Si no, patina |
| `struct player` junta todo lo de un tanque | Y por eso dos tanques serán gratis |

---

**Anterior:** [Capítulo 5](../../ch05/doc/README.md) ·
**Siguiente:** [Capítulo 7 — Colisiones](../../ch07/doc/README.md)
