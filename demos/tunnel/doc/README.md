# tunnel — El túnel infinito

*[English version](README-EN.md)*

**Efecto 15 de 19 del recorrido.** Coste medido: **77 microsegundos por frame**,
más **2 milisegundos** de montar las tablas al entrar.

El código está en [`../tunnel.c`](../tunnel.c).

---

## Qué hace

Volar por dentro de un tubo forrado con la foto, sin salir nunca.

Es *el* efecto de las intros de los 90 junto con el rotozoom, y el que mejor
enseña la idea que sostiene a media carpeta: **cuando una cuenta es cara y no
cambia, no se optimiza, se precalcula.**

## La cuenta que no se puede hacer

Para cada píxel de la pantalla hay que saber dos cosas del tubo:

```
   angulo       donde cae alrededor del tubo  ->  atan2(dy, dx)
   profundidad  lo lejos que esta             ->  k / raiz(dx2 + dy2)
```

Una **arcotangente** y una **raíz cuadrada** por píxel. Son 64.000 píxeles a 70
frames por segundo: cuatro millones y medio de raíces por segundo en un 386. No
es que sea lento, es que no existe.

## La idea: ninguna de las dos cambia nunca

El ángulo y la profundidad de un píxel dependen de **dónde está el píxel**, y el
píxel no se mueve.

Lo único que cambia con el tiempo es por dónde vamos del tubo y cuánto hemos
girado, y eso son **dos números para toda la pantalla**, no dos números por
píxel.

```
   AL ENTRAR (una vez)          CADA FRAME (64.000 veces)
   ------------------           ------------------------
   depth[i] = k / radio         depth[i] + avance
   angle[i] = atan2(dy,dx)      angle[i] + giro
   64.000 raices                dos sumas y dos AND
   64.000 arcotangentes
```

```c
	screen[offset] = texture[
		(((depth[offset] + step_depth) & TUNNEL_MASK) << DEMO_TEXTURE_SHIFT)
	  +  ((angle[offset] + step_angle) & TUNNEL_MASK)
	];
```

Dos lecturas de tabla, dos sumas, dos AND y una lectura de textura. **Ni una
raíz, ni una división, ni un producto.** El movimiento entero del túnel son las
dos variables `advance` y `spin`.

Cuesta **128.000 bytes de memoria**, y ese es el trato: memoria por tiempo. Es
el mismo trato que hace `demo_row[]` en `demolib.h`, solo que a lo bestia.

## Y las tablas se montan sin `math.h`

Aquí no entra ni un `float`, así que hay que fabricar las dos funciones.

**La raíz cuadrada, bit a bit.** El método de la escuela en base 2: se prueba a
poner un bit en la raíz empezando por el más alto y se queda el que no se pasa.
Son 8 vueltas para un número de 16 bits, contra las 189 que daría el «ir sumando
uno hasta pasarse».

**La arcotangente, 65 números.** Solo hace falta **un octavo de vuelta**: 16 de
los 128 pasos. Todo lo demás sale por simetría, porque un círculo es el mismo
octavo repetido ocho veces dado la vuelta.

```c
	if (ay <= ax){
		base = tunnel_atan[(ay * 64) / ax];                    // primer octavo
	}else{
		base = TUNNEL_QUARTER - tunnel_atan[(ax * 64) / ay];   // el mismo, del reves
	}
```

Eso convierte una función trascendente en 65 números y cuatro comparaciones de
signo. Las dos divisiones son **entre el lado largo, siempre**, que es lo que
garantiza que la razón cae en 0..64 y que no se divide entre cero.

## Las trampas

**La vuelta al tubo son 128 pasos, no 256.** Todo el resto de la carpeta usa
ángulos de 256 pasos. Aquí no, y no es un capricho: 128 es el ancho de la
textura, así que **el ángulo ya es la columna de textura** y no hay que
escalarlo. Un paso de escala por píxel son 64.000 productos por frame.

**El centro exacto tiene radio 0.** La profundidad sería infinita y la división
reventaría. Se le pone radio 1, que lo deja en el píxel más lejano de todos.

**Y hay que alimentar la tarjeta de sonido mientras se montan las tablas.**
Son 64.000 raíces y 64.000 arcotangentes, lo más lento que hace esta carpeta
después de cargar el cubo con fotos. Sin el `demo_sound()` cada 16 filas, la
música se quedaría en bucle durante el montaje. Es la misma razón por la que
`demos.c` llama a `demo_sound()` a los dos lados de cargar un BMP.

## Se ve una costura radial

La textura no encaja consigo misma por los lados, así que donde el ángulo da la
vuelta (de 127 a 0) se ve una línea. Es el mismo precio que paga
[carpet](../../carpet/doc/README.md) y lo pagaban todos los túneles de la época.

## Coste

El más caro de los quince, y es honrado: escribe los 64.000 píxeles todos los
frames sin ahorrarse ni uno. A cambio, el bucle interior es de los más simples
de la carpeta — toda la dificultad está en el montaje.

## Experimentos

1. Pon `TUNNEL_SPIN` a 0. Deja de girar y se ve solo el avance: ahí se entiende
   que son dos movimientos independientes sumados a dos tablas.
2. Sube `TUNNEL_DEPTH` a 8192. El tubo se estira y se ven muchísimos más anillos
   apretados en el fondo.
3. Cambia `radius < 1` por `radius < 20`. Aparece un disco plano en el centro:
   es el fondo del túnel dejando de ser infinito.
4. Sustituye la raíz entera por `ax + ay / 2` (la aproximación que usa el radar
   del juego). El túnel deja de ser redondo y se convierte en un rombo. Es una
   buena forma de ver cuánto importa la raíz de verdad.

---

**Anterior:** [El rotozoom que no se acaba nunca](../../carpet/doc/README.md) ·
**Siguiente:** [La foto envuelta en una bola que gira](../../sphere/doc/README.md) ·
**Índice:** [Los diecinueve efectos](../../README.md)
