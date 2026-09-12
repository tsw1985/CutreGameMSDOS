# mosaic — El pixelado que respira

*[English version](README-EN.md)*

**Efecto 8 de 10 del recorrido.** Coste medido: **19,6 microsegundos por frame**, y antes de optimizarlo eran 78.

El código está en [`../mosaic.c`](../mosaic.c).

---

## Qué hace

La imagen se rompe en cuadrados enormes en los que no se distingue nada, y va
bajando hasta el píxel de verdad. Luego vuelve a subir.

## La idea

Cada bloque toma el color del píxel que hay en **su esquina de arriba a la
izquierda**. Ni medias, ni promedios, ni nada que cueste: se coge un color y se
rellena.

```
   imagen           bloque de 4
   a b c d          a a a a
   e f g h    ->    a a a a
   i j k l          a a a a
   m n o p          a a a a
```

Y el relleno es un `memset` por fila de bloque, no un píxel cada vez.

## Paso a paso

**1. El tamaño del bloque de este frame**, de un seno llevado a 1..40:

```c
	block = 1 + (((demo_sin(phase) + DEMO_ONE) * (MOSAIC_MAX_BLOCK - 1)) / (DEMO_ONE * 2));
```

**2. Recorrer la pantalla de bloque en bloque**, recortando el último de cada
fila y columna, que casi nunca cabe entero.

**3. Rellenar cada bloque** con un `memset` por cada una de sus filas.

## La optimización, que es la mitad de este documento

La primera versión costaba **78 microsegundos por frame**, once veces más que
los efectos baratos. Ahora cuesta **19,6**. El cambio son seis líneas:

```c
	if (block == last_block){
		demo_show(screen);
		phase = (phase + MOSAIC_SPEED) & DEMO_ANGLE_MASK;
		continue;
	}
```

**Si el bloque mide lo mismo que en el frame anterior, el dibujo sería idéntico
y ya está en `screen`.** No hay nada que hacer.

Y la cuenta es demoledora: el tamaño solo cambia unas 40 veces en los ocho
segundos que dura el efecto, y a 70 fotogramas por segundo eso son 560 frames.
**El 93% de ellos estaba repintando píxel a píxel un resultado que ya tenía
delante.**

Y un caso más:

```c
	if (block == 1){
		memcpy(screen, image, DEMO_SCREEN);
		...
	}
```

Con bloques de un píxel no hay mosaico que valga: es la imagen tal cual, y un
`memcpy` hace el trabajo de 64.000 `memset` de un byte.

## Las trampas

**`block` nunca puede ser 0.** El `1 +` del cálculo está para eso: un bloque de
tamaño 0 es un `for (x = 0; x < 320; x += 0)`, o sea un bucle infinito con la
pantalla congelada.

**`last_block` empieza en −1**, no en 0. Tiene que ser un valor que no coincida
con ningún tamaño posible, para que el primer frame se pinte siempre.

**40 divide a 320 y a 200.** Con el bloque máximo la pantalla queda en cuadros
exactos. Con 41 sobrarían restos raros en el borde.

## Coste

19,6 µs/frame de media, contra los 78 de antes. Los frames en los que el bloque
cambia siguen costando lo mismo; lo que ha desaparecido son los otros 520.

## Experimentos

1. Quita el `if (block == last_block)` y mide. Es la lección entera en un
   `diff`.
2. Sube `MOSAIC_MAX_BLOCK` a 100. Se ve un frame con cuatro cuadrados.
3. Haz que el bloque no sea cuadrado: usa uno para el ancho y otro distinto para
   el alto.

---

**Anterior:** [La gota en el estanque](../../ripple/doc/README.md) ·
**Siguiente:** [Acercarse y alejarse](../../zoom/doc/README.md) ·
**Índice:** [Los diez efectos](../../README.md)
