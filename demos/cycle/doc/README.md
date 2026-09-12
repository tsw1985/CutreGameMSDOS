# cycle — La paleta girando

*[English version](README-EN.md)*

**Efecto 1 de 10 del recorrido.** Coste medido: **1,6 microsegundos por frame**.

El código está en [`../cycle.c`](../cycle.c).

---

## Qué hace

La imagen se queda **completamente quieta** y los colores se mueven. Con una
foto normal sale un efecto psicodélico; con un dibujo hecho a propósito, con
los colores en rampa, parece agua corriendo o fuego ardiendo.

## La idea

Este es el efecto que hay que entender primero, porque enseña la pieza que
todos los demás dan por sabida: **en el modo 13h, un byte de la pantalla no es
un color, es un número de color.**

```
   memoria de video          la paleta (el DAC)
   [ 37 ][ 37 ][ 12 ]        37 -> (rojo 40, verde 12, azul 4)
                             12 -> (rojo  0, verde 60, azul 0)
```

Si cambias la entrada 37 de la paleta, **todos** los píxeles que valen 37
cambian de color a la vez, sin tocar ni un byte de la pantalla.

Así se hacían las cascadas que caían y los letreros que parpadeaban en juegos
que no podían permitirse redibujar nada.

## Paso a paso

**1. Pintar la imagen una sola vez.**

```c
	memcpy(screen, image, DEMO_SCREEN);
	demo_show(screen);
```

Y ya está. No se vuelve a tocar la pantalla en los ocho segundos que dura.

**2. Girar la paleta un puesto.** Se guarda la primera entrada, se arrastran
todas una posición hacia abajo, y la guardada se pone al final:

```
   antes:   [0][1][2][3] ... [254][255]
   despues: [0][2][3][4] ... [255][ 1 ]
                                    ^ la que era la 1
```

**3. Mandarla al DAC** con `bmp_write_pallete_data_into_dac()`.

**4. No girar cada frame.** A 70 fotogramas por segundo la vuelta entera
duraría menos de cuatro segundos y marearía. `CYCLE_EVERY 3` gira una de cada
tres.

## El código que importa

```c
	first = CYCLE_FIRST * CYCLE_ENTRY;
	last  = (CYCLE_FIRST + CYCLE_COUNT - 1) * CYCLE_ENTRY;

	saved[0] = palette[first + 0];
	saved[1] = palette[first + 1];
	saved[2] = palette[first + 2];

	for (i = first; i < last; i = i + CYCLE_ENTRY){
		palette[i + 0] = palette[i + CYCLE_ENTRY + 0];
		palette[i + 1] = palette[i + CYCLE_ENTRY + 1];
		palette[i + 2] = palette[i + CYCLE_ENTRY + 2];
	}

	palette[last + 0] = saved[0];
```

Es un `memmove` disfrazado, pero escrito a mano porque hay que mover de **tres
en tres bytes** (azul, verde, rojo) y guardar la entrada que se pisa.

## Las trampas

**El color 0 no gira.** `CYCLE_FIRST` vale 1 a propósito. El 0 es el negro del
fondo en todas las paletas de este proyecto; si girara, el fondo se pondría a
parpadear de colores y quedaría fatal.

**Este efecto no puede usar `demo_show()`.** Es el único. Los demás terminan el
frame con retrazo + volcado + sonido, pero éste no vuelca nada: lo único que
hace es escribir la paleta, **y esa escritura tiene que caer dentro del borrado
vertical**. Por eso hace:

```c
	demo_wait_retrace();
	/* ... escribir el DAC ... */
	demo_sound();
```

Si el sonido se mezclara antes, se comería la ventana de borrado y la paleta
entraría con el haz pintando. En DOSBox no se nota; en una VGA de verdad, sí.

**Devolver la paleta como estaba al salir con ESC.** El efecto deja la paleta
girada, y la imagen siguiente heredaría los colores a medio girar. El
reproductor lo tiene en cuenta: funde a negro con la paleta que haya **ahora**,
no con la que cargó.

## Coste

**El más barato de los diez, y por mucho.** Cero píxeles por frame. Solo las
1.024 escrituras al DAC, y ni siquiera todos los frames.

## Experimentos

1. Pon `CYCLE_EVERY` a 1. Marea.
2. Pon `CYCLE_FIRST` a 0 y mira lo que le pasa al fondo.
3. Gira solo un tramo pequeño, por ejemplo de la entrada 200 a la 230, y usa
   una imagen que tenga ahí una rampa de color. Eso es exactamente como se
   animaba el agua en los juegos de la época.
4. Prueba el efecto sobre `demo11.bmp`, que es un plasma con rampas continuas.
   Está puesta ahí para esto.

---

**Siguiente:** [El desplazamiento infinito](../../scroll/doc/README.md) ·
**Índice:** [Los diez efectos](../../README.md)
