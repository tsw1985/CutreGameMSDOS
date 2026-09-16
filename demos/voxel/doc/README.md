# voxel — Volar sobre la foto

*[English version](README-EN.md)*

**Efecto 18 de 20 del recorrido.** Coste medido: **135 microsegundos por frame**,
el más caro de los veinte.

El código está en [`../voxel.c`](../voxel.c).

---

## Qué hace

La foto se lee como un mapa en relieve — lo claro es monte, lo oscuro es valle —
y luego se vuela por encima. Es el motor del **Comanche (1992)**, que dejó a todo
el mundo con la boca abierta porque parecía 3D y no había ni un polígono.

## De una foto a un paisaje

Hacen falta dos mapas: uno de **alturas** y otro de **colores**. La foto da los
dos, y gratis:

```
   color  = el pixel, tal cual
   altura = lo claro que es ese color
```

Lo segundo **no se puede mirar en el píxel**, y ahí está la trampa que persigue a
media carpeta: en 256 colores un píxel no es un color, es un **índice**. El 200
no es más claro que el 3, es simplemente el que ocupa la casilla 200. Hay que ir
a la paleta, sumar sus tres canales y quedarse con eso.

Son 256 sumas al entrar y dejan una tabla que convierte índice en altura de un
vistazo.

## Y luego hay que estirar el relieve

Esto no estaba y hubo que ponerlo después de mirar el banco. Con las alturas
crudas, una foto de tonos parecidos daba **un llano perfecto**:

```
   demo13: 87% de sus pixeles caen en la MISMA altura
           -> el paisaje es una meseta, y es correcto
   demo01: sus colores van de 7 a 210
           -> montanas y barrancos
```

Así que se busca el más bajo y el más alto **de los que la foto usa** —
recorriendo la textura, no la paleta, porque media paleta puede estar sin
estrenar — y se reparte ese trozo por los 63 escalones. Una foto plana sale con
montañas suaves y una contrastada con acantilados, pero ninguna sale como una
mesa de billar.

Lo que no arregla nada: si el 87% de la foto es de un solo tono, el 87% del
paisaje es una meseta, por mucho que se estire. Eso no es un defecto del código,
es que la foto es lisa.

## El algoritmo, que es de una línea

Por cada **columna** de la pantalla se lanza un rayo hacia el horizonte y se va
andando. En cada paso se mira la altura del terreno, se proyecta a qué altura de
pantalla le toca, y:

> si sale **más arriba** de lo más alto pintado hasta ahora en esta columna, se
> pinta el trozo nuevo. Si no, está tapado y no se pinta.

```
   recorriendo de CERCA a LEJOS:

   horizonte ---------------------------
                        ___
                   ____/   \___            <- este se ve: sale mas arriba
              ____/            \__
         ____/                              <- este queda tapado: no se pinta
   ____/
```

Eso basta para que lo cercano tape a lo lejano **sin buffer de profundidad, sin
ordenar y sin repintar un solo píxel dos veces**. Un número por columna — *hasta
dónde he llegado* — hace el trabajo que en un 3D de verdad hace un z-buffer de
64.000 bytes.

## Y el truco de siempre: ni una división

Proyectar es dividir por la distancia, y son 41.000 pasos por frame. Las
distancias, en cambio, son siempre las mismas 128: se camina de 1 en 1. Así que
se precalculan los 128 inversos al entrar:

```c
	screen_y = VOXEL_HORIZON
	         + (((VOXEL_CAMERA - terrain) * inverse[z]) >> DEMO_SHIFT);
```

Un producto y un desplazamiento donde había una división.

## Las trampas

**La cámara tiene que volar por encima del monte más alto** o se metería dentro
del terreno. 80 contra 63 deja poco margen a propósito: cuanto más raso se vuela,
más se abren las distancias entre lo alto y lo bajo y más relieve se ve. Con la
cámara a 200 el mismo paisaje parece un mapa pintado.

**Nada puede dibujarse por encima del horizonte**, y eso da una salida temprana
que es exacta y no una aproximación: como `VOXEL_CAMERA - terrain` es siempre
positivo, `screen_y` siempre cae por debajo de `VOXEL_HORIZON`. Cuando una
columna llega ahí, los pasos que queden son tiempo tirado.

**Los AND dan la vuelta al mapa** porque la textura es de 128, potencia de dos.
El paisaje se repite hasta el infinito en las dos direcciones y nunca se acaba.
Es lo mismo que hace [carpet](../../carpet/doc/README.md), en horizontal.

## Coste

**El más caro de los veinte**, y por dos motivos que se suman: 41.000 pasos de
rayo y hasta 64.000 escrituras **con paso de 320 bytes**, porque una columna de
pantalla no son bytes seguidos. Es la misma escritura incómoda que
[melt](../../melt/doc/README.md), pero aquí con una cuenta en cada paso.

Si se arrastra en tu máquina, `VOXEL_DEPTH` a 96 quita una cuarta parte del
trabajo y solo se pierde la banda más lejana, que ya está comprimida en ocho
filas.

## Experimentos

1. Pon `VOXEL_CAMERA` a 200. El relieve se aplana hasta parecer un mapa pintado:
   ahí ves que la altura de vuelo es el mando del contraste.
2. Quita el estirado del relieve (el bloque de `lowest`/`loftiest`). Prueba
   después con una foto clara y verás la mesa de billar.
3. Pon `VOXEL_SPREAD` a 256. El ángulo de visión se abre tanto que los bordes se
   estiran como un ojo de pez.
4. Cambia el recorrido a de lejos a cerca y quita el `if (screen_y >= highest)`.
   Sigue saliendo bien y cuesta el doble: eso es pintar lo que luego se tapa.

---

**Anterior:** [La foto envuelta en una bola que gira](../../sphere/doc/README.md) ·
**Siguiente:** [El cubo de alambre](../../cube/doc/README.md) ·
**Índice:** [Los veinte efectos](../../README.md)
