# twister — La columna retorcida

*[English version](README-EN.md)*

**Efecto 11 de 20 del recorrido.** Coste medido: **35 microsegundos por frame**.

El código está en [`../twister.c`](../twister.c).

---

## Qué hace

Una columna vertical de cuatro caras gira sobre sí misma, y además está
**retorcida**: la parte de arriba no mira donde mira la de abajo. La foto va
pegada a las cuatro caras, así que se ve pasar entera según gira.

Es de los efectos más reconocibles de la época — el de *Second Reality* — y de
los más baratos que hay.

## La idea

Es un efecto 3D que **no tiene 3D**, y ese es todo el truco.

Un cubo de verdad hay que rotarlo, proyectarlo y rellenarlo. Una columna
vertical no, porque por mucho que gire **un punto nunca cambia de fila**. Una
fila de la pantalla solo puede enseñar lo que hay a esa altura de la columna.

O sea que el problema entero se cae a una dimensión:

```
   vista desde arriba, a la altura de la fila y:

        esquina 1
           /\            las cuatro esquinas estan a un cuarto
   esq 0  /  \  esq 2    de vuelta una de otra: 64 de los 256
          \  /           pasos de angulo
           \/
        esquina 3

   en pantalla solo importa su x:  cx + sin(angulo) * radio
```

No hay matriz, no hay `z`, no hay división por profundidad. Cuatro senos y un
relleno de tramos por fila.

## Paso a paso

**1. El ángulo de esta fila**, y aquí está el retorcido entero:

```c
	angle = phase + (y * TWISTER_TWIST);
```

Si le quitas el `y *`, las cuatro esquinas caen en la misma `x` en todas las
filas y lo que queda es una caja girando. Ese producto por la fila **es** el
efecto.

**2. Las cuatro esquinas**, más una quinta que es la primera otra vez para que
el bucle de caras no tenga que preguntar si ha dado la vuelta:

```c
	for (corner = 0; corner < 5; corner++){
		corner_x[corner] = (DEMO_WIDTH / 2)
		                 + ((demo_sin(angle + (corner * 64)) * TWISTER_RADIUS) >> DEMO_SHIFT);
	}
```

**3. La cara que mira para otro lado no se pinta**, y el test son dos
caracteres:

```c
	if (left >= right){
		continue;
	}
```

Al girar la columna las esquinas se mueven en pantalla y las caras se dan la
vuelta. Una cara cuya esquina izquierda ha adelantado a la derecha es una cara
que estamos viendo por detrás.

Y como las cuatro se recorren **en orden alrededor de la columna**, las que
pasan el test nunca se solapan: no hace falta ordenar por profundidad ni llevar
un buffer de `z`. Es el mismo argumento que usa [texcube](../../texcube/doc/README.md)
con su producto vectorial, en una dimensión menos.

**4. La foto, estirada sobre la cara.** Cada cara se lleva un cuarto del ancho
de la foto (320 / 4 = 80), y ese cuarto hay que meterlo en los `span` píxeles
que ocupa la cara ahora mismo:

```c
	texture_step = ((long)TWISTER_FACE_WIDTH << DEMO_SHIFT) / (long)span;
	texture_u    = (long)(face * TWISTER_FACE_WIDTH) << DEMO_SHIFT;
```

Una división por cara y por fila — 800 como mucho por frame — y dentro del bucle
de píxeles solo queda una suma.

## Las trampas

**El paso de textura nunca se pasa del borde, y es por el truncamiento.** Tras
`span - 1` sumas, `texture_u` vale como mucho

```
	face*80 + (80 * (span-1) / span)   <   (face+1)*80
```

estrictamente menor porque la división entera redondea hacia abajo. Por eso la
última cara nunca llega a leer la columna 320, que en la última fila sería el
byte 64000 — uno fuera del buffer. Si alguna vez cambias esa división por una
que redondee, este efecto empieza a leer fuera.

**El recorte es con `int`.** Con el radio actual la columna no se sale de la
pantalla, pero sube `TWISTER_RADIUS` y una `x` negativa metida en un `unsigned`
son 65.000 y pico. Y al recortar por la izquierda hay que **adelantar también la
coordenada de textura**, o la cara se estira:

```c
	if (left < 0){
		texture_u += texture_step * (long)(-left);
		left = 0;
	}
```

## Coste

Un `memset` de 64.000 más unos 35.000 píxeles con textura. La mitad de la
pantalla queda en negro casi siempre, y eso es lo que lo hace barato: un
[carpet](../../carpet/doc/README.md) toca los 64.000 todos los frames.

## Experimentos

1. Pon `TWISTER_TWIST` a 0. Desaparece el retorcido y se queda una caja girando:
   ahí ves de qué iba el efecto.
2. Súbelo a 3. La columna da casi tres vueltas de arriba abajo y se convierte en
   un muelle.
3. Quita el `if (left >= right) continue;`. Se pintan las cuatro caras, las de
   detrás encima de las de delante, y la columna se vuelve transparente y
   confusa. Eso es lo que te ahorra el descarte.
4. Sube `TWISTER_RADIUS` a 200 y quita el recorte de `left`. Cuelgue o basura en
   pantalla, según el día.

---

**Anterior:** [El pixelado que respira](../../mosaic/doc/README.md) ·
**Siguiente:** [Acercarse y alejarse](../../zoom/doc/README.md) ·
**Índice:** [Los veinte efectos](../../README.md)
