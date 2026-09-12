# bounce — La foto que se pasea

*[English version](README-EN.md)*

**Efecto 6 de 10 del recorrido.** Coste medido: **8,1 microsegundos por frame**.

El código está en [`../bounce.c`](../bounce.c).

---

## Qué hace

La foto entera se pasea por la pantalla sobre un fondo negro, dibujando una
figura de Lissajous: un seno para la horizontal y otro para la vertical, con
velocidades distintas.

## La idea

Este es el efecto que enseña **el recorte con signo**, que es la trampa que más
veces ha mordido en este proyecto entero.

La imagen se desplaza `(dx, dy)`. La parte que se sale hay que no copiarla, y la
parte que queda hay que copiarla desde el sitio correcto. Suena trivial y no lo
es.

```
   dx positivo:              dx negativo:
   [   |IMAGEN------->]      [<-------IMAGEN|   ]
    ^ negro                                  ^ negro
    empieza en dx            empieza en 0
    copia 320-dx             copia 320+dx  (dx es negativo)
    desde la columna 0       desde la columna -dx
```

## Paso a paso

**1. Los dos desplazamientos, de dos senos con velocidades primas entre sí:**

```c
	offset_x = (demo_sin(angle_x) * BOUNCE_RANGE_X) >> DEMO_SHIFT;
	offset_y = (demo_cos(angle_y) * BOUNCE_RANGE_Y) >> DEMO_SHIFT;
```

3 y 5 no tienen divisores comunes, así que el recorrido tarda muchísimo en
cerrarse y no parece un bucle.

**2. Fondo negro entero**, de una vez: `memset(screen, 0, DEMO_SCREEN)`.

**3. Descartar las filas que no caen:**

```c
	source_y = y - offset_y;
	if (source_y < 0 || source_y >= DEMO_HEIGHT){
		continue;
	}
```

**4. El recorte horizontal**, que es el código que hay que mirar:

```c
	if (offset_x >= 0){
		destination_x = offset_x;
		source_x      = 0;
		copy_width    = DEMO_WIDTH - offset_x;
	}else{
		destination_x = 0;
		source_x      = -offset_x;
		copy_width    = DEMO_WIDTH + offset_x;
	}

	if (copy_width <= 0){
		continue;
	}
```

## Las trampas

**Todo esto tiene que ser `int`, no `unsigned`.** Es la misma trampa que el
recorte de sprites del juego y que la resta del radar: un `-9` metido en un
`unsigned` es 65527, y un `memcpy` de 65527 bytes escribe medio buffer de
pantalla en memoria que no es suya. En DOS eso no es un error, es corrupción
silenciosa que se manifiesta veinte segundos después.

**El `if (copy_width <= 0)` no sobra.** Con la amplitud actual la imagen nunca
se sale del todo, pero sube `BOUNCE_RANGE_X` a 400 y sin esa línea tienes un
`memcpy` de longitud negativa.

## Coste

Un `memset` grande y hasta 200 `memcpy`. Prácticamente lo mismo que `scroll`.

## Experimentos

1. Quita el `(int)` de las variables de desplazamiento y hazlas `unsigned int`.
   Prepárate para ver la pantalla llena de basura, y entenderás por qué el juego
   entero insiste tanto en esto.
2. Pon `BOUNCE_SPEED_X` y `BOUNCE_SPEED_Y` los dos a 4. El recorrido se cierra
   en una diagonal y se nota que se repite.
3. Sube los dos rangos a 200. Ahí verás para qué está el `copy_width <= 0`.

---

**Anterior:** [La imagen que ondula](../../wobble/doc/README.md) ·
**Siguiente:** [La gota en el estanque](../../ripple/doc/README.md) ·
**Índice:** [Los diez efectos](../../README.md)
