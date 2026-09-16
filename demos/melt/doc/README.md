# melt — La pantalla que se derrite

*[English version](README-EN.md)*

**Efecto 5 de 20 del recorrido.** Coste medido: **27 microsegundos por frame**.

El código está en [`../melt.c`](../melt.c).

---

## Qué hace

Cada columna de la foto se descuelga por su cuenta, a su velocidad y empezando
cuando le toca, hasta que no queda nada. Es el barrido con el que **Doom cambiaba
de pantalla en 1993**, y sigue siendo la transición más reconocible que se ha
hecho.

Es también el efecto con menos matemáticas de los veinte: no hay punto fijo, ni
tabla, ni proyección. Son 320 números que crecen.

## Por qué va por columnas y no por filas

Los cuatro efectos anteriores mueven **filas enteras**, porque una fila son 320
bytes seguidos y eso es un `memcpy`. Este no puede:

```
   cada columna esta caida una cantidad distinta

   x=0   x=1   x=2   x=3
    |     |           |
    v     v     v     v      <- una FILA de la pantalla se compone
   ###   ###   ###   ###        de 320 trozos de 320 filas
   ###   ###         ###        distintas de la foto
```

Recorrerlo por columnas significa escribir con un paso de 320 bytes, que es lo
peor que se le puede pedir a una caché. En un 386 eso da igual — no hay caché
que estropear — y son 64.000 escrituras sin una sola cuenta, así que sigue
saliendo de los baratos.

## El azar sin `rand()`, y que no salga un peine

Doom sorteaba el retraso de cada columna. Aquí no hay `rand()` ni hace falta: dos
senos de periodos distintos, sumados, dan una sucesión que no se repite en 320
columnas y que además sale **igual en las dos máquinas** de una partida en red.

Pero el reparto no puede ser azar del todo, y esto costó una pasada por el banco.
Con dos senos de periodo corto, cada columna caía a un ritmo sin relación con su
vecina y lo que salía era un **peine**: 320 palos independientes, no una cortina.

```
   PEINE (mal)                  CORTINA (bien)
   |‾|_|‾‾|_|‾|__|‾|            ‾‾\__/‾‾‾\_/‾‾‾‾\__
   frecuencias parecidas        una onda LENTA que manda
   y del mismo peso             + una rapida que solo mella
```

Doom no lo hacía con senos: sorteaba la primera columna y a cada siguiente le
dejaba separarse como mucho un paso de la anterior. El resultado es el mismo y
aquí se consigue **pesando las frecuencias**:

```c
	mixed = (demo_sin(x * MELT_SLOW) * MELT_SLOW_WEIGHT)
	      +  demo_sin(x * MELT_FAST);
```

`MELT_SLOW` es 2 sobre 256 pasos, o sea una onda cada 128 columnas: esa hace la
cortina. `MELT_FAST` es 37, una onda cada siete columnas, y con un tercio del
peso solo le pone el borde mellado.

## Paso a paso

**1. El reparto, una sola vez.** Una columna tiene que caer siempre a la misma
velocidad: si se recalculara por frame, la cortina temblaría en vez de caer.

**2. La velocidad sale de otra onda, y desfasada 64 pasos** (un cuarto de
vuelta). Si saliera de la misma, las columnas que empiezan tarde serían además
las lentas y la cortina se abriría siempre por el mismo sitio.

**3. Gastar el retraso primero y caer después:**

```c
	if (delay[x] > 0){
		delay[x]--;
	}else if (offset[x] < DEMO_HEIGHT){
		offset[x] += speed[x];
	}
```

**4. Volver a empezar cuando no queda nada cayendo.** Siete segundos dan para dos
pasadas y pico, y dejar la pantalla en negro los tres últimos segundos sería una
diapositiva perdida.

## Las trampas

**El margen de velocidades es estrecho a propósito.** Con `MELT_SPEED_MIN` 1 y
`MAX` 4, las columnas rápidas van cuatro veces más deprisa que las lentas y la
cortina se despedaza a los dos segundos. 2 y 4 mantienen la forma hasta el final.

**El `+ 1024` en vez de un valor absoluto.** `mixed` va de −1024 a 1024 y hace
falta en 0..2048. Sumarle el rango lo desplaza entero; un valor absoluto lo
doblaría por la mitad y las columnas de la izquierda caerían igual que las de la
derecha.

## Experimentos

1. Pon `MELT_SLOW_WEIGHT` a 0. Ahí tienes el peine, y entiendes por qué esa línea
   es todo el efecto.
2. Pon `MELT_FAST` también a 2. La cortina sale perfectamente lisa y parece una
   persiana, no algo derritiéndose.
3. Quita el desfase de 64 de la velocidad. La cortina se abre siempre por el
   mismo sitio.
4. Sube `MELT_DELAY_MAX` a 200. Las primeras columnas llegan abajo antes de que
   las últimas hayan empezado.

---

**Anterior:** [Bandas a distinta velocidad](../../stripes/doc/README.md) ·
**Siguiente:** [La imagen que ondula](../../wobble/doc/README.md) ·
**Índice:** [Los veinte efectos](../../README.md)
