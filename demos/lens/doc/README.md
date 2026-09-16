# lens — La lupa que se pasea por la foto

*[English version](README-EN.md)*

**Efecto 12 de 19 del recorrido.** Coste medido: **9,4 microsegundos por frame**.

El código está en [`../lens.c`](../lens.c).

---

## Qué hace

Un círculo de cristal recorre la imagen y agranda lo que pilla debajo. Clásico de
los 90 y de los salvapantallas de la época, y uno de los pocos efectos que deja
ver la foto **entera** todo el rato: el resto de la pantalla no se toca.

## La idea: la lupa no se mueve, la foto sí

Lo que deforma una lupa depende solo de **dónde está el píxel dentro de la
lupa**, no de por dónde ande la lupa. El píxel del centro siempre lee del centro,
el del borde siempre lee del borde.

O sea que el desplazamiento de cada punto del cristal se calcula una vez, al
entrar, y se guarda como un desplazamiento en bytes:

```c
	lens_delta[i] = (dy_origen * 320) + dx_origen;
```

Y mover la lupa es sumarle a todo eso la posición del centro. Cada frame son
6.100 sumas y 6.100 lecturas, y ni una raíz ni una división.

Es la idea del [túnel](../../tunnel/doc/README.md) — precalcular lo que no cambia
— aplicada a una superficie de 6.100 píxeles en vez de a los 64.000 de la
pantalla. Por eso la tabla ocupa 12 KB y no 128.

## La forma del cristal

```
	origen = r * (1 - k * (1 - (r/R)^2))
```

En el borde (`r = R`) el paréntesis vale 1 y **no desplaza nada**, así que la
lupa se funde con la foto sin un corte. En el centro (`r = 0`) aumenta
`1/(1-k)`, que con `k = 0,4` son 1,67 aumentos.

```
   r/R:     0.0   0.25   0.5   0.75   1.0
   origen:  0.0   0.16   0.35  0.62   1.0     <- llega justo al borde
                                    ^ aqui no salta
```

Lo importante es esa última columna. Una lupa que en el borde salta se ve como un
recorte pegado encima, no como cristal.

## Paso a paso

**1. La tabla, recorriendo el cuadrado que contiene al círculo.** De los puntos
de dentro se guarda de dónde leen; de las filas se guarda **dónde empieza y acaba
la parte útil**, que es lo que deja el bucle de pintar sin un solo *¿estoy
dentro?*. Es el mismo truco del tramo visible del
[rotozoom](../../rotozoom/doc/README.md).

**2. La raíz entera, aquí a lo bruto:**

```c
	radius = 0;
	while ((radius + 1) * (radius + 1) <= squared){
		radius++;
	}
```

Son 6.100 puntos **una vez en toda la vida del efecto**, así que no merece la
pena traerse el método bit a bit del túnel. Es el único sitio de la carpeta donde
la versión lenta es la correcta.

**3. Y el paseo**, dos senos de velocidades primas entre sí, como en
[bounce](../../bounce/doc/README.md).

## Las trampas

**El origen se cuenta desde el CENTRO de la lupa, no desde el píxel que se está
pintando.** Esto costó un `Segmentation fault` en el banco: `lens_delta` guarda
*de dónde lee el punto (lx,ly) del cristal*, y ese de-dónde está medido respecto
al centro. Sumárselo al píxel actual lo duplica todo y se sale del buffer por
abajo — el sanitizador lo cazó leyendo 87 bytes más allá del final de la imagen.

```c
	screen[destination] = image[center_offset + (unsigned int)delta[index]];
	/*                          ^^^^^^^^^^^^^  y no destination */
```

**El centro se queda a un radio de los bordes SIEMPRE:**

```c
	range_x = (DEMO_WIDTH  / 2) - LENS_RADIUS - 1;
```

Y con eso el origen nunca se sale del buffer, lo que permite que el bucle de
pintar no compruebe límites. La alternativa sería un *¿está dentro?* por píxel,
6.100 veces por frame. Aquí el recorte se resuelve **una vez, en el recorrido**,
no en el dibujo.

## Coste

Un `memcpy` de 64.000 para la foto y 6.100 píxeles sueltos. El `memcpy` es lo
caro: la lupa en sí cuesta la décima parte.

## Experimentos

1. Pon `LENS_STRENGTH` a 0. La lupa desaparece aunque el código siga corriendo:
   `factor` vale 256 y cada punto lee de sí mismo.
2. Súbelo a 200. Aumenta 4,6 veces y se ve el píxel gordo.
3. Cambia la fórmula por `source_radius = (radius * radius) / LENS_RADIUS`. En el
   borde sigue sin saltar y en el centro aumenta infinito: sale una gota de agua
   en vez de una lupa.
4. Sube `LENS_RADIUS` a 90 sin tocar `range_x`. El centro se acerca demasiado al
   borde y ahí ves por qué está ese `- LENS_RADIUS - 1`.

---

**Anterior:** [Acercarse y alejarse](../../zoom/doc/README.md) ·
**Siguiente:** [Girar y hacer zoom a la vez](../../rotozoom/doc/README.md) ·
**Índice:** [Los diecinueve efectos](../../README.md)
