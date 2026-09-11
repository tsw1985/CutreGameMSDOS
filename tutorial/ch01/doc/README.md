# Capítulo 1 — El modo 13h y la memoria de vídeo

**Qué vas a conseguir:** una pantalla en 320x200 con los 256 colores de la
paleta pintados a mano.

**Qué código real del juego se usa:** `src/bmp.c` (solo la variable `vga`).

---

## Compilar y ejecutar

Desde DOS, dentro de esta carpeta:

```
make
chap01
```

Y para limpiar:

```
make clean
```

---

## 1. Qué es un "modo de vídeo"

Una tarjeta VGA puede funcionar de muchas formas distintas: texto de 80x25,
gráficos de 640x480 con 16 colores, gráficos de 320x200 con 256... A cada una
de esas configuraciones se le llama un **modo**, y cada una tiene un número.

Al arrancar, DOS pone la tarjeta en el **modo 3**: texto, 80 columnas por 25
filas. Es lo que ves cuando escribes `dir`.

Nosotros queremos el **modo 0x13** (se lee "trece hache", en hexadecimal):

| | |
|---|---|
| Resolución | 320 x 200 píxeles |
| Colores | 256 a la vez |
| Bytes por píxel | 1 |
| Memoria que ocupa | 320 x 200 = **64.000 bytes** |

Ese modo es *el* modo de los juegos de DOS, y la razón es la última fila: un
byte por píxel y todo cabe en un bloque de memoria seguido. Es el más fácil de
manejar que existe.

## 2. Cómo se pide

No hay una función de C para esto. Se le pide a la BIOS con una
**interrupción**: una llamada al sistema de las de antes.

```c
static void set_video_mode(unsigned int mode){

	union REGS regs;

	regs.x.ax = mode;
	int86(0x10, &regs, &regs);

}
```

- `int86()` es de Turbo C. Dispara una interrupción con los registros que le
  pongas.
- `0x10` es la interrupción de **vídeo**.
- Poniendo `AX = 0x0013` le estás diciendo: función 0 (cambiar de modo), modo
  0x13.
- `0x0003` vuelve al texto de siempre.

El juego de verdad hace esto mismo desde ensamblador, en `src/video.asm`,
porque así se escribió al principio. Es exactamente lo mismo.

## 3. La pantalla es memoria. Esto es lo importante

Aquí está la idea que hay que llevarse del capítulo.

**No hay ninguna función de dibujar.** En modo 13h, la tarjeta VGA coloca sus
64.000 bytes de pantalla en la dirección **A000:0000**. Escribes un byte ahí y
aparece un píxel. Sin más.

En `src/bmp.c`, línea 15:

```c
unsigned char *vga = (unsigned char *) MK_FP(0xA000,0);
```

`MK_FP` significa *make far pointer*: construye un puntero a partir de un
segmento (0xA000) y un desplazamiento (0). A partir de ahí, `vga` es un array
de 64.000 bytes que resulta que es la pantalla.

## 4. Dónde está el píxel (x, y)

La memoria es una tira. La pantalla es un rectángulo. La conversión es:

```
posicion = y * 320 + x
```

```
    x=0                    x=319
  y=0  [0][1][2] ......... [319]
  y=1  [320][321] ........ [639]
  y=2  [640] .............. [959]
   .
  y=199 [63680] .......... [63999]
```

Ese `* 320` tiene nombre: se llama **stride** o paso de fila, y es *cuántos
bytes hay que avanzar para bajar una fila*.

Aquí coincide con el ancho de la pantalla, y por eso pasa desapercibido.
**En el capítulo 20 dejará de coincidir**, cuando el mapa sea más ancho que la
pantalla, y ahí es donde se equivoca todo el mundo. Apúntalo desde ya.

## 5. El byte no es un color

La segunda idea del capítulo, y es la que sorprende.

Si escribes `vga[0] = 4;` no estás diciendo "pinta de rojo". Estás diciendo:
**"pinta con el color que haya en la posición 4 de la paleta"**.

La **paleta** es una tabla de 256 entradas que vive dentro de la tarjeta. Cada
entrada tiene tres números (rojo, verde, azul) de 0 a 63. El byte del píxel es
solo un índice a esa tabla.

Eso tiene dos consecuencias enormes:

1. **Cambiando la paleta cambias todos los colores de la pantalla de golpe**,
   sin tocar un solo píxel. Los fundidos a negro de los juegos de la época son
   exactamente eso.
2. **Dos imágenes con paletas distintas no se pueden mezclar.** Si cargas los
   píxeles de una y la paleta de otra, verás los colores cambiados. Por eso
   todos los BMP de este juego comparten paleta byte a byte.

La rejilla de 16x16 que pinta el programa es justamente la paleta por defecto
de la VGA: los 256 colores que la tarjeta trae puestos al arrancar.

## 6. Lee el código

Abre `chap01.c`. Los bucles son deliberadamente tontos:

```c
	for (y = 0; y < 96; y++){
		for (x = 0; x < 320; x++){
			color = (unsigned char)(((y / 6) * 16) + (x / 20));
			offset = (y * WIDTH) + x;
			vga[offset] = color;
		}
	}
```

Píxel a píxel, sin ninguna optimización. Así se ve la idea. Ya habrá tiempo de
hacerlo rápido.

`WIDTH` y `HEIGHT` (320 y 200) vienen de `header/bmp.h`, el header real del
juego. **Nada de números mágicos copiados.**

## 7. Experimentos

Cosas que puedes cambiar y volver a compilar:

1. **Pinta un solo píxel** en el centro: `vga[(100 * 320) + 160] = 15;`
   Búscalo en la pantalla. Es uno, y es diminuto.
2. **Cambia el `* 320` por `* 321`** en el bucle de abajo y mira qué pasa. Vas
   a ver la imagen inclinada: acabas de romper el stride, que es el error del
   que te avisaba el punto 4.
3. **Quita el segundo `getch()`** y compila. El programa vuelve a texto tan
   rápido que no ves nada. Eso te dice que el dibujo es instantáneo.
4. **Escribe fuera de la pantalla:** `vga[70000] = 15;` En DOS no hay
   protección de memoria: no falla, escribe en otro sitio. Si el juego hace
   algo raro después, ya sabes por qué.

## 8. Lo que hay que llevarse

| | |
|---|---|
| Un modo de vídeo se pide a la BIOS con `int 0x10` | |
| **La pantalla es memoria en A000:0000** | La idea central |
| Un píxel está en `y * 320 + x` | Ese 320 es el *stride* |
| **El byte es un índice a la paleta, no un color** | Por eso los BMP tienen que compartir paleta |

---

**Siguiente:** [Capítulo 2 — Cargar un BMP](../../ch02/doc/README.md)
