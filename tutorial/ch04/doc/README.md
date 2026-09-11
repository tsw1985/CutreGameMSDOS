# Capítulo 4 — Sprites: recortar y pintar con transparencia

**Qué vas a conseguir:** tanques y balas encima del mapa, y un tanque que sale
por los bordes sin romper nada.

**Qué código real se usa:** `src/bmp.c`, `header/players.h`.

```
make
chap04
```

---

## 1. Una sola imagen para todos los dibujos

Todos los sprites del juego están en un único fichero, `res/sprites.bmp`, en
una rejilla:

```
   +--------------------------------------------------+
   | tanque1 arriba | tanque1 abajo | ... | explosion |
   +--------------------------------------------------+
   | tanque2 arriba | tanque2 abajo | ... |   bala    |
   +--------------------------------------------------+
```

Por dos razones:

1. **Un fichero se abre una vez**, no treinta.
2. **Todos comparten paleta por construcción.** Si cada sprite fuera su propio
   BMP con su propia paleta, no podrías mezclarlos en pantalla.

## 2. Recortar

```c
bmp_extract_sprite(2, 5, TANK_WIDTH, TANK_HEIGHT, tank_up);
```

Coge un rectángulo de 18x18 que empieza en (2,5) de la hoja y lo copia a
`tank_up`, **empaquetado**: en el destino las filas van seguidas de 18 en 18,
no de 320 en 320.

```
   En la HOJA (320 de ancho):        En el DESTINO (18 de ancho):

   ....XXXXXXXX................       XXXXXXXX
   ....XXXXXXXX................       XXXXXXXX
   ....XXXXXXXX................       XXXXXXXX
       ^ el sprite
```

`TANK_WIDTH` y `TANK_HEIGHT` salen de `header/players.h`, el header real. Nada
de números copiados a mano.

**No todos los sprites miden lo mismo**: el tanque es 18x18, la bala 4x3, la
explosión 13x13. Por eso el ancho y el alto son parámetros.

## 3. El detalle que sorprende: la hoja no está en memoria

Fíjate en que se llama `bmp_open_sprite_sheet()`, no "load".

El juego **abre el fichero y lee cada sprite directamente del disco**, una fila
cada vez. La hoja completa (64.000 bytes) nunca llega a existir en memoria.

Eso no fue así siempre. Se cambió por una razón muy concreta de gestión de
memoria que está contada entera en el
[manual de la cámara, parte 9.4](../../../doc/ES/MANUAL-CAMARA.md). Resumen: esos
64.000 bytes convivían con el mapa de 256.000 y al liberarlos dejaban un
agujero en medio de la memoria que impedía cargar el último sonido.

## 4. La transparencia es el color 0

Un sprite es un rectángulo, pero un tanque no lo es. ¿Qué pasa con las esquinas?

```c
	pixel = sprite[src_offset];
	if(pixel != 0) {
		dest_buffer[dest_offset] = pixel;
	}
```

**El color 0 no se pinta.** Se salta, y queda lo que hubiera debajo.

Por eso en el juego el índice 0 de la paleta está reservado y no se usa para
dibujar nada de verdad: es el "color" que significa "aquí no hay nada".

Es lo que hace que el mapa se vea entre las esquinas del tanque en vez de un
cuadrado de fondo.

## 5. El recorte, y por qué importa tanto

La segunda mitad del programa pasea el tanque de un borde al otro, **saliéndose
por los dos lados**, con coordenadas negativas y mayores que 320.

Sin recorte, eso es catastrófico. Y no por lo que parece:

```c
	/* La version ANTIGUA, con parametros sin signo */
	unsigned int dest_x
```

Si el tanque está en x = -9 y `dest_x` es `unsigned int` de 16 bits, **-9 no
vale -9: vale 65527**. Entonces:

```
   dest_offset = fila * 320 + 65527
```

Eso son unos 65.000 bytes fuera del buffer de pantalla. Y en DOS **no hay
protección de memoria**: la escritura ocurre y machaca lo que haya. Puede ser
otro buffer, tu propio código, o la tabla de vectores de interrupción.

El síntoma no es un error: es que el juego se cuelga cinco minutos después, en
un sitio sin relación con el bug.

El arreglo son dos cosas:

1. **Parámetros con signo** (`int dest_x`), para que -9 sea -9.
2. **Calcular qué parte del sprite cae dentro** antes de los bucles, y recorrer
   solo esa.

Los detalles están en el
[manual de la cámara, parte 4](../../../doc/ES/MANUAL-CAMARA.md).

## 6. Por qué `int` y no `long`

Duda que sale sola: si las coordenadas pueden ser negativas, ¿por qué no usar
`long` y quedarse tranquilo?

Porque el 8086 **no tiene aritmética de 32 bits**. Cada operación con `long` se
convierte en varias instrucciones, y las multiplicaciones y divisiones en
llamadas a rutinas de librería.

Un `int` de Turbo C llega a **32.767**. El mundo más grande del juego mide 640.
Sobran 32.000.

## 7. Experimentos

1. **Cambia el `if(pixel != 0)` por `if(pixel != 15)`.** Ahora lo transparente
   es el blanco y el tanque sale con un cuadrado negro alrededor.
2. **Recorta en coordenadas equivocadas**, por ejemplo `bmp_extract_sprite(10, 10, ...)`.
   Vas a ver un tanque cortado con trozos del de al lado. Así se ve que la hoja
   es una rejilla y que las coordenadas tienen que ser exactas.
3. **Recorta 40x40 en vez de 18x18.** Te llevas el sprite y sus vecinos.
4. **Comenta el `bmp_close_sprite_sheet()`.** No se rompe nada, pero dejas un
   fichero abierto. DOS da pocos manejadores; con veinte así te quedas sin.

## 8. Lo que hay que llevarse

| | |
|---|---|
| Todos los sprites en una hoja | Un fichero y una paleta |
| Recortar = copiar empaquetando | En el destino el paso de fila es el del sprite |
| **El color 0 no se pinta** | Eso es la transparencia |
| **Coordenadas con signo + recorte** | Sin eso, corrupción de memoria silenciosa |
| `int`, no `long` | El 8086 no tiene 32 bits |

---

**Anterior:** [Capítulo 3](../../ch03/doc/README.md) ·
**Siguiente:** [Capítulo 5 — El teclado](../../ch05/doc/README.md)
