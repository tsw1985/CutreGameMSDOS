# blinds — La persiana veneciana

*[English version](README-EN.md)*

**Efecto 3 de 10 del recorrido.** Coste medido: **7,6 microsegundos por frame**.

El código está en [`../blinds.c`](../blinds.c).

---

## Qué hace

La pantalla se parte en veinte tiras horizontales y cada una se abre desde su
centro hasta descubrir la imagen, y luego se cierra. Es la transición de los
programas de diapositivas de la época.

## La idea

Cada tira es una **ventana que crece desde el medio**. Lo que está dentro de la
ventana es la imagen; lo de fuera, negro:

```
   cerrada:   [################|################]
   a medias:  [########|IMAGEN IMAGEN|##########]
   abierta:   [IMAGEN IMAGEN IMAGEN IMAGEN IMAGEN]
```

Y lo que le da el aire de persiana de verdad, en vez de parecer un telón, es que
**cada tira va un poco por detrás de la de arriba**.

## Paso a paso

**1. Cuánto está abierta esta tira.** Se saca de un seno, y se le suma un
desfase que depende del número de tira:

```c
	half = (((demo_sin(phase + (slat * BLINDS_STAGGER)) + DEMO_ONE)
	         * (DEMO_WIDTH / 2)) / (DEMO_ONE * 2));
```

El `+ DEMO_ONE` mueve el seno de −256..256 a 0..512, y la división lo lleva a
0..160, que es media pantalla.

**2. Acotar.** El redondeo puede dejar `half` un pelo fuera de rango, y de ahí
saldría un `memset` de tamaño negativo, que en `size_t` es un número enorme.

**3. Pintar cada línea de la tira** con dos `memset` (los bordes negros) y un
`memcpy` (el centro abierto).

## El código que importa

```c
	memset(screen + row, 0, (DEMO_WIDTH / 2) - half);
	memset(screen + row + (DEMO_WIDTH / 2) + half, 0, (DEMO_WIDTH / 2) - half);

	memcpy(screen + row + (DEMO_WIDTH / 2) - half,
	       image  + row + (DEMO_WIDTH / 2) - half,
	       open);
```

Fíjate en que el `memcpy` copia **de la misma posición** en la imagen y en la
pantalla. La tira no desplaza nada, solo tapa.

## Las trampas

**El `BLINDS_SLAT` tiene que dividir a 200.** Vale 10, así que salen 20 tiras
exactas. Con 11 sobrarían 2 líneas al final que no se pintarían nunca y se
quedarían con lo que hubiera antes.

**Acotar `half` a 0..160 no es paranoia.** Es la única defensa contra un
`memset` de longitud negativa, y ese fallo no da un error: escribe medio
megabyte de ceros por encima de lo que pille.

## Coste

Barato: 200 líneas × (2 `memset` + 1 `memcpy`). El cálculo del seno se hace una
vez por **tira**, no por línea: 20 senos por frame.

## Experimentos

1. Pon `BLINDS_STAGGER` a 0. Todas las tiras se abren a la vez y deja de
   parecer una persiana: parece un telón.
2. Sube `BLINDS_STAGGER` a 20. La ola da más de una vuelta a lo largo de la
   pantalla.
3. Cambia `BLINDS_SLAT` a 4. Cuarenta tiras finas, mucho más nervioso.

---

**Anterior:** [El desplazamiento infinito](../../scroll/doc/README.md) ·
**Siguiente:** [Bandas a distinta velocidad](../../stripes/doc/README.md) ·
**Índice:** [Los diez efectos](../../README.md)
