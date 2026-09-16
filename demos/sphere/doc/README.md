# sphere — La foto envuelta en una bola que gira

*[English version](README-EN.md)*

**Efecto 17 de 20 del recorrido.** Coste medido: **22 microsegundos por frame**.

El código está en [`../sphere.c`](../sphere.c).

---

## Qué hace

Un globo terráqueo con tu foto de mapa, girando sobre negro. Es el otro gran
efecto de tabla de la época junto con el túnel, y sale del mismo razonamiento.

## La idea, que es la del túnel

Para cada píxel de la pantalla hay que saber dos cosas de la bola:

```
   latitud    a que altura del globo cae   ->  asin(dy / R)
   longitud   por donde da la vuelta       ->  asin(dx / ancho_de_esa_fila)
```

Dos arcosenos por píxel, tan imposible como las raíces del
[túnel](../../tunnel/doc/README.md). Y otra vez: ninguno depende del tiempo. Se
calculan al entrar y el frame se queda en una suma y un AND.

```c
	screen[i] = texture[ (latitud[i] << 7) + ((longitud[i] + giro) & 127) ];
```

## Pero aquí cuesta la mitad que el túnel, y eso tiene gracia

El túnel necesita dos tablas de 64.000 bytes. Esta solo una, y el motivo merece
pararse:

> **La latitud de un píxel depende solo de su fila.**

Todos los píxeles de la fila 40 están a la misma altura del globo, se miren por
donde se miren. O sea que la latitud son **doscientos números**, uno por fila, y
no 64.000.

```
   fila 40  ---------------------  todos a la misma latitud
            \                   /
             \_________________/
```

La longitud sí es por píxel, porque depende de lo ancho que sea el globo a esa
altura: cerca del polo, dos píxeles separados por poco abarcan muchos grados.

64.600 bytes en vez de 128.000, y de paso el bucle interior se ahorra una lectura
de tabla y un desplazamiento por píxel, porque la fila de textura se calcula una
sola vez por fila. Por eso cuesta 22 microsegundos y el túnel 77.

## El contorno se resuelve una vez y no se vuelve a mirar

La bola es un círculo, así que cada fila de la pantalla la corta en **un trozo
seguido**. Se guarda dónde empieza y dónde acaba cada fila y el bucle de pintar
no pregunta nunca si está dentro: lo de fuera se hace con `memset`. Es el mismo
argumento del tramo visible del [rotozoom](../../rotozoom/doc/README.md).

## Las trampas

**El píxel no es cuadrado**, igual que en [cube](../../cube/doc/README.md). Un
círculo de radio igual en las dos direcciones sale con forma de huevo. Aquí se
resuelve al revés que allí: en vez de encoger lo que se dibuja, **se estira la
`y` antes de entrar en las cuentas** y todo lo demás razona como si fuera un
círculo de verdad.

```c
	stretched = (dy * SPHERE_ASPECT_NUM) / SPHERE_ASPECT_DEN;
```

**La fila del polo tiene ancho cero.** `asin(dx / half)` dividiría entre cero. Se
resuelve aparte: en el polo todas las longitudes valen lo mismo, así que el
ángulo es 0 y punto.

**La cola de la tabla de arcosenos parece un error y no lo es:**

```
	..., 29, 29, 30, 32
```

Cerca de 1 el arcoseno se pone vertical y un cambio diminuto en el seno son
muchos grados. En la bola eso cae justo en el borde, donde la textura se comprime
hasta desaparecer y nadie lo nota.

## Coste

Barato para lo que parece. La bola ocupa unos 24.000 píxeles de los 64.000 y el
resto se hace con `memset`; el bucle interior es una suma, un AND y dos lecturas.
Sale por debajo del [zoom](../../zoom/doc/README.md), que no tiene tres
dimensiones.

## Experimentos

1. Quita el estirado de la `y`. Sale un huevo, y entiendes de un vistazo lo del
   píxel no cuadrado.
2. Cambia `(angle + SPHERE_QUARTER) * 2` por `(angle + SPHERE_QUARTER)`. La
   textura solo usa media altura y la bola sale con la foto aplastada contra el
   ecuador.
3. Pon `SPHERE_SPIN` a 0. Se queda quieta y se ve la deformación de la esfera sin
   el movimiento distrayendo.
4. Quita el caso `half == 0`. División entre cero en la primera fila del polo.

---

**Anterior:** [El túnel infinito](../../tunnel/doc/README.md) ·
**Siguiente:** [Volar sobre la foto](../../voxel/doc/README.md) ·
**Índice:** [Los veinte efectos](../../README.md)
