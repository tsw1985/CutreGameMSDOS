# Capítulo 23 — La máscara de bits y la memoria de DOS

*[English version](README-EN.md)*

**Último capítulo del bloque del mapa grande.** Va de la pieza que hace que el
mapa grande **quepa**, y de la lección más cara de todo el proyecto.

```
make
chap23
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

## 5. Cómo se lee un bit

Aquí está el código, que es lo que hay que ver:

```c
int bmp_is_wall(int x, int y){

	unsigned long bit_index;
	unsigned int  byte_index;
	unsigned char bit;

	/* fuera del mapa cuenta como muro: red de seguridad */
	if (x < 0 || y < 0 || x >= map_width || y >= map_height){
		return 1;
	}

	bit_index  = ((unsigned long)y * (unsigned long)map_width) + (unsigned long)x;
	byte_index = (unsigned int)(bit_index >> 3);
	bit        = (unsigned char)(1 << (unsigned int)(bit_index & 7L));

	if ((buffer_collision_mask[byte_index] & bit) != 0){
		return 1;
	}

	return 0;

}
```

Paso a paso, con el píxel **(100, 50)** de un mundo de 640 de ancho:

| Paso | Qué hace | Con los números |
|---|---|---|
| `bit_index` | Qué número de píxel es, contando desde el principio | 50·640 + 100 = **32.100** |
| `>> 3` | Dividir entre 8: **en qué byte** está | 32.100 / 8 = **4.012** |
| `& 7` | El resto: **qué bit** dentro de ese byte | 32.100 % 8 = **4** |
| `1 << 4` | Construir una máscara con solo ese bit | `00010000` |
| `& ` | Comprobar si está encendido | |

### Por qué `>> 3` y `& 7` y no `/ 8` y `% 8`

Hacen exactamente lo mismo, pero:

- `>> 3` es **un desplazamiento**: una instrucción
- `/ 8` es **una división**: en un 8086, decenas de ciclos

El compilador de Turbo C probablemente convierta `/8` en `>>3` él solo, pero
escribirlo así deja claro que **es una operación de bits**, no aritmética.

Lo mismo con `& 7` en vez de `% 8`. Y funciona porque 8 es potencia de 2: los
tres bits de abajo del número **son** el resto de dividir entre 8.

### Y el multiplicar tampoco es caro

```c
	bit_index = (unsigned long)y * (unsigned long)map_width + x;
```

Eso parece una multiplicación de 32 bits, que en un 8086 sería una llamada a
rutina de librería. Pero no lo es:

El 8086 tiene una instrucción **`MUL`** que multiplica **dos números de 16 bits
y da un resultado de 32 bits**, en una sola instrucción. `y` cabe en 16 bits,
`map_width` también, y el resultado necesita 32. Es exactamente el caso que esa
instrucción resuelve.

El compilador la usa. Así que el coste total de `bmp_is_wall()` frente a leer un
byte suelto es: **un `MUL`, un desplazamiento y dos ANDs.**

Y se llama 3 veces por tanque y frame más una por bala: **8 llamadas por
frame**. Despreciable.

## 6. Fuera del mapa cuenta como muro

```c
	if (x < 0 || y < 0 || x >= map_width || y >= map_height){
		return 1;
	}
```

Esas cuatro líneas no son una regla del juego: son una **red de seguridad**.

Los mapas están dibujados con un borde macizo de 16 a 33 píxeles, así que nunca
debería llegarse ahí. Pero si algún día dibujas un mapa con un agujero en el
borde, el tanque **se para en seco** en vez de que el juego lea memoria que no
es suya y se cuelgue veinte segundos después por una razón incomprensible.

Es la misma filosofía que el recorte del capítulo 4: **que un dato malo produzca
un comportamiento raro pero acotado**, no corrupción silenciosa.

## 7. La lección cara: fragmentación

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

## 8. El intento que lo empeoró

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

## 9. El arreglo bueno: quitar el agujero

No mover cosas de sitio. **Quitar el agujero.**

La hoja de sprites era un buffer de 64.000 bytes del que se recortaban los
sprites al arrancar y que después no se leía nunca más. Se eliminó del todo:
`sprites.bmp` **se abre y cada sprite se lee directamente del fichero** (eso es
lo que viste en el capítulo 4).

- Se ahorran 64.000 bytes
- **No se libera nada nunca**, así que no hay agujero
- Cuesta unos 340 `fseek` al arrancar y nada más

## 10. Las cuatro reglas

1. **Mide, no supongas.** Dos líneas de `coreleft()` en el log valieron más que
   todo el razonamiento sobre cómo debería comportarse el asignador.
2. **Total libre no es contiguo libre.** La lección de verdad.
3. **La reserva más grande, la primera**, sobre un montón limpio. Y si puedes,
   no liberes nada durante la ejecución.
4. **Un fallo silencioso cuesta más que uno ruidoso.** `farmalloc` devolvió
   NULL, el código hizo un `printf` sobre una pantalla en modo gráfico (o sea,
   invisible) y siguió. Por eso el síntoma fue *"se ha roto todo"* en vez de
   *"el mapa no cupo"*.

## 11. Experimentos

1. **Mira los números que imprime.** Compáralos con los 571.344 del texto.
2. **Pide un mapa de 1280x800** en `bmp_init_buffers()`. Son 1.024.000 bytes:
   `farmalloc` devuelve NULL y lo verás.
3. **Añade `coreleft()` a tus propios programas.** Es la costumbre que más
   tiempo ahorra en DOS.

## 12. Lo que hay que llevarse

| | |
|---|---|
| `malloc` no pasa de 65.535 | Para más, `farmalloc` |
| Un puntero `far` **da la vuelta** a los 64 KB | Para más, `huge` |
| **1 bit por píxel**: el mundo entero cabe en 32 KB | Menos que una pantalla antes |
| **Total libre ≠ contiguo libre** | La lección cara |
| Lo grande primero; mejor no liberar nada | |
| **Mide, no supongas** | |

---

## Y ya está el mundo grande entero

Con esto el bloque 5 está cerrado: el mundo, la ventana, la cámara y la memoria
que hace que todo quepa.

Queda una cosa por resolver, y es consecuencia directa de la cámara: si cada uno
sigue a su tanque, **¿cómo encuentras al otro?** De eso va el
[capítulo 24](../../ch24/doc/README.md), que pone un número en pantalla dibujado
con sprites.

Para profundizar:

- [Manual de la cámara](../../../doc/ES/MANUAL-CAMARA.md) — 60 páginas sobre este bloque
- [Manual de red](../../../doc/ES/MANUAL-RED.md) — lo mismo para los capítulos 15-19
- [Tutorial de sonido](../../../doc/ES/TUTORIAL-SONIDO.md) — para reusar `sound.c`
- [Tutorial de red](../../../doc/ES/TUTORIAL-RED.md) — para reusar `net.c`

---

**Anterior:** [Capítulo 22](../../ch22/doc/README.md) ·
**Siguiente:** [Capítulo 24](../../ch24/doc/README.md) ·
**Índice:** [El curso](../../README.md)
