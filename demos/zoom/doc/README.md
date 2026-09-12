# zoom — Acercarse y alejarse

*[English version](README-EN.md)*

**Efecto 9 de 10 del recorrido.** Coste medido: **110 microsegundos por frame**, y antes de optimizarlo eran 144.

El código está en [`../zoom.c`](../zoom.c).

---

## Qué hace

La imagen se acerca y se aleja respirando, centrada, con negro alrededor cuando
está lejos.

## La idea

Aquí cambia la técnica. Hasta ahora todos los efectos movían **filas enteras**
con `memcpy`. Un zoom no puede: hay que **estirar o encoger** la fila, o sea
decidir para cada píxel de pantalla de qué píxel de la imagen viene.

Y la forma de pensarlo es al revés de lo que uno cree. No se coge la imagen y se
escala: **se recorre la pantalla** y para cada píxel se pregunta *"¿de dónde
vengo yo?"*. Así no quedan agujeros: cada píxel de la pantalla se rellena
exactamente una vez.

```
   escala 2.0 (alejado)          escala 0.5 (acercado)
   pantalla:  0 1 2 3 4 5        pantalla:  0 1 2 3 4 5
   imagen:    0 2 4 6 8 10       imagen:    0 0 1 1 2 2
              ^ se salta          ^ se repite
```

## Paso a paso

**1. La escala del frame**, de un seno, entre 0,6 y 2,2 aumentos.

**2. Dónde cae la esquina de arriba a la izquierda.** Se parte del centro de la
imagen y se retrocede media pantalla escalada, para que el zoom sea alrededor
del centro y no de la esquina:

```c
	start_u = ((long)(DEMO_WIDTH / 2) << DEMO_SHIFT) - (step * (DEMO_WIDTH / 2));
```

**3. Recorrer la pantalla** sumando `step` a la coordenada de origen.

## La optimización: la tabla de columnas

Y aquí está lo que hay que aprenderse.

**En un zoom sin giro, la columna de origen depende SOLO de la x.** La fila 0 y
la fila 199 leen exactamente las mismas 320 columnas. La primera versión hacía
esa cuenta 64.000 veces por frame **cuando hay 320 respuestas distintas**.

```c
	u = start_u;
	for (x = 0; x < DEMO_WIDTH; x++){
		sx = (int)(u >> DEMO_SHIFT);
		if ((unsigned int)sx < DEMO_WIDTH){
			column[x] = (unsigned int)sx;
			if (x < first){ first = x; }
			last = x;
		}
		u += step;
	}
```

320 cuentas al principio del frame, y el bucle interior se queda en:

```c
	for (x = first; x <= last; x++){
		screen[destination + x] = image[source_row + column[x]];
	}
```

Ni una suma larga, ni un desplazamiento, ni una comparación de límites. **De 144
microsegundos por frame a 110.**

Y de paso salen gratis `first` y `last`, o sea dónde empieza y acaba la parte
visible, así que los bordes negros se rellenan con `memset` en vez de píxel a
píxel.

## Las trampas

**La comparación es en `unsigned`** y hace el trabajo de dos:

```c
	if ((unsigned int)sx < DEMO_WIDTH)
```

Un `sx` negativo, visto como `unsigned`, es un número enorme y falla el "menor
que 320" de una vez. Es el truco de siempre para recortar con una sola
comparación en vez de dos.

**`step` nunca puede ser 0**, de ahí el `if (scale < 32)`. Con paso 0 todas las
columnas leerían el mismo píxel: no se colgaría, pero saldría una pantalla de un
solo color.

**Una fila entera fuera de la imagen se pinta con un `memset`**, sin mirar
columna por columna. Cuando la imagen está lejos, la mayoría de las filas son
estas.

## Coste

110 µs/frame, contra 144 antes de la tabla. Sigue siendo el segundo más caro de
los diez: es inevitable, porque de verdad toca los 64.000 píxeles.

## Experimentos

1. Quita la tabla y vuelve a calcular `u >> 8` dentro del bucle. Mide.
2. Pon `ZOOM_SCALE_AMP` a 0 y `ZOOM_SCALE_MID` a `DEMO_ONE`. La imagen sale
   idéntica al original. Si no sale idéntica, hay un error de medio píxel en
   alguna parte.
3. Pon `ZOOM_SCALE_MID` a `DEMO_ONE * 4`. Verás la imagen diminuta en el centro
   y comprobarás lo rápido que va cuando casi todo son `memset`.

---

**Anterior:** [El pixelado que respira](../../mosaic/doc/README.md) ·
**Siguiente:** [Girar y hacer zoom a la vez](../../rotozoom/doc/README.md) ·
**Índice:** [Los diez efectos](../../README.md)
