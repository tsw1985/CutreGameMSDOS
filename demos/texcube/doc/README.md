# texcube — El cubo con una foto en cada cara

*[English version](README-EN.md)*

**Efecto 19 de 19 del recorrido.** Coste medido: **50 microsegundos por frame**,
más el tiempo de cargar seis BMP y construir cinco tablas de color al entrar.

El código está en [`../texcube.c`](../texcube.c).

---

## Qué hace

El cubo de siempre, pero **relleno**: seis caras y en cada una una de las fotos
de la presentación, girando en tres dimensiones sobre los tres ejes.

Es el efecto que todo el mundo recuerda de las intros y, con diferencia, el más
largo de esta carpeta. Por tres problemas que no tiene ningún otro.

---

## Problema 1: seis fotos y una sola paleta

Y es el peor de los tres, aunque no lo parezca.

El VGA en modo 13h tiene **un** juego de 256 colores. Cada BMP trae el suyo.
Medido sobre `res/demo/`:

```
   demo01 contra demo02 .............. 1 entrada igual de 256
   media de las 15 dos a dos ......... 2,3 entradas iguales de 256
```

O sea que pegar la foto 2 en una cara con la paleta de la foto 1 puesta no da
«una foto con los colores un poco raros»: da **ruido de colores al azar**.

Es exactamente la misma trampa que el radar del juego, donde `numbers.bmp` y
`big.bmp` difieren en 254 de las 256 entradas y por eso el radar no existe sin
tema.

**La salida es la que usaban las demos de la época: remapear.** Se queda la
paleta de la foto que toca en la presentación, y de las otras cinco se traduce
cada uno de sus 256 colores al índice **más parecido** de esa paleta.

```c
	distance = texcube_square[db] + texcube_square[dg] + texcube_square[dr];
	if (distance < best_distance){
		best_distance = distance;
		best_index    = target;
	}
```

Fuerza bruta: 65.536 comparaciones por foto, cinco fotos, **327.680 distancias**
al entrar. Los canales van de 0 a 63, así que una tabla de 64 cuadrados cambia
los tres productos por tres lecturas — y en un 386 un producto de 16 bits cuesta
lo que quince lecturas.

**Lo que se ve:** las caras salen algo lavadas si las paletas no se parecen, y
se reconocen perfectamente. La calidad depende de la foto que mande: una foto
con color reparte bien, y una monocroma tiñe las otras cinco de su color. Es el
límite del formato, no del código.

---

## Problema 2: la perspectiva deforma las texturas

Rellenar un cuadrilátero interpolando la textura en línea recta — lo que se
llama **mapeado afín** — está mal en cuanto hay perspectiva: la parte de la cara
que está lejos tiene que comprimirse más deprisa de lo que una recta comprime.

Se nota como un pliegue por la diagonal de la cara, que es el defecto por el que
se reconocen los juegos de la primera PlayStation.

Lo correcto es dividir por la profundidad en **cada píxel**, y eso son 25.000
divisiones por frame que no caben. Lo que se hace aquí es lo que hacían ellos:

```
   cara sin partir            cara en rejilla de 4x4
   +-------------+            +--+--+--+--+
   |            /|            +--+--+--+--+   25 puntos proyectados
   |          /  |            +--+--+--+--+   CON perspectiva, y dentro
   |  pliegue    |            +--+--+--+--+   de cada trocito el error
   +-------------+            +--+--+--+--+   ya no se ve
```

**150 divisiones por frame en vez de 25.000**, y el pliegue desaparece.

Y los 25 puntos de la rejilla **no hay que rotarlos**: rotar es una operación
lineal, así que interpolar entre las esquinas ya rotadas da exactamente lo mismo
que rotar el punto interpolado. Se rotan 8 vértices y el resto sale de sumas.

---

## Problema 3: hay que rellenar

El [cubo de alambre](../../cube/doc/README.md) pinta doce rectas y cuesta 4,7
microsegundos. Este pinta unos 25.000 píxeles con textura y cuesta 50: **diez
veces más para la misma geometría.** Ahí está la demostración de que lo caro de
un 3D es el relleno.

Por eso hay un rasterizador de triángulos de verdad, que es la pieza más seria
de toda la carpeta.

**Los gradientes** son lo que hace barato el bucle interior. En un mapeado afín,
cuánto avanza `u` al moverse un píxel a la derecha es **el mismo número en todo
el triángulo**: no depende de la fila. Se calcula una vez con el determinante de
los tres vértices y dentro ya solo hay sumas:

```c
	du_dx = (((long)(pu[1] - pu[0]) * (long)(py[2] - py[0]))
	      -  ((long)(pu[2] - pu[0]) * (long)(py[1] - py[0]))) / area;
```

Cuatro divisiones por triángulo y ni una por píxel.

**Las caras que miran para atrás no se pintan**, con el producto vectorial en 2D
de dos lados seguidos de la cara ya proyectada. Si sale de un signo la vemos de
frente, y si sale del otro por detrás. Con eso **no hace falta ordenar las caras
ni llevar un buffer de profundidad**: en un cuerpo convexo, las caras que pasan
la prueba no se tapan entre ellas nunca. En una pose cualquiera pasan
exactamente 3 de las 6.

---

## Las trampas

**No hay que saltarse las filas de fuera con un `continue` a secas.** Los bordes
del triángulo se recorren sumando una pendiente por fila; si al salir de la
pantalla dejas de sumarla, cuando el triángulo vuelve a entrar los bordes están
donde los dejó la última fila visible y sale torcido. Hay que **avanzar siempre
y dibujar solo a veces**.

**La coordenada del último punto de la rejilla es 127 y no 128.** El
rasterizador enmascara con `& 127`, así que un 128 volvería al 0 y se vería una
raya del borde opuesto pegada a la arista:

```c
	step_u = ((DEMO_TEXTURE_SIZE - 1) << DEMO_SHIFT) / TEXCUBE_GRID;
```

**Las seis fotos se leen fila a fila**, sin pasar por un buffer de 64.000. Es
más lento (128 `fseek` por foto en vez de uno) y a cambio no pide ni un byte
grande justo en el momento en el que ya hay 98.304 de texturas pedidos. Y como
el BMP guarda la última fila primero, la fila de fichero es `199 - fila`, y así
no hace falta `bmp_revert_bmp()` ni tener las dos versiones en memoria a la vez.

**Los bloques pequeños se piden después de los grandes.** Si se pidieran antes
dejarían el montón picado y las seis peticiones de 16.384 tendrían que buscarse
la vida entre los huecos. Es la regla del mapa de 256.000 del juego, a escala.

**Y hay que alimentar la tarjeta de sonido durante la carga**, cada 16 filas de
textura. Seis BMP y cinco remapeos es lo más lento que hace esta carpeta.

## De dónde salen las seis fotos

De la propia presentación, empezando por la que se está enseñando:

```c
	current = demo_image_current();
	... demo_image_path(current + index) ...
```

Así cada vez que el cubo sale en la intro lleva seis fotos distintas.
`demo_image_path()` envuelve sola, o sea que con menos de seis imágenes se
repiten y no falla nada. Son dos funciones de `demos.c` y no un array global a
propósito: el efecto nunca puede cambiar la lista.

## Experimentos

1. Pon `TEXCUBE_GRID` a 1. Vuelve el pliegue de la diagonal en cada cara: es la
   forma más rápida de ver para qué está la rejilla.
2. Súbelo a 8. No se ve ninguna diferencia y el coste sube: ahí está el punto
   donde dejar de subdividir.
3. Cambia `if (cross <= 0)` por `if (cross >= 0)`. Se pintan las tres caras de
   atrás en vez de las tres de delante y el cubo se ve del revés.
4. Quita el `continue` del descarte del todo. Se pintan las seis, la última gana,
   y el cubo se convierte en un cuadrilátero plano.
5. En `texcube_load_face()`, fuerza `is_current = 1` para todas las caras. Ahí
   ves lo que pasa sin remapeo: cinco caras de ruido de colores.

---

**Anterior:** [El cubo de alambre](../../cube/doc/README.md) ·
**Índice:** [Los diecinueve efectos](../../README.md)
