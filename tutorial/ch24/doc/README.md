# Capítulo 24 — El radar de cercanía: números con sprites

*[English version](README-EN.md)*

El capítulo 22 te dio una cámara que sigue a **tu** tanque. Eso resolvió un
problema y creó otro: en un mundo de cuatro pantallas, el otro tanque está casi
siempre fuera de lo que ves, y encontrarlo es dar vueltas al azar hasta
tropezarte con él.

Este capítulo lo arregla con un número en la parte de abajo de la pantalla, de
**000%** a **100%**, que dice cómo de cerca está el enemigo.

```
make
chap24
```

| Tecla | |
|---|---|
| flechas | mueven el tanque **azul** (el que sigue la cámara) |
| W A S D | mueven el tanque **rojo**, para que veas cambiar el número |
| **T** | quita y pone el contorno negro |
| ESC | salir |

---

## 1. Por qué con sprites y no con `printf`

En el modo 13h **no hay texto**. La pantalla es un buffer de 64.000 bytes donde
cada byte es un píxel, y la BIOS no tiene ninguna función para escribir una
letra ahí que no sea pintarla píxel a píxel.

Así que un número en pantalla es exactamente lo mismo que un tanque: **un
sprite recortado de un BMP**. Lo único nuevo es que hay que decidir *cuál* de
los once dibujar en cada sitio.

## 2. La hoja de números

`res\Numbers\<TEMA>\numbers.bmp` es una hoja de 320x200 **igual que
`sprites.bmp`**: 54 bytes de cabecera, la paleta en el 54, los píxeles en el
1078. Se abre y se recorta con las mismas dos funciones del capítulo 4.

Lo único que hay que saber es la rejilla:

```
   +--------+--------+--------+--------+     +--------+
   |   0    |   1    |   2    |   3    | ... |   %    |
   | 18x18  | 18x18  | 18x18  | 18x18  |     | 18x18  |
   +--------+--------+--------+--------+     +--------+
   x=0      x=18     x=36     x=54           x=180
```

**Once celdas de 18x18 en una sola fila**, la celda N empieza en `x = N * 18`.
Las diez cifras en orden y el `%` al final.

Y un detalle que decide una constante más abajo: dentro de su celda de 18, la
tinta de cada cifra ocupa **las filas 2 a 15** y entre 8 y 14 columnas:

| | Tinta (ancho real) |
|---|---|
| las cifras | de 8 px (el `1`) a 13 px |
| el `%` | 13 o 14 px |
| la celda | **18 px** |

O sea que **cada celda tiene aire a los lados**. Si dibujas las cifras a 18
píxeles de distancia, el número se lee como tres cifras sueltas en vez de como
un número. Por eso existe esto en `header\players.h`:

```c
#define NUMBER_WIDTH 			18
#define NUMBER_HEIGHT 			18
#define NUMBER_TOTAL_SPRITES 	11
#define NUMBER_PERCENT_CELL 	10

#define NUMBER_ADVANCE 			13
```

`NUMBER_ADVANCE` **no es** `NUMBER_WIDTH`, y ahí está la gracia: las celdas se
solapan cinco píxeles y no pasa nada, porque el color 0 es transparente y solo
se escribe la tinta.

```
   a 18 (el ancho de la celda):     0   5   0   %      <- tres cifras sueltas
   a 13 (NUMBER_ADVANCE):          050%               <- un numero
```

## 3. Los once punteros, y dónde viven

```c
extern char *number_0;
extern char *number_1;
/* ... */
extern char *number_percent;
```

Declarados en `header\players.h`, **definidos en `src\main.c`** (y en este
capítulo, en `chap24.c`). Un `extern` dice *"esto existe en alguna parte"*, y el
enlazador lo busca donde esté.

### Por qué NO están dentro de `struct player`

Era la primera idea y es la equivocada. El radar es **un cartel para toda la
pantalla**, no algo que tenga cada tanque. Metidos en la struct habría **dos
juegos idénticos** de once sprites, uno por jugador:

| | Bytes |
|---|---|
| Un juego de 11 sprites de 18x18 | 3.564 |
| Dentro de `struct player`, con dos jugadores | **7.128** |

El doble, para dibujar exactamente lo mismo. En una máquina de 640 KB eso no se
hace.

## 4. Reservarlos: el patrón **todo o nada**

```c
	char **target[NUMBER_TOTAL_SPRITES];
	unsigned int cell;

	target[0]  = &number_0;
	target[1]  = &number_1;
	/* ... */
	target[NUMBER_PERCENT_CELL] = &number_percent;

	for (cell = 0; cell < NUMBER_TOTAL_SPRITES; cell++){

		*target[cell] = (char *)malloc(NUMBER_WIDTH * NUMBER_HEIGHT);

		if (*target[cell] == NULL){
			free_sprite_numbers();     /* devolver lo que ya habia */
			return 0;
		}

	}
```

Ese `char **target[11]` es un **array de punteros a puntero**: cada casilla
guarda *la dirección de una de las variables globales*. Así el bucle puede
escribir en `number_0`, `number_1`... sin once líneas repetidas de `malloc` con
su `if` cada una.

Y lo importante es el `free_sprite_numbers()` dentro del `if`: **o están los
once o no está ninguno**. Un juego a medias tendría un `NULL` en el medio, el
código lo dibujaría y se colgaría. Así, si falla la memoria, simplemente no hay
radar y el juego sigue igual que antes de que el radar existiera.

Luego el recorte es otro bucle, con el cortador de siempre:

```c
	bmp_open_sprite_sheet(file);

	for (cell = 0; cell < NUMBER_TOTAL_SPRITES; cell++){
		bmp_extract_sprite(cell * NUMBER_WIDTH, 0,
		                   NUMBER_WIDTH, NUMBER_HEIGHT,
		                   *target[cell]);
	}

	bmp_close_sprite_sheet();
```

## 5. Cuándo, en el arranque

En el juego, `init_sprite_numbers()` se llama **justo después de
`init_graphics()`**, y hay tres razones:

1. **El tema no se decide hasta ahí.** `theme_folder` se elige dentro de
   `init_graphics()`, y sin tema no se sabe qué `numbers.bmp` abrir.

2. **Solo hay un `FILE *`.** `bmp_open_sprite_sheet()` trabaja sobre una única
   variable global en `src\bmp.c`, y no se suelta hasta que `init_graphics()`
   ha recortado el último tanque. Abrir `numbers.bmp` antes se llevaría por
   delante el manejador de la hoja de tanques.

3. **Lo grande primero.** Son 3.564 bytes pedidos *después* de los 256.000 del
   mapa. Ésa es la regla del capítulo 23, y aquí se cumple sola.

Y el `free` va al final, junto a los `player_free()`.

## 6. La paleta: por qué el radar necesita un tema

Ésta es la parte que no se ve venir.

El DAC se carga con la paleta **del mapa**. Y `numbers.bmp` está pintado con la
paleta de **su** tema. Son la misma, así que las cifras salen del color que el
artista quiso.

Pero si mezclas, no. Comparando entrada por entrada:

| | Entradas de paleta distintas |
|---|---|
| `numbers.bmp` de SKYNET contra `map_sky.bmp` | **0** de 256 |
| `numbers.bmp` de SKYNET contra `big.bmp` (el mapa original, sin tema) | **254** de 256 |

Con un mapa sin tema, los índices de color de las cifras caerían sobre colores
que no tienen nada que ver y el número saldría de colores al azar. Por eso en el
juego el radar **solo existe con `-sky`, `-war` o `-neon`**, y con `/bigmap` a
secas no se dibuja nada. No es una limitación: es que no habría nada que leer.

## 7. **La idea del capítulo: la resta que no se hace**

Desde el capítulo 20, **todo** lo que se dibuja lleva la misma cuenta:

```
   pantalla = mundo - camara
```

```c
	draw_sprite_to_buffer(sprite, TANK_WIDTH, TANK_HEIGHT,
	                      (int)tank.position_x - camera_x,     /* <-- */
	                      (int)tank.position_y - camera_y,     /* <-- */
	                      buffer_background_image_data);
```

Los tanques la llevan. Las balas la llevan. La explosión la lleva.

**El radar no.**

```c
	draw_sprite_to_buffer(figure[cell], NUMBER_WIDTH, NUMBER_HEIGHT,
	                      RADAR_X + (cell * NUMBER_ADVANCE),   /* sin camara */
	                      RADAR_Y,                             /* sin camara */
	                      buffer_background_image_data);
```

Y no es un olvido: **el radar no está *en* el mundo, está en la pantalla.** Su
sitio son las mismas cuatro celdas pase lo que pase.

Esto es lo que se llama un **HUD** (*heads-up display*), y la regla que lo
separa de todo lo demás es exactamente ésta:

> Si algo tiene una posición en el mundo, resta la cámara.
> Si está pegado a la pantalla, no.

Es la misma idea del capítulo 22 vista del otro lado. Allí decíamos: *"si la
cámara decide dónde va algo, ese algo es decoración"*. El radar es decoración
pura.

### Dónde se pone en la pantalla

```c
#define RADAR_DIGITS			3
#define RADAR_CELLS				(RADAR_DIGITS + 1)
#define RADAR_WIDTH				(((RADAR_CELLS - 1) * NUMBER_ADVANCE) + NUMBER_WIDTH)

#define RADAR_MARGIN_BOTTOM		2
#define RADAR_X					((WIDTH - RADAR_WIDTH) / 2)
#define RADAR_Y					(HEIGHT - NUMBER_HEIGHT - RADAR_MARGIN_BOTTOM)
```

`RADAR_WIDTH` **no** es `4 * 18`. Las tres primeras celdas solo avanzan 13, pero
la última ocupa su ancho entero:

```
   |<-13->|<-13->|<-13->|<----18---->|
   [  0   ][  0   ][  9  ][     %    ]
   |<--------- 57 pixeles ---------->|
```

De ahí `RADAR_X = (320 - 57) / 2 = 131` y `RADAR_Y = 200 - 18 - 2 = 180`. Todo
son **constantes**: el compilador las calcula una vez y en tiempo de ejecución
no se divide nada.

Y se dibuja **el último de todo**, después de tanques y balas, para que nada
pueda pintarse encima.

## 8. De un número a cuatro sprites

```c
	value = percent;

	for (cell = RADAR_DIGITS - 1; cell >= 0; cell--){
		figure[cell] = digit[value % 10];
		value = value / 10;
	}

	figure[RADAR_DIGITS] = number_percent;
```

De derecha a izquierda, que es como se saca un número a cachos:

- `% 10` da la cifra de las unidades
- `/ 10` tira esa cifra y deja el resto

Con **50**:

| Vuelta | `value` | `value % 10` | Va a | `value / 10` |
|---|---|---|---|---|
| 1 | 50 | **0** | celda 2 | 5 |
| 2 | 5 | **5** | celda 1 | 0 |
| 3 | 0 | **0** | celda 0 | 0 |

Resultado: `[0][5][0][%]` → **050%**

### Por qué tres cifras siempre, con ceros a la izquierda

El cero de la izquierda **sale solo**: el bucle da tres vueltas pase lo que
pase, y cuando `value` ya es 0, `0 % 10` sigue siendo 0. No hay que rellenar
nada a mano.

Y es lo que se quiere. Si el número cambiase de ancho al pasar de 9 a 10, un
número centrado **saltaría de sitio**:

```
   ancho variable:      9%      ->     10%     ->    100%
                      (centrado)     (centrado)    (centrado)
                         ^ cada salto mueve el cartel

   ancho fijo:         009%     ->     010%     ->   100%
                         ^ el cartel no se mueve nunca
```

Un cartel que salta es un cartel que miras en vez de jugar.

## 9. El algoritmo de cercanía

### Primero: sin coma flotante

La distancia de verdad entre dos puntos es:

```
   distancia = raiz(dx*dx + dy*dy)
```

Esa raíz cuadrada es un `float`, y en este proyecto **no hay ni un solo
float**. Meter uno obliga a Turbo C a enlazar su librería de coma flotante
entera dentro de un programa que está contando sus bytes. Por un cartel, no.

La aproximación clásica de enteros es:

```
   distancia = mayor + (menor / 2)
```

```c
	if (dx < dy){
		swap = dx;
		dx = dy;
		dy = swap;
	}

	distance = (long)dx + ((long)dy / 2L);
```

Se queda a un **11%** de la distancia real y cuesta una comparación, una suma y
un desplazamiento.

### Las otras candidatas, y por qué no

| Fórmula | Nombre | Qué hace mal **aquí** |
|---|---|---|
| `\|dx\| + \|dy\|` | Manhattan | Castiga las diagonales: un tanque en diagonal parece mucho más lejos que uno en recto a la misma distancia real |
| `maximo(\|dx\|,\|dy\|)` | Chebyshev | Lo contrario: premia las diagonales |
| `mayor + menor/2` | la elegida | Se parece a la distancia de verdad en todas las direcciones |

Y la diagonal importa mucho, porque los dos tanques arrancan **en esquinas
opuestas**: el otro está casi siempre en diagonal.

Medido ejecutando el código de verdad:

| Situación | Lectura |
|---|---|
| 100 px en horizontal | **88%** |
| 100 px en vertical | **88%** |
| 100 px en diagonal | **87%** |
| 100 px en horizontal, del revés | **88%** |

Un punto de diferencia entre recto y diagonal. Eso es lo que se busca.

### La escala

```c
	if (map_width > map_height){
		worst_distance = (long)(map_width  - TANK_WIDTH)
		               + ((long)(map_height - TANK_HEIGHT) / 2L);
	}else{
		worst_distance = (long)(map_height - TANK_HEIGHT)
		               + ((long)(map_width  - TANK_WIDTH) / 2L);
	}

	percent = 100L - ((distance * 100L) / worst_distance);
```

La misma fórmula sobre el mundo entero, sacada de `map_width` y `map_height`.
**No hay ningún 640 escrito a mano**: el día que cargues un mapa de otro tamaño,
el radar se recalibra solo. Y el `- TANK_WIDTH` está porque un tanque es una
caja y no un punto: su esquina nunca puede llegar al borde de verdad.

Con el mapa de 640x400 sale una escala así:

| Situación | Lectura |
|---|---|
| Esquina a esquina | 7% |
| Los dos *spawns* del juego | **50%** |
| Pegados de lado (18 px) | 98% |
| Uno encima del otro | 100% |

### Trampa 1: el cast a `int` **antes** de restar

```c
	dx = (int)a->position_x - (int)b->position_x;
```

`position_x` es `unsigned int`. Si el tanque A está **a la izquierda** del B, la
resta en unsigned no da negativo: **da la vuelta por abajo** y sale 65.000 y
pico.

```
   sin cast:    100 - 300  ->  65336   ->  distancia enorme  ->  0%
   con cast:    100 - 300  ->    -200  ->  abs = 200         ->  bien
```

El radar marcaría 0% cada vez que los tanques estuviesen del revés. Es la misma
trampa del recorte de sprites del capítulo 4, y del `player_add_offset()` del
capítulo 7.

### Trampa 2: la cuenta tiene que ir en `long`

```c
	percent = 100L - ((distance * 100L) / worst_distance);
```

`distance * 100` llega a unos **78.000** en el mapa de 640x400. Un `unsigned
int` de 16 bits se queda en **65.535**.

En 16 bits, esa multiplicación **da la vuelta**, el porcentaje sale disparatado
y el radar marca un alegre **100% en la otra punta del mundo**: justo lo
contrario de lo que tiene que decir. Por eso las tres variables son `long` y el
literal se escribe `100L`.

## 10. El contorno

El radar **no tiene fondo propio**: cae sobre el trozo de mapa que la cámara
esté enseñando en ese momento. Sobre el suelo oscuro de SKYNET las cifras se
leen de maravilla; sobre el muro de piedra clara de MILITAR casi desaparecen.

Y eso no se arregla eligiendo bien el sitio, porque el sitio lo elige el
jugador moviéndose.

La solución es un **contorno negro**, y para dibujarlo hace falta una función
nueva en `src\bmp.c`:

```c
	if(sprite[src_offset] != 0) {
		dest_buffer[dest_offset] = color;     /* <-- un color plano */
	}
```

`draw_sprite_silhouette_to_buffer()` es `draw_sprite_to_buffer()` palabra por
palabra, recorte incluido, **cambiando un solo byte**: en vez de copiar el color
del sprite, escribe el color que le pasas.

> La **forma** del sprite dice **dónde** escribir.
> El parámetro `color` dice **qué** escribir.

Se dibuja cuatro veces, un píxel a cada lado:

```c
static int radar_outline_x[4] = { -1,  1,  0,  0 };
static int radar_outline_y[4] = {  0,  0, -1,  1 };
```

Las diagonales no: costarían la mitad más para un grosor que a este tamaño no se
ve.

### Y el color 0 no es un problema

El contorno se dibuja en el **color 0**, que es el transparente. Suena a error y
no lo es:

- **transparente** es lo que se lee **del sprite**
- **0** es lo que se escribe **en la pantalla**, y ahí es un negro normal

Y se eligió el 0 porque es **negro puro en la paleta de los tres temas**,
comprobado entrada por entrada. Así no hace falta un color de contorno distinto
para cada tema.

### Las dos pasadas no se pueden juntar

```c
	for (cell ...) { los 4 contornos }      /* pasada 1: TODOS los contornos */
	for (cell ...) { la cifra }             /* pasada 2: TODAS las cifras   */
```

Las cifras van a 13 píxeles de distancia y su tinta llega a medir 14, así que
cada celda **pisa un poco la anterior**. Si cada cifra se contorneara y se
pintara antes de pasar a la siguiente, el contorno negro de una se comería el
borde derecho de la que ya estaba pintada:

```
   bien:   contorno contorno contorno contorno
           cifra    cifra    cifra    cifra

   mal:    contorno cifra  contorno cifra  ...
                           ^ este contorno pisa la cifra de antes
```

Pulsa **T** en el programa del capítulo y muévete hasta el muro de abajo: se ve
en un segundo.

## 11. El radar y la red: no existe

El radar sale de dos posiciones que **las dos máquinas ya simulan igual**, así
que las dos calculan el mismo número sin que cruce un solo byte por el cable.

Y como es decoración, **no entra en el checksum** del capítulo 19. Misma regla
que `camera_x`:

> Si lo decide el código de dibujo, no entra en el estado del juego.

Si entrara, no pasaría nada malo *hoy*... hasta el día que alguien cambiara la
fórmula en una máquina y no en la otra, y el juego reportara una
desincronización por un cartel.

## 12. Experimentos

1. **Pulsa T** y ponte encima del muro de abajo. Ésa es la razón de existir del
   contorno.
2. **Pon `NUMBER_ADVANCE` a 18** y recompila. Verás las tres cifras sueltas en
   vez de un número.
3. **Quita el `(int)` de la resta** de `compute_proximity_percent()` y muévete
   con el tanque rojo a la izquierda del azul. El radar se cae a 0%.
4. **Cambia el `100L` por `100`** y vete a la otra punta del mapa. Ahí está el
   desbordamiento de 16 bits.
5. **Cambia la fórmula por `dx + dy`** (Manhattan) y compara una diagonal con
   una recta a la misma distancia.
6. **Quita la resta de la cámara** a los tanques y dásela al radar. Verás las
   dos cosas mal a la vez, que es la mejor manera de entender la regla.

## 13. Lo que hay que llevarse

| | |
|---|---|
| En el modo 13h **no hay texto** | Un número son sprites, como todo |
| Una hoja de N celdas iguales | `celda N empieza en N * ancho` |
| El **avance** no es el ancho de la celda | La tinta no llena la celda |
| **HUD = no restar la cámara** | Si está pegado a la pantalla, no está en el mundo |
| `% 10` y `/ 10` | Sacar las cifras de un número, de derecha a izquierda |
| Ancho fijo con ceros a la izquierda | Un cartel que no salta |
| `mayor + menor/2` | Una distancia decente sin raíz y sin `float` |
| Cast a `int` **antes** de restar `unsigned` | O la resta da la vuelta |
| `long` en cuanto multiplicas por 100 | 16 bits se acaban en 65.535 |
| La silueta en un color plano | La forma dice dónde, el color dice qué |
| Decoración fuera del checksum | Misma regla que la cámara |

---

## Fin del curso

Ahora sí. Has visto el camino entero: de una pantalla en negro a dos tanques
peleando en red por un mundo de cuatro pantallas, con un cartel que te dice si
te estás acercando.

Para profundizar:

- [Manual de la cámara](../../../doc/ES/MANUAL-CAMARA.md) — 60 páginas sobre los capítulos 20-24
- [Manual de red](../../../doc/ES/MANUAL-RED.md) — lo mismo para los capítulos 15-19
- [Tutorial de sonido](../../../doc/ES/TUTORIAL-SONIDO.md) — para reusar `sound.c`
- [Tutorial de red](../../../doc/ES/TUTORIAL-RED.md) — para reusar `net.c`

---

**Anterior:** [Capítulo 23](../../ch23/doc/README.md) ·
**Índice:** [El curso](../../README.md)
