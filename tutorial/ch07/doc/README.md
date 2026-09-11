# Capítulo 7 — Colisiones contra el mapa

*[English version](README-EN.md)*

**Qué vas a conseguir:** un tanque que no atraviesa las paredes. Y poder ver,
pulsando TAB, **el mapa que ve el juego** en vez del que ves tú.

**Qué código real se usa:** `src/bmp.c` (`bmp_is_wall`), `src/players.c`
(`player_update_future_collision_points`).

```
make
chap07
```

Pulsa **TAB** dentro del juego. Ahí está la mitad de la lección.

---

## 1. Dos mapas, no uno

El juego carga **dos** ficheros distintos:

| Fichero | Qué es | Para qué |
|---|---|---|
| `cutre.bmp` | El dibujo bonito: ladrillos, arbustos | **Solo para mirar** |
| `cutrecol.bmp` | Dos colores: azul suelo, amarillo muro | **Solo para decidir** |

Parece un desperdicio tener dos. No lo es.

## 2. Por qué el dibujo no vale para decidir

Aquí está el concepto del capítulo, y por eso el programa te deja ver los dos.

**El dibujo miente.** Un muro de ladrillo tiene líneas oscuras de mortero entre
ladrillo y ladrillo. Si preguntas "¿de qué color es este píxel?" y caes en una
junta, te contesta *negro*, y el juego concluye que ahí no hay pared.

En el mapa del juego grande hay **22.806 píxeles** que son negros en el dibujo y
sí son muro.

Y al revés: los arbustos se dibujan pero se pueden pisar. **13.047 píxeles** que
parecen sólidos y no lo son.

Pulsa TAB y compara. Los muros de colisión son **rectángulos limpios** que no
siguen el contorno exacto del dibujo. Eso es a propósito: el dibujo es para el
ojo, la colisión es para las reglas.

Hay una tercera fuente que **nunca** debes usar: leer la memoria de vídeo. Para
cuando lo haces, los tanques ya están pintados encima, y preguntar por el píxel
de la punta del cañón te devuelve el color del propio tanque.

## 3. Mirar antes de saltar

La segunda idea del capítulo, y es la que hay que copiar a cualquier juego que
hagas.

Hay dos formas de gestionar una colisión:

**La mala:** mover el tanque, comprobar si ha quedado dentro de una pared, y si
sí, devolverlo. Eso deja un instante en el que el tanque está **dentro** del
muro. Cualquier cosa que mire el estado en ese momento ve algo imposible: la
IA, el sonido, el código de red.

**La buena, que es la que usa el juego:**

```c
static int try_move(int direction){

	tank.current_direction = direction;          /* 1. girar es gratis */

	if (is_blocked_by_wall(direction) == 1){     /* 2. preguntar */
		return 0;
	}

	/* 3. y solo ahora, moverse */
	...
}
```

Se calcula **dónde estaría**, se comprueba eso, y solo si está libre se aplica.
El tanque nunca llega a estar dentro de una pared, ni por un frame.

Fíjate en que girar se hace **antes** de la comprobación: cambiar de dirección
nunca puede chocar, y quieres que el tanque mire hacia donde empujas aunque no
pueda avanzar.

## 4. Tres puntos, no 324

```c
	player_update_future_collision_points(&tank, direction);

	if (bmp_is_wall(tank.future_cannon_tip_x, tank.future_cannon_tip_y)) return 1;
	if (bmp_is_wall(tank.future_track1_x,     tank.future_track1_y))     return 1;
	if (bmp_is_wall(tank.future_track2_x,     tank.future_track2_y))     return 1;
```

El tanque mide 18x18 = 324 píxeles. ¿Por qué solo tres?

```
   Yendo hacia ARRIBA:

        T           <- punta del canon
      O   O         <- las dos orugas
      +-------+
      |       |     el resto del tanque viene detras,
      |       |     por donde ya ha pasado
      +-------+
```

Porque **el tanque solo avanza hacia delante**. Lo que puede tocar algo primero
es su frente, y el frente son tres puntos: la punta del cañón y las dos
esquinas de las orugas.

Comprobar los 324 costaría 324 lecturas por tanque y por frame. En un 486, eso
se nota. Y no cambiaría ni una decisión.

`player_update_future_collision_points()` sabe qué tres puntos son según la
dirección, y los deja en los campos `future_*` de la estructura.

## 5. Una regla al dibujar mapas

Esta no la puede comprobar el código y te va a morder:

> **Un muro tiene que ser más grueso que el paso del tanque.**

El tanque avanza `PIXEL_TO_MOVE` = **2 píxeles** por vez. Las coordenadas que
puede ocupar son inicio, inicio+2, inicio+4... **nunca las intermedias**.

Un muro de 1 píxel de grosor puede caer justo en una coordenada que el tanque
nunca pisa, y **lo atraviesa sin enterarse**.

Los mapas de este juego tienen muros de 8 píxeles o más. Y en el mapa grande, el
borde exterior tiene de 16 a 33.

## 6. Las orugas, otra vez

```c
		if (moved == 1){
			update_animation();
		}
```

`try_move()` devuelve si el tanque **avanzó de verdad**. Así, empujando contra
una pared, las orugas se quedan quietas en vez de patinar.

Es un detalle diminuto que se nota muchísimo al jugar.

## 7. Cómo se dibuja el mapa de colisiones

Cuando pulsas TAB, el programa pinta el fondo preguntándole a `bmp_is_wall()`
píxel a píxel:

```c
			for (py = 0; py < HEIGHT; py++){
				for (px = 0; px < WIDTH; px++){
					if (bmp_is_wall(px, py) == 1){
						buffer_background_image_data[offset] = 252;
					}else{
						buffer_background_image_data[offset] = 3;
					}
				}
			}
```

64.000 llamadas por frame. Es lentísimo y da igual: es una ayuda para aprender,
no forma parte del juego.

Lo interesante es que **no está leyendo `cutrecol.bmp`**. Ese fichero ya no
existe en memoria: al cargarlo, `bmp_fill_background_collision_in_buffer()` lo
convirtió en una **máscara de un bit por píxel**. El capítulo 22 explica por
qué.

## 8. Experimentos

1. **Pulsa TAB y pásate el mapa entero.** Ver el mundo como lo ve el juego
   cambia cómo piensas sobre él.
2. **Comenta la llamada a `bmp_fill_background_collision_in_buffer()`.** Sin
   máscara, `bmp_is_wall()` devuelve 0 en todo y atraviesas las paredes.
3. **Invierte el `if` en `is_blocked_by_wall()`** (`== 0` en vez de `== 1`).
   Ahora solo puedes moverte *dentro* de las paredes.
4. **Comprueba solo la punta del cañón**, quitando las dos orugas. Verás que el
   tanque mete las esquinas dentro de los muros al pasar rozando.
5. **Sube `PIXEL_TO_MOVE`** a 10 en `header/players.h` y recompila. Empezarás a
   atravesar muros finos: el problema del punto 5, en vivo.

## 9. Lo que hay que llevarse

| | |
|---|---|
| **Dos mapas: uno para mirar, otro para decidir** | El dibujo miente |
| **Mirar antes de saltar** | Calcula dónde estarías, comprueba, y luego múevete |
| Tres puntos, no el rectángulo | Solo el frente puede tocar primero |
| Los muros, más gruesos que el paso | Si no, se atraviesan |
| La animación solo avanza si avanzaste | Empujar una pared no mueve las orugas |

---

**Anterior:** [Capítulo 6](../../ch06/doc/README.md) ·
**Siguiente:** [Capítulo 8 — Dos jugadores](../../ch08/doc/README.md)
