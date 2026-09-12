# rotozoom — Girar y hacer zoom a la vez

*[English version](README-EN.md)*

**Efecto 10 de 10 del recorrido.** Coste medido: **179 microsegundos por frame**, y antes de optimizarlo eran 257.

El código está en [`../rotozoom.c`](../rotozoom.c).

---

## Qué hace

La imagen da vueltas sobre negro mientras se acerca y se aleja. **EL** efecto de
las intros de los 90, el que había que tener.

## La idea

Es el `zoom` con giro, pero el giro lo cambia todo.

La cuenta para un píxel `(x,y)` medido desde el centro es:

```
   u =  x*cos(a)*escala + y*sin(a)*escala
   v = -x*sin(a)*escala + y*cos(a)*escala
```

Cuatro multiplicaciones por píxel. **256.000 por frame.** En un 8086 eso no va
ni en broma.

### El truco de la época

La cuenta es **lineal**. Eso significa que al avanzar un píxel a la derecha, `u`
y `v` siempre crecen lo mismo, dé igual dónde estés:

```
   x:    0     1     2     3     4
   u:  100   112   124   136   148      <- siempre +12
   v:   50    57    64    71    78      <- siempre +7
```

Así que se calculan esos dos incrementos **una vez por frame** y dentro del
bucle solo hay dos sumas:

```c
	u += du_dx;
	v += dv_dx;
```

**De 256.000 multiplicaciones a 2 sumas por píxel.** Ése es el rotozoom.

## Paso a paso

**1. Los cuatro incrementos**, calculados una vez:

```c
	du_dx = ((long)demo_cos(angle) * scale) >> DEMO_SHIFT;
	dv_dx = ((long)demo_sin(angle) * scale) >> DEMO_SHIFT;
	du_dy = -dv_dx;
	dv_dy =  du_dx;
```

`du_dy` y `dv_dy` son los de bajar una fila, y salen de los otros dos: bajar una
fila es lo mismo que avanzar una columna, girado 90 grados.

**2. La esquina de arriba a la izquierda.** Se parte del centro de la imagen y
se retrocede media pantalla **en las dos direcciones giradas**. Sin esto, la
imagen giraría alrededor de su esquina.

**3. Recorrer**, sumando `du_dx`/`dv_dx` por columna y `du_dy`/`dv_dy` por fila.

## La optimización: el tramo visible

La imagen girada es un **cuadrilátero**, y una recta horizontal corta un
cuadrilátero en **un solo trozo**. No en dos, no en varios: uno.

```
   fila 40:   ############/IMAGEN\###############
                          ^      ^
                       first    last
```

Así que en vez de preguntar 320 veces por fila "¿estoy dentro?", se calcula
dónde empieza y dónde acaba, se rellena lo de fuera con `memset` y **dentro del
tramo no se comprueba nada**, porque por construcción todo cae dentro.

De **257 µs/frame a 179**. Y en un rotozoom con la imagen alejada, lo de fuera
son la mayoría de los píxeles de la pantalla.

### Cómo se calcula el tramo, sin dividir en negativo

`rotozoom_span()` resuelve `0 <= inicio + x*paso < limite`. Y todas sus
divisiones son **de positivo entre positivo**, a propósito:

> En C89 el redondeo de una división con negativos **queda a gusto del
> compilador**. Eso es exactamente el tipo de cosa que funciona en gcc y hace
> otra cosa en Turbo C.

Cuando el paso es negativo se le da la vuelta al signo y se resuelve el mismo
problema al revés:

```c
	if (step > 0){
		if (start >= 0){
			first = 0;
		}else{
			first = ((-start) + step - 1) / step;   /* redondeo hacia arriba */
		}
		...
	}else{
		down = -step;      /* ahora se divide en positivo */
		...
	}
```

## Las trampas

**`scale` nunca puede ser 0**: sería dividir el mundo entre nada. De ahí el
`if (scale < 32)`.

**Dos productos 8.8 seguidos dan 16.16.** Por eso cada multiplicación lleva su
`>> DEMO_SHIFT` detrás. Si se te olvida uno, el efecto sale a una escala 256
veces mayor y no ves nada.

**El tramo hay que intersecarlo**, no quedarte con uno de los dos. Un píxel
tiene que estar dentro en `u` **y** en `v`; el tramo bueno es la intersección de
los dos.

## Coste

**El más caro de los diez, con diferencia**: 179 µs/frame, más de veinte veces
lo que cuesta un `wobble`. Es inevitable, y por eso está el primero de la lista
de efectos: cuando la máquina va justa, es el que se nota.

## Experimentos

1. Pon `ROTOZOOM_SPIN` a 0. Se convierte en un `zoom`, y verás que el `zoom` de
   verdad hace lo mismo mucho más rápido.
2. Quita el `>> DEMO_SHIFT` de `du_dx`. Fondo negro y nada más.
3. Sustituye `rotozoom_span()` por el `if` por píxel de la primera versión y
   mide. La diferencia es mayor cuanto más alejada está la imagen.
4. Prueba a hacer que `du_dy = dv_dx` en vez de `-dv_dx`. Sale un espejo girado:
   es útil para entender por qué el signo está donde está.

---

**Anterior:** [Acercarse y alejarse](../../zoom/doc/README.md) ·
**Índice:** [Los diez efectos](../../README.md)
