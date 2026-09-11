# Capítulo 22 — La máscara de bits y la memoria de DOS

**Último capítulo.** Va de la pieza que hace que el mapa grande **quepa**, y de
la lección más cara de todo el proyecto.

```
make
chap22
```

En modo texto, para poder leer los números.

---

## 1. El presupuesto

DOS en modo real tiene **640 KB**, y de ahí sale todo: el sistema, los drivers,
los TSR de red, tu código, tu pila y todo lo que reserves.

En la máquina de pruebas de este proyecto quedaban **571.344 bytes** libres al
arrancar. Ese es el presupuesto entero.

`coreleft()` y `farcoreleft()` te lo dicen. En el modelo huge de Turbo C
devuelven lo mismo: **son el mismo depósito**.

## 2. El techo de 64 KB

`malloc()` recibe un `size_t`, que en Turbo C es de **16 bits**. El número más
grande que cabe ahí es **65.535**.

```c
	malloc(64000);    /* bien */
	malloc(256000);   /* imposible: no se puede ni pedir */
```

Para bloques mayores está `farmalloc()`, que recibe un `unsigned long`. Por eso
el mapa se pide con `farmalloc` y todo lo demás con `malloc`.

## 3. Y el puntero tiene que ser `huge`

En el 8086 una dirección son dos números de 16 bits:

```
   fisica = segmento * 16 + desplazamiento
```

La aritmética de un puntero `far` **solo toca el desplazamiento**. Y el
desplazamiento son 16 bits, así que al pasar de 65.535 **da la vuelta a cero**
en vez de llevarse una al segmento.

En un buffer de 64.000 da igual, nunca llegas. En uno de 256.000 das la vuelta
cuatro veces y lees basura.

Un puntero **`huge`** se **normaliza** en cada operación: el compilador ajusta
segmento y desplazamiento para que el desplazamiento quede siempre entre 0 y 15.

```c
extern unsigned char huge *buffer_original_background_bmp;
```

Y de ahí viene un detalle de `bmp_draw_world_window()` que si no parece
absurdo: la dirección se **reconstruye desde la base** en cada vuelta del bucle
en vez de irse acumulando. Al reconstruirla, Turbo C la normaliza, y entonces el
`memcpy` de 320 bytes que viene detrás no puede cruzar la frontera del segmento.

## 4. Un bit por píxel

El mapa de colisiones tiene que cubrir el mundo **entero**, no solo lo que se
ve: en red las dos máquinas simulan los dos tanques, así que tu máquina tiene
que saber si el tanque del otro, en una sala que no ves, ha chocado.

A un byte por píxel serían **256.000 bytes**. No caben.

Pero mira lo que se le pregunta a ese mapa:

```c
	if (bmp_is_wall(x, y) == 1)
```

**Solo hay dos respuestas posibles.** De los 256 valores que caben en un byte te
importa uno. Estás gastando 8 bits para un sí/no.

```
   Un byte por pixel (8 pixeles = 8 bytes):
     [00] [00] [FF] [FF] [00] [00] [00] [FF]

   Un bit por pixel (8 pixeles = 1 byte):
     [ 00110001 ]
```

| | Bytes |
|---|---|
| 640x400 a 1 byte/px | 256.000 |
| 640x400 a **1 bit/px** | **32.000** |

Y fíjate bien: son **la mitad** de lo que costaba el mapa de colisiones de **una
sola pantalla** antes (64.000). El mundo entero ocupa menos que una pantalla.

El coste de leer un bit es un desplazamiento y dos ANDs. Nada.

## 5. La lección cara: fragmentación

Durante el desarrollo, el juego cargaba todo **menos el último efecto de
sonido**:

```
Sound: could not load died.wav
```

Y la cuenta decía que **debería caber**: quedaban **129.982 bytes libres** y el
fichero pide **42.090**.

No faltaba memoria. **La memoria libre estaba en el sitio equivocado.**

El orden de arranque era:

```
  1. farmalloc(256000)   el mapa
  2. malloc(64000)       la hoja de sprites
  3. recortar los sprites
  4. free(64000)         soltarla        <- DEJA UN AGUJERO
  5. farmalloc(42090)    el WAV          <- no lo encuentra
```

```
   +----------------------------------------------------+
   |  MAPA 256000 | agujero 64000 | pantalla |  libre   |
   +----------------------------------------------------+
                   ^^^^^^^^^^^^^^
                   libre, pero enterrado en medio
```

De los 129.982 libres, 64.000 estaban en ese agujero y el resto arriba del todo.
**Y no están pegados.**

> ## MEMORIA LIBRE TOTAL NO ES MEMORIA LIBRE CONTIGUA.

## 6. El intento que lo empeoró

El primer arreglo fue cargar el sonido **antes** que el mapa. El sonido cargó
perfectamente... **y entonces el mapa no cupo**: quedaban 247.371 bytes
contiguos y pedía 256.000. Faltaban 8.629.

Y como `farmalloc` devolvió NULL y el código siguió con un puntero nulo, el
resultado fue el fondo desaparecido y los tanques dibujados sobre basura.

El log lo decía con toda claridad, pero había que saber leerlo:

```
Sound: loaded, 375680 bytes left        <- los cuatro WAV cargaron
Map 640x400  memory now: near 246800    <- pero init_graphics solo gasto 128880
```

`init_graphics` consumió 128.880 bytes cuando el mapa solo ya son 256.000. **La
resta no engaña: el mapa nunca se reservó.**

## 7. El arreglo bueno: quitar el agujero

No mover cosas de sitio. **Quitar el agujero.**

La hoja de sprites era un buffer de 64.000 bytes del que se recortaban los
sprites al arrancar y que después no se leía nunca más. Se eliminó del todo:
`sprites.bmp` **se abre y cada sprite se lee directamente del fichero** (eso es
lo que viste en el capítulo 4).

- Se ahorran 64.000 bytes
- **No se libera nada nunca**, así que no hay agujero
- Cuesta unos 340 `fseek` al arrancar y nada más

## 8. Las cuatro reglas

1. **Mide, no supongas.** Dos líneas de `coreleft()` en el log valieron más que
   todo el razonamiento sobre cómo debería comportarse el asignador.
2. **Total libre no es contiguo libre.** La lección de verdad.
3. **La reserva más grande, la primera**, sobre un montón limpio. Y si puedes,
   no liberes nada durante la ejecución.
4. **Un fallo silencioso cuesta más que uno ruidoso.** `farmalloc` devolvió
   NULL, el código hizo un `printf` sobre una pantalla en modo gráfico (o sea,
   invisible) y siguió. Por eso el síntoma fue *"se ha roto todo"* en vez de
   *"el mapa no cupo"*.

## 9. Experimentos

1. **Mira los números que imprime.** Compáralos con los 571.344 del texto.
2. **Pide un mapa de 1280x800** en `bmp_init_buffers()`. Son 1.024.000 bytes:
   `farmalloc` devuelve NULL y lo verás.
3. **Añade `coreleft()` a tus propios programas.** Es la costumbre que más
   tiempo ahorra en DOS.

## 10. Lo que hay que llevarse

| | |
|---|---|
| `malloc` no pasa de 65.535 | Para más, `farmalloc` |
| Un puntero `far` **da la vuelta** a los 64 KB | Para más, `huge` |
| **1 bit por píxel**: el mundo entero cabe en 32 KB | Menos que una pantalla antes |
| **Total libre ≠ contiguo libre** | La lección cara |
| Lo grande primero; mejor no liberar nada | |
| **Mide, no supongas** | |

---

## Fin del curso

Has visto el camino entero: de una pantalla en negro a dos tanques peleando en
red por un mundo de cuatro pantallas.

Para profundizar:

- [Manual de la cámara](../../../doc/ES/MANUAL-CAMARA.md) — 60 páginas sobre este bloque
- [Manual de red](../../../doc/ES/MANUAL-RED.md) — lo mismo para los capítulos 15-19
- [Tutorial de sonido](../../../doc/ES/TUTORIAL-SONIDO.md) — para reusar `sound.c`
- [Tutorial de red](../../../doc/ES/TUTORIAL-RED.md) — para reusar `net.c`

---

**Anterior:** [Capítulo 21](../../ch21/doc/README.md) ·
**Índice:** [El curso](../../README.md)
