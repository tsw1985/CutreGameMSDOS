# water — La foto reflejada en el agua

*[English version](README-EN.md)*

**Efecto 7 de 20 del recorrido.** Coste medido: **2,2 microsegundos por frame**,
el más barato de los veinte.

El código está en [`../water.c`](../water.c).

---

## Qué hace

La mitad de arriba es la foto. La mitad de abajo es la misma foto del revés,
ondulando, como si la escena estuviera al borde de un estanque.

## Es el wobble, con tres diferencias

[wobble](../../wobble/doc/README.md) desplaza cada fila según un seno. Esto
también, y por eso va justo detrás en el recorrido: lo que cambia no es la
técnica, **es lo que se hace con ella**.

```
   1. solo se ondula la mitad de abajo
   2. esa mitad lee la foto AL REVES, de la fila 99 hacia arriba
   3. la amplitud CRECE hacia abajo, en vez de ser constante
```

La tercera es la que hace el trabajo:

```
   amplitud constante              amplitud creciente
   ~~~~~~~~~~~~~~~~~  orilla       ~~~~~~~~~~~~~~~~~  orilla
   ~~~~~~~~~~~~~~~~~                ~~~~~~~~~~~~~~~
   ~~~~~~~~~~~~~~~~~                 ~~~~~~~~~~~~~
   ~~~~~~~~~~~~~~~~~                  ~~~~~~~~~~~
   parece una bandera              parece agua
   puesta boca abajo
```

Y tiene sentido físico: lo que se refleja cerca del horizonte está lejos, y lo
que está lejos apenas se mueve en la imagen aunque el agua se mueva igual.

## Paso a paso

**1. La mitad de arriba se copia UNA vez, fuera del bucle.** No cambia nunca, así
que son 32.000 bytes por frame que no se tocan. Ahí está la mitad de por qué este
efecto es el más barato de la carpeta.

**2. La amplitud, que es una recta y no un seno:**

```c
	amplitude = WATER_AMPLITUDE_FAR
	          + (((WATER_AMPLITUDE_NEAR - WATER_AMPLITUDE_FAR) * depth)
	             / (DEMO_HEIGHT - WATER_HORIZON));
```

Lo que tiene que crecer es **cuánto** se mueve, no **cómo**: la forma de la onda
ya la pone el seno de la línea siguiente.

**3. La fila de origen, contada hacia arriba desde la orilla.** Eso es lo que da
la vuelta al reflejo:

```c
	source_y = WATER_HORIZON - 1 - depth;
```

**4. Y el desplazamiento, con dos `memcpy` y dos `memset`** — uno para el trozo
que se ve y otro para el hueco negro que deja.

## Las trampas

**`shift` es `int` y puede ser negativo**, y ahí está la trampa que persigue a
este proyecto entero: metido en un `unsigned` sería 65527, y un `memcpy` de
65527 bytes se lleva por delante lo que haya detrás del buffer. Por eso hay dos
ramas explícitas y no una cuenta lista que valga para las dos.

**`source_y` se recorta a 0.** Con el horizonte en la mitad justa nunca se sale,
pero muévelo a un tercio de la pantalla y el reflejo pediría filas negativas.

## Coste

**El más barato de los veinte**, y no por poco: la mitad de la pantalla no se
toca y la otra mitad son 100 `memcpy` de menos de 320 bytes. Ni un píxel se
escribe de uno en uno.

## Experimentos

1. Pon `WATER_AMPLITUDE_FAR` igual a `WATER_AMPLITUDE_NEAR`. Vuelve la bandera
   boca abajo y se pierde el agua.
2. Quita el `- 1 - depth` y deja `source_y = depth`. El reflejo deja de estar
   invertido y parece una pantalla partida.
3. Sube `WATER_HORIZON` a 150. Un charco en vez de un lago.
4. Sube `WATER_AMPLITUDE_NEAR` a 40. Ahí ves para qué están las dos ramas del
   recorte.

---

**Anterior:** [La imagen que ondula](../../wobble/doc/README.md) ·
**Siguiente:** [La foto que se pasea](../../bounce/doc/README.md) ·
**Índice:** [Los veinte efectos](../../README.md)
