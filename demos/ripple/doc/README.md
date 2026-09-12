# ripple — La gota en el estanque

*[English version](README-EN.md)*

**Efecto 7 de 10 del recorrido.** Coste medido: **9,0 microsegundos por frame**.

El código está en [`../ripple.c`](../ripple.c).

---

## Qué hace

Ondas que salen del centro de la pantalla hacia fuera, como una gota cayendo en
un estanque.

## La idea

Se parece al `wobble` y no es lo mismo, y la diferencia es la que hay entre una
bandera y un charco:

| | El seno depende de… | Resultado |
|---|---|---|
| **wobble** | la FILA | Las ondas viajan de arriba abajo, todas igual de fuertes |
| **ripple** | la DISTANCIA AL CENTRO | Las ondas salen del medio **y se van muriendo** |

Ese apagarse con la distancia es lo que lo hace parecer agua de verdad. Sin eso,
el borde de la pantalla ondula igual que el centro y el ojo no se lo cree.

```
   fila   0  amplitud  0   .
   fila  50  amplitud 10   ~~~
   fila 100  amplitud 20   ~~~~~~~   <- el centro
   fila 150  amplitud 10   ~~~
   fila 199  amplitud  0   .
```

## Paso a paso

**1. La distancia de la fila al centro**, en valor absoluto.

**2. La amplitud, que cae en línea recta hacia los bordes:**

```c
	amplitude = (RIPPLE_AMPLITUDE * ((DEMO_HEIGHT / 2) - distance)) / (DEMO_HEIGHT / 2);
```

Una caída cuadrática sería más física y costaría una multiplicación más por
fila para algo que el ojo no distingue.

**3. El seno, con la distancia dentro:**

```c
	shift = (demo_sin(phase + (distance * RIPPLE_DENSITY)) * amplitude) >> DEMO_SHIFT;
```

**4. Los dos `memcpy`** de siempre.

## Las trampas

**`RIPPLE_SPEED` es NEGATIVO.** Vale −5, y no es un error. Con un valor positivo
las ondas vienen de fuera hacia el centro, como si la gota cayera en el borde de
la pantalla. Con negativo salen del centro, que es lo que hace una gota de
verdad. Cambiarle el signo es la diferencia entre parecer agua y parecer un
efecto raro.

**La amplitud puede salir 0**, y entonces el desplazamiento es 0 y los `memcpy`
copian la fila tal cual. Correcto y sin casos especiales.

## Coste

Igual que `wobble`: un seno por fila. Las dos multiplicaciones extra de la
amplitud son 200 por frame, nada.

## Experimentos

1. Cambia `RIPPLE_SPEED` a +5 y compara. Es el mismo código y parece otro
   efecto.
2. Quita la atenuación: pon `amplitude = RIPPLE_AMPLITUDE` fijo. Se convierte en
   un `wobble` con el seno medido desde el centro, y deja de parecer agua.
3. Haz la caída cuadrática multiplicando la amplitud por sí misma y dividiendo.
   Mira si notas la diferencia. Yo no la noto, y por eso no está.

---

**Anterior:** [La foto que se pasea](../../bounce/doc/README.md) ·
**Siguiente:** [El pixelado que respira](../../mosaic/doc/README.md) ·
**Índice:** [Los diez efectos](../../README.md)
