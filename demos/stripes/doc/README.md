# stripes — Bandas a distinta velocidad

*[English version](README-EN.md)*

**Efecto 4 de 10 del recorrido.** Coste medido: **7,6 microsegundos por frame**.

El código está en [`../stripes.c`](../stripes.c).

---

## Qué hace

La pantalla se corta en bandas horizontales y cada una se desplaza a su ritmo:
unas rápido a la derecha, otras despacio a la izquierda. La imagen se
descuartiza y se recompone sola cuando las velocidades vuelven a coincidir.

## La idea

Es el **parallax** de los juegos de plataformas usado al revés. Allí el fondo
lejano va más lento que el suelo para dar profundidad; aquí sirve para romper la
imagen.

Y la clave está en de dónde sale la velocidad de cada banda: **de un seno de su
número de banda**, no de un número al azar.

```
   banda  0  1  2  3  4  5  6  7  ...
   veloc. +6 +5 +3  0 -3 -5 -6 -5      <- un seno
```

Con velocidades aleatorias parece una televisión estropeada. Con un seno, las
bandas vecinas van parecido y el conjunto **ondula**.

## Paso a paso

**1. Un desplazamiento acumulado por banda**, en un array de 25 enteros.

**2. La velocidad de cada banda, fija durante todo el efecto:**

```c
	speed = (demo_sin(stripe * STRIPES_SPREAD) * STRIPES_SPEED) >> DEMO_SHIFT;
```

**3. Acumular y dar la vuelta.** Aquí sí con `while` y no con `if`:

```c
	while (offset[stripe] < 0){
		offset[stripe] = offset[stripe] + DEMO_WIDTH;
	}
	while (offset[stripe] >= DEMO_WIDTH){
		offset[stripe] = offset[stripe] - DEMO_WIDTH;
	}
```

**4. Los dos `memcpy` de siempre**, ocho veces por banda.

## Las trampas

**Aquí el `while` sí hace falta.** En `scroll` bastaba un `if` porque el
desplazamiento crecía de uno en uno. Aquí crece de seis en seis y en las dos
direcciones, y después de cientos de frames no me fío. Un `if` que se quede
corto deja un desplazamiento fuera de rango y el `memcpy` se sale del buffer.

**`STRIPES_SPREAD` es primo (11).** Si fuera un divisor de 256, el patrón de
velocidades se repetiría cada pocas bandas y se vería la simetría.

## Coste

El mismo que `scroll`: dos `memcpy` por línea. El seno se calcula una vez por
banda, 25 por frame.

## Experimentos

1. Cambia el seno por `rand()`. Compara: pasa de ondulación a avería.
2. Pon `STRIPES_HEIGHT` a 1. Doscientas bandas de una línea. Es otro efecto
   completamente distinto y también muy de la época.
3. Haz que la velocidad cambie con el tiempo: suma `phase` dentro del
   `demo_sin()`. Las bandas se aceleran y frenan.

---

**Anterior:** [La persiana veneciana](../../blinds/doc/README.md) ·
**Siguiente:** [La imagen que ondula](../../wobble/doc/README.md) ·
**Índice:** [Los diez efectos](../../README.md)
