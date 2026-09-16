# cube — El cubo de alambre

*[English version](README-EN.md)*

**Efecto 19 de 20 del recorrido.** Coste medido: **4,7 microsegundos por frame**.

El código está en [`../cube.c`](../cube.c).

---

## Qué hace

Un cubo de alambre girando sobre dos ejes, por encima de la foto.

Es el primer efecto de la carpeta que es **3D de verdad**: ocho vértices, doce
aristas y una proyección en perspectiva, todo con enteros. La idea está sacada
del [doscube de root42](https://github.com/root42/doscube).

## Y fíjate en el coste

**4,7 microsegundos.** Es el segundo más barato de los quince, por detrás de
`cycle`, y está en el puesto 14 del recorrido.

Ahí se rompe la coincidencia que traían los trece anteriores, donde lo difícil
era lo caro. Un cubo de alambre no rellena nada: se calculan **ocho puntos** por
frame y se pintan doce rectas. Lo caro de un 3D no son las matemáticas, **es el
relleno** — y eso lo demuestra el efecto siguiente, que hace exactamente lo
mismo que este y cuesta diez veces más.

Por eso los efectos 3D de la época empezaban todos por aquí.

## La cadena de cuentas, de un vértice a un píxel

```
   1. el cubo, ocho vertices en +/-1                    (constante)
   2. girar en Y, luego en X                            (4 productos)
   3. alejarlo, sumando a la z                          (1 suma)
   4. dividir por la z: eso es la perspectiva           (2 divisiones)
   5. escalar y centrar en la pantalla                  (2 sumas)
```

Las divisiones son el único sitio donde este efecto divide, y son **16 por
frame**. Un rotozoom no divide ni una vez, pero toca 64.000 píxeles: son dos
formas opuestas de gastarse el presupuesto.

## Paso a paso

**1. El giro**, dos rotaciones seguidas. La de Y mezcla `x` y `z` y deja la `y`
en paz; la de X actúa sobre lo que salió de la anterior:

```c
	rotated_x = (((long)corner_x[index] * cos_y) +
	             ((long)corner_z[index] * sin_y)) >> DEMO_SHIFT;
```

Todo en `long` porque un producto de dos números 8.8 se sale de un `int` de 16
bits **antes** de bajarlo de nuevo.

**2. La perspectiva entera es dividir por la `z`:**

```c
	rotated_z += CUBE_DISTANCE;
	screen_x[index] = (int)((rotated_x * CUBE_FOCAL) / rotated_z) + (DEMO_WIDTH / 2);
```

Lo que está lejos tiene `z` grande y se encoge; lo que está cerca tiene `z`
pequeña y se agranda. Eso es todo.

**3. Las doce aristas, en una tabla de pares de índices**, para que el bucle de
pintar sean cuatro líneas y no doce llamadas a mano.

## Las trampas

**El píxel no es cuadrado.** El modo 13h son 320x200 estirados sobre una
pantalla de 4:3, o sea que cada píxel es un 20% más alto que ancho. Si proyectas
la `y` con el mismo número que la `x`, el «cubo» sale más alto que ancho y
parece una caja de zapatos de pie.

```c
#define CUBE_ASPECT_NUM 	5
#define CUBE_ASPECT_DEN 	6
```

5/6 es 0,833, que es justo 1/1,2. Es lo único de todo el efecto que **no sale de
la geometría: sale del monitor.**

**El color de las aristas no se puede elegir a dedo.** En un programa con la
paleta por defecto del modo 13h, el 15 es blanco y se acabó. Aquí no: cada foto
trae su propia paleta, así que el índice 15 de `demo03.bmp` es lo que le tocara
a esa foto, que igual es marrón.

Así que se busca. Los 768 bytes vienen en **BGR y de 0 a 63** — tal como los
deja `bmp_load_pallete_data()`, que divide por 4 — y se elige el índice cuya
suma de los tres canales sea mayor: el blanco de esa foto, sea cual sea su
número. Es la misma trampa que el radar del juego con `numbers.bmp`.

**La `z` nunca llega a cero**, y por eso no hay que recortar contra el plano de
la cámara. El cubo está a 4 y su vértice más cercano a 4 − √3 = 2,27. Sin ese
margen habría que recortar, que es bastante más trabajo que sumar una constante.

**Las coordenadas de la recta son `int`.** Un vértice se sale de la pantalla
constantemente, y un −9 metido en un `unsigned` son 65527.

## Coste

Un `memcpy` de 64.000 para poner la foto de fondo, y eso es **lo más caro del
efecto con diferencia**: cuesta más que los ocho vértices y las doce rectas
juntos.

Repintar solo por donde pasó el cubo el frame anterior se puede hacer, pero son
doce rectas que hay que recordar y volver a recorrer, y a un `memcpy` de 64.000
bytes no le gana.

## Experimentos

1. Quita las dos líneas del aspecto. El cubo se estira a lo alto y ya no parece
   un cubo: es la forma más rápida de ver que 320x200 no es cuadrado.
2. Pon `CUBE_FOCAL` a 600. La perspectiva se abre tantísimo que las caras
   cercanas se deforman como un gran angular.
3. Pon `CUBE_SPIN_X` y `CUBE_SPIN_Y` los dos a 2. El cubo gira siempre por la
   misma diagonal y se nota que se repite a los tres segundos.
4. Cambia `color = cube_brightest(palette)` por `color = 15`. En unas fotos se
   verá y en otras desaparecerá el cubo: eso es el problema de la paleta.

---

**Anterior:** [Volar sobre la foto](../../voxel/doc/README.md) ·
**Siguiente:** [El cubo con una foto en cada cara](../../texcube/doc/README.md) ·
**Índice:** [Los veinte efectos](../../README.md)
