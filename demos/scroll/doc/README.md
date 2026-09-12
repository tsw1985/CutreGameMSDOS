# scroll — El desplazamiento infinito

*[English version](README-EN.md)*

**Efecto 2 de 10 del recorrido.** Coste medido: **7,6 microsegundos por frame**.

El código está en [`../scroll.c`](../scroll.c).

---

## Qué hace

La imagen se desplaza en diagonal sin parar, y lo que se sale por un lado entra
por el otro. No hay principio ni final: se comporta como si estuviera pegada a
un cilindro en las dos direcciones.

## La idea

Aquí se aprende la técnica que usan **la mitad de los efectos de esta
colección**: mover una fila entera con `memcpy` en vez de píxel a píxel.

Una fila desplazada es la misma fila partida en dos trozos que cambian de sitio:

```
   imagen:    [ A A A A | B B B B B B B ]
   pantalla:  [ B B B B B B B | A A A A ]
                              ^ el corte esta en 320 - desplazamiento
```

Dos `memcpy` y la fila está hecha. En un 8086 un `memcpy` es un `REP MOVSW`:
mueve dos bytes por instrucción y no pasa por el bucle de C ni una vez.

## Paso a paso

**1. Dos contadores**, uno horizontal y otro vertical, que avanzan y dan la
vuelta al llegar al borde.

**2. Para cada fila de pantalla, decidir de qué fila de la imagen viene:**

```c
	source_y = y + offset_y;
	if (source_y >= DEMO_HEIGHT){
		source_y = source_y - DEMO_HEIGHT;
	}
```

Un `if` y no un `%`: la resta es una instrucción y el módulo es una división.

**3. Los dos `memcpy`:**

```c
	memcpy(screen + destination_row + offset_x,
	       image + source_row,
	       DEMO_WIDTH - offset_x);

	memcpy(screen + destination_row,
	       image + source_row + (DEMO_WIDTH - offset_x),
	       offset_x);
```

## Las trampas

**El desplazamiento tiene que estar entre 0 y 319.** Si vale 320, el primer
`memcpy` copia 0 bytes y el segundo copia 320 desde `image + row + 0`... que
casualmente funciona, pero con 321 se sale del buffer. Por eso el contador se
vuelve a 0 **antes** de usarse, no después.

**Un `memcpy` de 0 bytes es legal** y no hace nada, que es justo lo que hace
falta cuando el desplazamiento es 0.

**Las dos velocidades no son iguales.** El vertical avanza uno de cada
`SCROLL_Y_EVERY` frames. Si las dos fueran iguales el recorrido sería una
diagonal perfecta que se repite enseguida y se nota el bucle.

## Coste

De los más baratos: 400 `memcpy` por frame y nada más. Ni una operación por
píxel.

## Experimentos

1. Pon `SCROLL_Y_EVERY` a 1 y mira cómo se nota que el recorrido se repite.
2. Quita el segundo `memcpy`. Verás el hueco negro que deja lo que se sale.
3. Cambia el `if` de la vuelta por un `while`. No cambia nada, y ese es el
   punto: con estas velocidades nunca hace falta más de una vuelta.

---

**Anterior:** [La paleta girando](../../cycle/doc/README.md) ·
**Siguiente:** [La persiana veneciana](../../blinds/doc/README.md) ·
**Índice:** [Los diez efectos](../../README.md)
