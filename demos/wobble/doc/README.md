# wobble — La imagen que ondula

*[English version](README-EN.md)*

**Efecto 5 de 10 del recorrido.** Coste medido: **8,0 microsegundos por frame**.

El código está en [`../wobble.c`](../wobble.c).

---

## Qué hace

La imagen ondula como una bandera al viento, o como un reflejo en un charco.

## La idea

**Cada fila se desplaza a los lados según un seno**, y el seno avanza un poco
cada frame. Eso es todo.

```
   fila  0   ->|
   fila  8      ->|
   fila 16        ->|
   fila 24      ->|
   fila 32   ->|
   fila 40 <-|
```

Lo que hace que sea una onda y no un desorden es que el desfase depende de la
fila: `demo_sin(phase + y * WOBBLE_DENSITY)`. Filas vecinas, desplazamientos
parecidos.

## Paso a paso

**1. El desplazamiento de la fila:**

```c
	shift = (demo_sin(phase + (y * WOBBLE_DENSITY)) * WOBBLE_AMPLITUDE) >> DEMO_SHIFT;
```

`demo_sin()` va de −256 a 256, o sea que ya está en punto fijo 8.8:
multiplicarlo por la amplitud y bajar 8 bits da un número entre −24 y +24 sin
una sola división.

**2. Convertirlo en positivo:**

```c
	if (shift < 0){
		shift = shift + DEMO_WIDTH;
	}
```

Un desplazamiento de −5 es lo mismo que uno de +315 cuando la fila da la vuelta.

**3. Los dos `memcpy`**, igual que en `scroll`.

## Las trampas

**El `if` basta y el `while` sobra.** La amplitud es 24, así que el seno nunca
da menos de −24 y una sola suma lo mete en rango. Compáralo con `stripes`, donde
sí hace falta un `while`: la diferencia es que allí el desplazamiento se
**acumula** y aquí se calcula entero cada frame.

**`WOBBLE_DENSITY` decide cuántas ondas caben.** A 3 pasos de ángulo por fila,
200 filas son 600 pasos, o sea **algo más de dos vueltas completas**: dos ondas
y pico de arriba abajo.

## Coste

Un seno por **fila**, no por píxel: 200 consultas a la tabla por frame contra
las 64.000 operaciones de un rotozoom. Es de los efectos que más impresionan por
lo poco que cuestan.

## Experimentos

1. Sube `WOBBLE_AMPLITUDE` a 80. Sigue funcionando, y sigue bastando el `if`,
   porque 80 < 320.
2. Pon `WOBBLE_DENSITY` a 1: una sola onda muy larga, mucho más suave.
3. Cambia el `demo_sin(phase + y*D)` por `demo_sin(phase)`. Todas las filas se
   desplazan lo mismo y el efecto se convierte en un `scroll` horizontal que va
   y viene. Eso enseña que la ondulación **está en la dependencia de y**.

---

**Anterior:** [Bandas a distinta velocidad](../../stripes/doc/README.md) ·
**Siguiente:** [La foto que se pasea](../../bounce/doc/README.md) ·
**Índice:** [Los diez efectos](../../README.md)
