# carpet — El rotozoom que no se acaba nunca

*[English version](README-EN.md)*

**Efecto 15 de 20 del recorrido.** Coste medido: **73 microsegundos por frame**.

El código está en [`../carpet.c`](../carpet.c).

---

## Qué hace

Un suelo infinito de fotos repetidas que gira, se acerca y se pasea. Sin bordes,
sin fondo negro y llenando los 64.000 píxeles siempre.

Es el efecto de «alfombra» de las intros, y la idea está sacada del
[rotozoom de root42](https://github.com/root42/rotozoom).

## La idea

Es el [rotozoom](../../rotozoom/doc/README.md) y la diferencia cabe en una línea.
Donde aquel pregunta *«¿me he salido de la imagen?»* y pinta negro, este hace
`& 127` y vuelve a entrar por el otro lado.

```
   rotozoom                       carpet
   +-----------------+            +-----------------+
   |     .-----.     |            |-.  |  .-----.  |-|
   |    /       \    |            |  \ | /       \ | |
   |   |  FOTO   |   |            |---FOTO---FOTO--|-|
   |    \       /    |            |  / | \       / | |
   |     '-----'     |            |-'  |  '-----'  |-|
   +-----------------+            +-----------------+
    una copia, negro              se repite hasta el infinito
    alrededor                     (y se ve la costura)
```

## Por qué hace falta una textura y no vale la foto

Para dar la vuelta con un AND, el ancho tiene que ser **potencia de dos**. La
foto es de 320x200 y ninguno de los dos números lo es, así que envolver con ella
costaría dos divisiones por píxel: **128.000 divisiones por frame**, que en un
386 es no arrancar.

Por eso al entrar se encoge la foto a 128x128 con
[`demo_shrink_to_texture()`](../../demos.c), y a partir de ahí el bucle interior
no tiene ni una división ni una comparación.

16.384 bytes y un encogido de una vez, a cambio de quitar la división del sitio
por el que se pasa 64.000 veces por frame.

## Paso a paso

**1. Los cuatro incrementos**, idénticos a los del rotozoom: la cuenta es lineal,
así que avanzar un píxel a la derecha suma siempre lo mismo a `u` y a `v`.

**2. El centro se pasea**, que es lo que convierte «una foto girando» en «un
suelo por el que se vuela». Dos senos a distinta velocidad, una figura de
Lissajous igual que en [bounce](../../bounce/doc/README.md).

**3. Y el bucle interior, que no tiene ni un `if`:**

```c
	source = (unsigned int)((v >> 1) & 0x3F80L)
	       + (unsigned int)((u >> DEMO_SHIFT) & DEMO_TEXTURE_MASK);

	screen[destination + x] = texture[source];
```

Ni recorte, ni tramo visible, ni `memset` de los bordes. Todos los píxeles de la
pantalla caen dentro de la textura porque el AND se encarga de que caigan. El
rotozoom se gasta media función en calcular dónde empieza y acaba lo visible de
cada fila; aquí esa media función sobra entera.

## Las trampas

**El sesgo, y es la única de verdad.** El AND que da la vuelta solo funciona con
números positivos, y `u` y `v` se van a negativo constantemente: la esquina de
arriba a la izquierda de la pantalla cae a la izquierda del centro de la textura
casi siempre.

En C89, **lo que hace un `>>` con un número negativo lo decide el compilador**.
Turbo C desplaza con signo y saldría bien, `gcc` también, y aun así esto es
exactamente la clase de cosa que funciona en el banco de Linux y hace otra cosa
en el hierro. Así que no se le pregunta a nadie:

```c
#define CARPET_BIAS 	((long)DEMO_TEXTURE_SIZE * (long)DEMO_ONE * 64L)
```

Y tiene que ser un **múltiplo exacto** del tamaño de la textura en punto fijo
(128 * 256 = 32.768), o desplazaría la alfombra medio azulejo. 2.097.152 es
32.768 × 64 justo.

**El desplazamiento de menos.** La fila de textura se calcula con dos
operaciones en vez de tres:

```
	((v >> 8) & 127) << 7   ==   (v >> 1) & 0x3F80
```

Bajar 8 y subir 7 es bajar 1, y los siete bits de abajo que eso deja sucios los
limpia la misma máscara si se corre siete sitios. Comprobado sobre los 85.715
valores del rango real: cero diferencias. Un desplazamiento menos por píxel, y
son 64.000 píxeles. Quitó un 24% del coste del efecto.

Y **solo vale porque `v` es siempre positivo**, que es lo que compra el sesgo.

## El precio: se ve la costura

La foto no encaja consigo misma, así que donde se repite se ve el corte. Eso no
es un fallo: es lo que hace este efecto y lo que hacía el original. Lo que se
enseña es un mosaico, y un mosaico tiene juntas.

## Coste

El más caro de la carpeta después del [túnel](../../tunnel/doc/README.md), y por
un motivo sano: es el único junto con aquel que **escribe los 64.000 píxeles
todos los frames**. El rotozoom parece más gordo y se ahorra la mayoría de la
pantalla con `memset`.

Si se queda corto en tu máquina, el original de root42 renderizaba a 320x100 y
doblaba las filas. Se pierde la mitad del detalle vertical y se gana la mitad
del tiempo.

## Experimentos

1. Cambia los dos `&  DEMO_TEXTURE_MASK` por nada. Verás leer fuera de la
   textura: colores de la memoria que hay detrás.
2. Pon `CARPET_BIAS` a 0. En `gcc` no notarás nada; es justo el tipo de cosa que
   se rompe en el hierro y no en el banco.
3. Cámbialo a `2097152 + 1`. La alfombra se desplaza un píxel por vuelta y
   entiendes lo de «múltiplo exacto».
4. Pon `CARPET_SCALE_AMP` a `DEMO_ONE * 2`. Se ven tantas baldosas que la costura
   pasa a ser el efecto.

---

**Anterior:** [Girar y hacer zoom a la vez](../../rotozoom/doc/README.md) ·
**Siguiente:** [El túnel infinito](../../tunnel/doc/README.md) ·
**Índice:** [Los veinte efectos](../../README.md)
