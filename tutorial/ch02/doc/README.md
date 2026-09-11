# Capítulo 2 — Cargar un BMP y mostrarlo

**Qué vas a conseguir:** el mapa del juego en pantalla, cargado desde disco. Y
verlo primero **con los colores mal**, a propósito, para entender por qué.

**Qué código real se usa:** `src/bmp.c` entero.

```
make
chap02
```

---

## 1. Qué hay dentro de un fichero BMP

Un `.bmp` de 256 colores tiene tres partes, una detrás de otra:

```
   offset 0     +--------------------------+
                |  CABECERA   54 bytes     |  ancho, alto, bits por pixel...
   offset 54    +--------------------------+
                |  PALETA   256 x 4 bytes  |  los colores
   offset 1078  +--------------------------+
                |  PIXELES                 |  un byte por pixel
                +--------------------------+
```

Los números importantes:

| Offset | Qué hay |
|---|---|
| 18 | Ancho, entero de 4 bytes |
| 22 | Alto, entero de 4 bytes |
| 28 | Bits por píxel (aquí siempre 8) |
| **54** | Donde empieza la paleta |
| **1078** | Donde empiezan los píxeles |

El 1078 sale de `54 + 256*4`. Por eso está fijo en el código: mientras la
imagen sea de 8 bits con 256 colores, siempre es ahí.

Cada entrada de paleta son **4 bytes**, no 3: azul, verde, rojo y uno de
relleno que no se usa. Y fíjate en el orden: **BGR, al revés de lo normal.**

## 2. Dos cosas, no una

Esta es la trampa del capítulo, y por eso el programa lo enseña mal a
propósito.

Cargar una imagen son **dos operaciones independientes**:

1. **Los píxeles** → `bmp_fill_background_in_main_buffer()`
2. **La paleta** → `bmp_extract_pallete_from_file()` y después
   `bmp_write_pallete_data_into_dac()`

Si haces solo la primera, el dibujo está ahí, entero y correcto, pero pintado
con la tabla de colores que hubiera puesta antes. Eso es lo que ves en la
primera pantalla del programa.

Y luego, sin redibujar **ni un solo píxel**, se carga la paleta buena y la
imagen aparece bien:

```c
bmp_write_pallete_data_into_dac(buffer_palleta_data);
```

**Los 64.000 bytes de la pantalla no han cambiado.** Solo la tabla. Esa es la
demostración de lo que decía el capítulo 1: el byte es un índice.

## 3. Por qué se divide entre 4

Mira `bmp_load_pallete_data()` en `src/bmp.c`:

```c
	fread(&value,1,1,_file);
	a = (value/4);
```

El BMP guarda cada componente de color de **0 a 255**. El DAC de la VGA solo
acepta de **0 a 63** (6 bits por componente). Dividir entre 4 es la conversión.

Si se te olvida, todos los colores saturan y la imagen sale blanca.

## 4. Los BMP están del revés

Segunda trampa, y esta es histórica: **un BMP guarda sus filas de abajo
arriba.** La primera fila del fichero es la última de la imagen.

```
   El fichero:            La imagen:
   fila 0  -------------> fila 199  (la de abajo)
   fila 1  -------------> fila 198
   ...
   fila 199 ------------> fila 0    (la de arriba)
```

Si lo cargas tal cual, ves el dibujo boca abajo.

En `src/bmp.c` esto se resuelve leyendo las filas **hacia atrás**:

```c
	for (row = map_height - 1; row >= 0; row = row - 1){
		fread(map_line, 1, map_width, file);
		destination = buffer_original_background_bmp
		            + ((unsigned long)row * (unsigned long)map_width);
		memcpy(destination, map_line, map_width);
	}
```

El fichero se lee hacia delante y la imagen se escribe hacia atrás. Es el mismo
trabajo que darle la vuelta después, pero sin necesitar un buffer entero de
más.

## 5. Los buffers que reserva el juego

`bmp_init_buffers(WIDTH, HEIGHT)` pide todo esto:

| Buffer | Tamaño | Para qué |
|---|---|---|
| `buffer_original_background_bmp` | ancho x alto | El mapa limpio. **Nunca se dibuja encima** |
| `buffer_background_image_data` | 64.000 | El frame que se está construyendo |
| `buffer_palleta_data` | 309 | Los colores, de camino al DAC |
| `buffer_collision_mask` | ancho x alto / 8 | Los muros (capítulo 7) |

Que haya **dos** copias del mapa (la limpia y la que se dibuja) es lo que
permite borrar los tanques de un frame al siguiente: se vuelve a copiar el
original encima y listo. Sin eso, dejarían un rastro.

## 6. De buffer a pantalla

Dos pasos:

```c
bmp_draw_world_window(buffer_background_image_data);   /* mapa -> buffer */
bmp_paint_image_data_to_vga(buffer_background_image_data);  /* buffer -> VGA */
```

`bmp_draw_world_window()` copia **el trozo visible** del mapa. Con un mundo de
una pantalla, el trozo visible es todo, y equivale a un `memcpy` de 64.000
bytes. En el capítulo 21 dejará de serlo.

## 7. Experimentos

1. **Comenta la línea de la paleta** (`bmp_write_pallete_data_into_dac`). Te
   quedas con la imagen fea para siempre. Ese es el fallo más común al empezar.
2. **Carga la paleta de `sprites.bmp` y los píxeles de `cutre.bmp`.** Como
   comparten paleta en este proyecto, no cambia nada. Prueba con un BMP tuyo
   de otra paleta y verás el estropicio.
3. **Carga `cutrecol.bmp`** en vez de `cutre.bmp`. Es el mapa de colisiones:
   azul donde se puede pasar, amarillo donde hay muro. Es el mismo mapa visto
   por el juego en vez de por ti.
4. **Quita el `bmp_delete_buffers()` del final.** No pasa nada, porque DOS
   recupera la memoria al salir. Pero acostúmbrate a soltarla.

## 8. Lo que hay que llevarse

| | |
|---|---|
| Un BMP es cabecera + paleta + píxeles | Los píxeles empiezan en 1078 |
| **Píxeles y paleta son dos cargas distintas** | Olvidar la segunda es el fallo clásico |
| El DAC quiere 0..63, el BMP da 0..255 | De ahí el `/4` |
| **Los BMP guardan las filas al revés** | Se lee hacia atrás, no se voltea después |
| Hay dos copias del mapa | Una limpia para poder borrar lo dibujado |

---

**Anterior:** [Capítulo 1](../../ch01/doc/README.md) ·
**Siguiente:** [Capítulo 3 — Doble buffer y retrazo](../../ch03/doc/README.md)
