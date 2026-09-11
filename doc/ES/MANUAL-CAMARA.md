# Manual: cómo se hizo la cámara y el mapa grande

De una pantalla fija de 320x200 a un mundo de 640x400 por el que dos tanques se
buscan, cada uno viendo su propio trozo.

> **¿Prefieres verlo funcionando antes de leer?**
> El [curso](../../tutorial/README.md) cubre esto mismo en cuatro capítulos que
> compilan y se ejecutan:
> [**ch20**](../../tutorial/ch20/doc/README.md) el problema ·
> [**ch21**](../../tutorial/ch21/doc/README.md) la ventana ·
> [**ch22**](../../tutorial/ch22/doc/README.md) la cámara ·
> [**ch23**](../../tutorial/ch23/doc/README.md) la memoria.
> Este manual es la referencia profunda; el curso es el camino de subida.

---

## Para quién es esto

Para ti dentro de seis meses, cuando abras `bmp.c` y veas doscientos `memcpy`
seguidos y no te acuerdes de por qué.

Doy por sabido que dominas C, que te manejas en DOS y que **entiendes tu propio
juego**: el bucle principal, cómo se dibuja un sprite, cómo se mueve un tanque.

Doy por no sabido **absolutamente nada de cámaras, scroll ni gestión de
memoria en DOS**. Ni qué es una ventana, ni qué es el recorte, ni por qué un
puntero puede ser `far` o `huge`. Todo eso se explica aquí desde cero.

### Cómo leerlo

Está en **orden de aprendizaje**, no en orden de fichero. Cada parte se apoya
en la anterior:

| Parte | De qué va |
|---|---|
| **1** | El problema. Qué pasa cuando el mapa no cabe en la pantalla |
| **2** | La idea que lo resuelve: dos sistemas de coordenadas |
| **3** | Pintar el fondo: la ventana |
| **4** | Pintar los sprites: el recorte |
| **5** | Mover la cámara: la zona muerta |
| **6** | Por qué el juego normal usa exactamente el mismo código |
| **7** | Las colisiones: la máscara de bits |
| **8** | La red: por qué la cámara no puede ser parte del juego |
| **9** | La memoria: 640 KB, y la tarde que se perdió por 8.600 bytes |
| **10** | Cómo llevarte esto a otro proyecto |
| **11** | Referencia de todo |
| **12** | Errores típicos y glosario |

**No te saltes la parte 2.** Es el 80% de la comprensión. Las partes 3, 4 y 5
son mecánica; sin la 2 son ruido.

---

# PARTE 1 — EL PROBLEMA

## 1.1 La pantalla no es el mundo

Hasta ahora, en este juego, el mapa y la pantalla eran **la misma cosa**.

El modo 13h de VGA da una pantalla de 320 píxeles de ancho por 200 de alto, un
byte por píxel. Eso son exactamente 64.000 bytes. Y el mapa, `cutre.bmp`, medía
320x200. Un byte por píxel. Exactamente 64.000 bytes.

Esa coincidencia hacía que dibujar el fondo fuese la operación más simple que
existe:

```c
memcpy(buffer_background_image_data, buffer_original_background_bmp, 64000);
```

Copia el mapa entero encima del buffer de pantalla entero. No hay nada que
decidir, porque el mapa **es** la pantalla.

Y dibujar un tanque era igual de directo. Si el tanque está en la posición
(110, 164) del mapa, se pinta en la posición (110, 164) de la pantalla. La
misma coordenada sirve para las dos cosas, porque son el mismo espacio.

Esto es muy cómodo y es una trampa: te acostumbras a que "la posición del
tanque" tenga un único significado. En cuanto el mapa crece, deja de tenerlo, y
si no te das cuenta de eso, todo lo demás falla de formas rarísimas.

## 1.2 Qué pasa cuando el mapa crece

Ahora el mapa es `big.bmp`: **640 x 400**. Cuatro veces más grande.

```
        640 pixeles
   +---------------------------+
   |                           |
   |                           |  400
   |                           |  pixeles
   |                           |
   +---------------------------+

        Y la pantalla:

   +--------------+
   |              |  200
   |              |
   +--------------+
        320
```

La pantalla física no ha cambiado. Sigue siendo 320x200, porque eso es lo que
da el hardware. Así que la pregunta es inmediata:

> **De esos 640x400 píxeles, ¿cuáles 320x200 se ven?**

Esa pregunta es, literalmente, toda la cámara. Todo lo demás es consecuencia.

## 1.3 Las tres formas de contestarla

Hay tres modelos clásicos, y elegir mal cuesta caro. Los cuento porque
entenderlos es entender por qué el código hace lo que hace.

### Modelo A — Habitaciones (pantalla fija)

Divides el mapa en trozos de 320x200 exactos. Un mapa de 640x400 son cuatro
"habitaciones":

```
   +------+------+
   |  0   |  1   |
   +------+------+
   |  2   |  3   |
   +------+------+
```

Se muestra una habitación entera. Cuando el tanque cruza el borde, la pantalla
**salta** a la habitación siguiente, de golpe.

Así funcionaban el Zelda del NES, el Bomberman, y media consola de los ochenta.

- **A favor:** simplísimo. El fondo sigue siendo un `memcpy` de 64.000 bytes,
  porque cada habitación es exactamente una pantalla.
- **En contra:** el salto se ve raro; el tanque pasa de estar pegado al borde
  derecho a aparecer en el izquierdo. Y si el tanque se queda justo en la
  frontera moviéndose adelante y atrás, la pantalla **parpadea** entre dos
  habitaciones varias veces por segundo.

### Modelo B — Cámara centrada siempre

El tanque está clavado en el centro de la pantalla y lo que se mueve es el
mundo, debajo de él.

- **A favor:** suavísimo, nunca hay saltos.
- **En contra:** es mareante. Tú crees que estás moviendo tu tanque, y lo que
  ves moverse es **todo lo demás**. En un juego de tanques, donde pasas mucho
  rato haciendo ajustes pequeños de posición, cansa.

### Modelo C — Zona muerta (el elegido)

La cámara está **quieta** mientras el tanque se mueva dentro de un rectángulo
en el centro de la pantalla. Cuando sale de ese rectángulo, la cámara empieza a
empujar, píxel a píxel, lo justo para devolverlo dentro.

- **A favor:** no hay salto, porque el desplazamiento es continuo. Y no hay
  mareo, porque en juego normal **la cámara está parada casi todo el rato**.
- **En contra:** hay que escribirla. Son unas veinte líneas.

En el juego de verdad, midiendo un paseo de 200 frames, **la cámara está quieta
el 88% de los frames**. Eso es exactamente lo que se buscaba: que el mundo solo
se mueva cuando de verdad vas a algún sitio.

Es el modelo C el que está implementado, y el resto del manual lo explica.

---

# PARTE 2 — LA IDEA: DOS SISTEMAS DE COORDENADAS

Esta es **la** parte importante. Si te llevas una sola cosa del manual, que sea
esta.

## 2.1 El mismo punto, dos números distintos

En cuanto el mundo es más grande que la pantalla, la frase "el tanque está en
la posición 400" se vuelve ambigua. ¿400 de qué?

Hay que separar dos espacios distintos:

**Coordenadas de MUNDO.** Dónde están las cosas de verdad, en el mapa completo.
Van de 0 a 639 en horizontal y de 0 a 399 en vertical. Aquí viven **todas las
cosas del juego**: la posición de los tanques, la de las balas, el mapa de
muros. Estas coordenadas no dependen de lo que estés mirando.

**Coordenadas de PANTALLA.** Dónde se pinta una cosa este frame. Van de 0 a 319
y de 0 a 199, porque eso es la pantalla. Solo las usa el código que dibuja, y
solo durante el instante en que dibuja.

```
   MUNDO 640x400
   +--------------------------------+
   |                                |
   |      +--------------+          |
   |      |  PANTALLA    |          |
   |      |              |   T      |   <- tanque en el mundo (500, 150)
   |      |    T         |          |      pero fuera de la ventana
   |      |              |          |
   |      +--------------+          |
   |                                |
   +--------------------------------+
```

El mismo tanque tiene, a la vez, una posición de mundo (que no cambia aunque
muevas la cámara) y una posición de pantalla (que cambia con la cámara aunque
el tanque no se mueva).

**Mezclar los dos es el error número uno.** Produce bugs desconcertantes: las
balas chocan con paredes que no están ahí, los tanques atraviesan muros, y todo
funciona bien mientras no te alejas del origen.

## 2.2 La cámara son dos números. Nada más

La cámara no es un objeto, ni una clase, ni tiene zoom ni rotación. Son dos
enteros que dicen **dónde está la esquina superior izquierda de la ventana**,
en coordenadas de mundo:

```c
extern int camera_x;
extern int camera_y;
```

Si `camera_x = 0` y `camera_y = 0`, ves la esquina de arriba a la izquierda del
mapa. Si `camera_x = 320` y `camera_y = 200`, ves el cuarto de abajo a la
derecha.

Eso es todo lo que es una cámara en 2D. En serio.

## 2.3 La fórmula

Convertir de mundo a pantalla es una resta:

```
pantalla_x = mundo_x - camera_x
pantalla_y = mundo_y - camera_y
```

Y ya está. Esa resta es la cámara entera.

Un ejemplo con números, que es como se entiende:

| | |
|---|---|
| El tanque está en el mundo en | (500, 150) |
| La cámara está en | (320, 100) |
| Luego en pantalla se pinta en | (500-320, 150-100) = **(180, 50)** |

Y si la cámara se mueve a (400, 100) sin que el tanque se mueva:

| | |
|---|---|
| El tanque sigue en el mundo en | (500, 150) |
| La cámara ahora está en | (400, 100) |
| En pantalla se pinta en | (500-400, 150-100) = **(100, 50)** |

El tanque no se ha movido ni un píxel en el mundo, pero se dibuja 80 píxeles
más a la izquierda. Eso es scroll.

## 2.4 La regla de oro

> **El estado del juego vive en coordenadas de mundo.**
> **La cámara solo decide qué se pinta.**

De esta frase salen, gratis, un montón de cosas que parecen difíciles:

- **Una bala disparada en una sala que no ves llega igual.** No existe "la bala
  de la sala B": existe una bala en la posición (400, 150). Cuando esa posición
  entra en tu ventana, la ves. No hay que programar nada especial.
- **Los dos tanques chocan correctamente aunque estén lejos.** La comprobación
  de cajas usa coordenadas de mundo; si están en salas distintas, sus
  coordenadas distan 300 píxeles y la caja no solapa. Nunca hay que comparar
  "¿están en la misma sala?".
- **En red, cada máquina puede tener su cámara donde quiera** sin que las dos
  partidas se separen. La parte 8 entra en esto a fondo.

Y también sale la regla negativa, que es igual de importante:

> **Si un valor lo calcula la cámara, no puede decidir nada del juego.**

---

# PARTE 3 — PINTAR EL FONDO: LA VENTANA

## 3.1 Por qué ya no vale un memcpy

Antes:

```c
memcpy(buffer_background_image_data, buffer_original_background_bmp, 64000);
```

Ese `memcpy` funciona porque los 64.000 bytes del mapa son, en el mismo orden,
los 64.000 bytes de la pantalla. Fila 0 del mapa, fila 0 de pantalla. Fila 1
del mapa, fila 1 de pantalla. Todo seguido.

Con un mapa de 640 de ancho eso deja de ser cierto, y la razón es que **la
memoria es lineal y la imagen es rectangular**.

## 3.2 Cómo está guardada una imagen en memoria

Una imagen de 640x400 no está guardada como un rectángulo. Está guardada como
una única tira de 256.000 bytes, fila tras fila:

```
  memoria:  [ fila 0 (640 bytes) ][ fila 1 (640 bytes) ][ fila 2 ] ...
```

Para saber en qué byte está el píxel (x, y) se calcula:

```
  posicion = y * ancho_del_mapa + x
```

Ese `ancho_del_mapa` se llama el **stride** o "paso de fila": cuántos bytes hay
que avanzar para bajar una fila. Aquí es 640.

Ahora mira lo que quieres copiar: una ventana de 320 de ancho dentro de un mapa
de 640 de ancho.

```
  MUNDO, 640 de ancho:

  fila 100:  ....................[XXXXXXXXXXXXXXXX]....................
  fila 101:  ....................[XXXXXXXXXXXXXXXX]....................
  fila 102:  ....................[XXXXXXXXXXXXXXXX]....................
                                  ^                ^
                                  camera_x         camera_x + 320

  En memoria, esas tres filas estan asi de separadas:

  [ ...320... XXXX ...320... ][ ...320... XXXX ...320... ][ ... ]
              ^-- lo que quiero      ^-- lo que quiero
                        <-- 640 bytes de distancia -->
```

**Los trozos que quieres no están pegados.** Entre el final de un trozo y el
principio del siguiente hay 320 bytes que no quieres. Un solo `memcpy` no puede
saltárselos.

## 3.3 La solución: una fila cada vez

Si no puedes copiarlo de una vez, lo copias en 200 veces: una por cada fila de
la pantalla.

```c
void bmp_draw_world_window(unsigned char *destination){

	int row;
	unsigned char huge *source;
	unsigned int destination_offset;

	destination_offset = 0;

	for (row = 0; row < HEIGHT; row++){

		source = buffer_original_background_bmp
		       + ((unsigned long)(camera_y + row) * (unsigned long)map_width)
		       + (unsigned long)camera_x;

		memcpy(destination + destination_offset, source, WIDTH);

		destination_offset = destination_offset + WIDTH;

	}

}
```

Léelo despacio, porque es el corazón del sistema:

- `row` va de 0 a 199: las 200 filas de la **pantalla**.
- `camera_y + row` convierte esa fila de pantalla en su fila de **mundo**. Si
  la cámara está en y=100, la fila 0 de pantalla es la fila 100 del mundo.
- `* map_width` salta esa cantidad de filas enteras del mundo (640 bytes cada
  una).
- `+ camera_x` avanza dentro de la fila hasta la columna donde empieza la
  ventana.
- `memcpy(..., WIDTH)` copia 320 bytes: una fila de pantalla completa.
- `destination_offset` avanza 320 en el buffer de pantalla, porque ahí las
  filas sí van seguidas.

**El total de bytes copiados es el mismo que antes**: 200 filas x 320 bytes =
64.000. No es más trabajo, es el mismo trabajo repartido en 200 llamadas en vez
de una. El coste extra es la sobrecarga de llamar 200 veces a `memcpy` en vez
de una, que en un 486 no se nota.

## 3.4 Por qué `source` se recalcula entero cada vuelta

Fíjate en que dentro del bucle la dirección se construye **desde el principio**
cada vez, en vez de hacer `source = source + map_width` al final.

Eso parece un desperdicio. Es deliberado, y tiene que ver con cómo funcionan
los punteros en DOS. La explicación completa está en la parte 9.3, pero el
resumen es: recalcularlo desde la base obliga al compilador a **normalizar** el
puntero, y un puntero normalizado nunca se sale de su segmento cuando le sumas
320. Si lo fueras acumulando, en algún momento daría la vuelta y copiarías
basura.

---

# PARTE 4 — PINTAR LOS SPRITES: EL RECORTE

## 4.1 El problema

El fondo siempre llena la pantalla entera. Un sprite no: puede estar
**a medias**.

```
   PANTALLA
   +--------------------------+
 T |                          |     <- tanque medio fuera por la izquierda
 T |                          |
   |                          |
   |                          |
   |                       T  T     <- y otro medio fuera por la derecha
   +--------------------------+
```

Y con la cámara esto no es un caso raro. Pasa **cada vez** que alguien se
acerca a un borde de la pantalla, o sea, constantemente.

## 4.2 El bug que casi cuelga la máquina

Así estaba la función original:

```c
void draw_sprite_to_buffer(unsigned char *sprite,
                           unsigned int sprite_width,
                           unsigned int sprite_height,
                           unsigned int dest_x,      /* <-- SIN SIGNO */
                           unsigned int dest_y,      /* <-- SIN SIGNO */
                           unsigned char *dest_buffer)
{
    for(y = 0; y < sprite_height; y++) {
        for(x = 0; x < sprite_width; x++) {
            dest_offset = ((dest_y + y) * 320) + (dest_x + x);
            pixel = sprite[src_offset];
            if(pixel != 0) {
                dest_buffer[dest_offset] = pixel;   /* sin comprobar NADA */
            }
        }
    }
}
```

Dos problemas, y el segundo es grave.

**Uno: no comprueba nada.** Escribe donde le digan.

**Dos: `dest_x` es `unsigned int`.** Y a esta función ahora se la llama así:

```c
draw_sprite_to_buffer(..., (int)player1.position_x - camera_x, ...);
```

Si el tanque está en el mundo en x=311 y la cámara en x=320, esa resta da
**-9**. Perfectamente razonable: el tanque está 9 píxeles a la izquierda del
borde de la pantalla.

Pero al meter -9 en un `unsigned int` de 16 bits, no vale -9. Vale **65527**.

Y entonces:

```
  dest_offset = (dest_y + y) * 320 + 65527
```

Eso es un desplazamiento de unos 65.000 bytes fuera del buffer de pantalla. En
DOS no hay protección de memoria: esa escritura **ocurre**, y machaca lo que
haya ahí. Puede ser otro buffer, puede ser el código del propio programa, puede
ser la tabla de vectores de interrupción. El síntoma es que el juego se
comporta raro o se cuelga, minutos después, en un sitio sin relación con el
bug.

## 4.3 El arreglo, en dos mitades

**Mitad uno: coordenadas con signo.**

```c
int dest_x,
int dest_y,
```

Un `int` en Turbo C llega hasta 32.767, de sobra para un mundo de 640, y ahora
-9 vale -9.

Aquí conviene aclarar una duda que sale sola: **¿por qué `int` y no `long`?**
Porque `long` son 32 bits, el 8086 no tiene aritmética de 32 bits, y cada
operación se convierte en varias instrucciones (y las multiplicaciones y
divisiones, en llamadas a rutinas de librería). Pagarías eso a cambio de un
rango de 2.000 millones que no vas a usar nunca. Con `int` tienes 32.767, que
son 102 pantallas de ancho.

**Mitad dos: recortar.**

```c
    /* Del todo fuera de la pantalla: no hay nada que hacer */
    if (dest_x >= WIDTH){ return; }
    if (dest_y >= HEIGHT){ return; }
    if (dest_x + (int)sprite_width <= 0){ return; }
    if (dest_y + (int)sprite_height <= 0){ return; }

    /* Que parte del sprite cae de verdad en la pantalla */
    start_x = 0;
    if (dest_x < 0){ start_x = -dest_x; }

    start_y = 0;
    if (dest_y < 0){ start_y = -dest_y; }

    end_x = (int)sprite_width;
    if (dest_x + end_x > WIDTH){ end_x = WIDTH - dest_x; }

    end_y = (int)sprite_height;
    if (dest_y + end_y > HEIGHT){ end_y = HEIGHT - dest_y; }

    for(y = start_y; y < end_y; y++) {
        for(x = start_x; x < end_x; x++) {
            ...
        }
    }
```

Con un ejemplo: sprite de 18x18 en `dest_x = -9`.

- No está del todo fuera (`-9 + 18 = 9 > 0`), así que seguimos.
- `start_x = 9`. Las columnas 0 a 8 del sprite caen fuera y no se recorren.
- `end_x = 18`. Por la derecha no se sale.
- Se pintan las columnas 9 a 17, o sea 9 columnas x 18 filas = 162 píxeles.

## 4.4 Por qué se calculan los límites antes y no dentro

Se podría haber escrito así, que es más corto:

```c
for(y = 0; y < sprite_height; y++) {
    if (dest_y + y < 0 || dest_y + y >= HEIGHT) continue;
    for(x = 0; x < sprite_width; x++) {
        if (dest_x + x < 0 || dest_x + x >= WIDTH) continue;
        ...
    }
}
```

Funciona igual, pero hace **dos comparaciones por píxel**, siempre, incluso
cuando el sprite está entero dentro. Un tanque son 324 píxeles, y hay dos
tanques y dos balas por frame.

Calculando los límites una vez antes de los bucles, el caso normal (sprite
entero dentro) no paga absolutamente nada, y el caso recortado tampoco. En un
486 esto importa.

## 4.5 Qué se probó

Esta función es la que puede colgar la máquina, así que se probó a lo bruto:
se colocó un sprite de 18x18 en **todas** las posiciones desde (-40, -40) hasta
(360, 240) --- 112.681 posiciones --- con **bytes centinela** (el valor 0xAA)
rodeando el buffer de pantalla por los dos lados.

Resultado: **ni un solo byte escrito fuera del buffer**. Y los conteos exactos:

| Caso | Píxeles pintados |
|---|---|
| Sprite entero dentro | 324 (18x18) |
| Medio fuera por la izquierda (-9) | 162 (9x18) |
| Medio fuera por la derecha | 162 (9x18) |
| Esquina superior izquierda (-9,-9) | 81 (9x9) |
| Del todo fuera | 0 |

---

# PARTE 5 — MOVER LA CÁMARA: LA ZONA MUERTA

Ya sabemos pintar la ventana esté donde esté. Falta decidir **dónde ponerla**
cada frame.

## 5.1 La idea, en una frase

> Mientras el tanque se mueva dentro de un rectángulo en el centro de la
> pantalla, la cámara no se mueve. Cuando se sale, la cámara le empuja
> exactamente lo que se ha salido.

```
   PANTALLA 320x200

   +----------------------------------+
   |                                  |
   |     +----------------------+     |  <- 70 px de margen arriba
   |     |                      |     |
   |     |     ZONA MUERTA      |     |
   |     |   la camara no se    |     |
   |     |      mueve aqui      |     |
   |     |                      |     |
   |     +----------------------+     |  <- 70 px de margen abajo
   |                                  |
   +----------------------------------+
      ^                            ^
      100 px                    100 px
```

## 5.2 Los dos números que la definen

```c
#define CAMERA_DEAD_ZONE_X 	100
#define CAMERA_DEAD_ZONE_Y 	 70
```

Son los márgenes desde el borde de la pantalla hasta el borde de la zona
muerta. Con esos valores y un tanque de 18x18, la zona muerta va:

- En horizontal, de x=100 a x = 320 - 100 - 18 = **202**. Un carril de 102 px.
- En vertical, de y=70 a y = 200 - 70 - 18 = **112**. Un carril de 42 px.

**Hay un límite que no puedes pasar** al elegir estos números: el margen tiene
que ser menor que la mitad de la pantalla menos el tanque. Si no, el borde
izquierdo de la zona muerta quedaría a la derecha del derecho, los dos
"empujes" se dispararían a la vez y la cámara se pelearía consigo misma
temblando en el sitio.

| | Límite | Valor usado |
|---|---|---|
| `CAMERA_DEAD_ZONE_X` | menos de (320-18)/2 = 151 | 100 |
| `CAMERA_DEAD_ZONE_Y` | menos de (200-18)/2 = 91 | 70 |

## 5.3 El código

```c
void bmp_camera_follow(int target_x, int target_y, int target_width, int target_height){

	int screen_x;
	int screen_y;
	int right_edge;
	int bottom_edge;

	/* Donde esta el objetivo DENTRO de la ventana ahora mismo */
	screen_x = target_x - camera_x;
	screen_y = target_y - camera_y;

	right_edge  = WIDTH  - CAMERA_DEAD_ZONE_X - target_width;
	bottom_edge = HEIGHT - CAMERA_DEAD_ZONE_Y - target_height;

	if (screen_x < CAMERA_DEAD_ZONE_X){
		camera_x = camera_x - (CAMERA_DEAD_ZONE_X - screen_x);
	}else if (screen_x > right_edge){
		camera_x = camera_x + (screen_x - right_edge);
	}

	if (screen_y < CAMERA_DEAD_ZONE_Y){
		camera_y = camera_y - (CAMERA_DEAD_ZONE_Y - screen_y);
	}else if (screen_y > bottom_edge){
		camera_y = camera_y + (screen_y - bottom_edge);
	}

	bmp_camera_clamp();

}
```

Nota que **recibe enteros, no un `struct player`**. Es a propósito: así `bmp.c`
sigue sin saber qué es un tanque, y mañana la cámara puede seguir a lo que te
dé la gana en otro proyecto. Es la misma filosofía que se aplicó a `sound.c` y
a `net.c`.

### Por qué se resta `target_width`

```c
	right_edge = WIDTH - CAMERA_DEAD_ZONE_X - target_width;
```

Porque **`position_x` es la esquina superior IZQUIERDA** del sprite, no su
centro. El borde derecho del tanque está 18 píxeles más allá de su posición.

Sin restarlo, el margen derecho se mediría contra la esquina izquierda y el
tanque se metería 18 píxeles de más en la zona de empuje: la zona muerta
quedaría **descentrada** hacia la derecha, y se notaría al ir en esa dirección.

### Por qué `else if` y no dos `if` sueltos

Con la zona muerta bien dimensionada (5.2) los dos casos son excluyentes: no se
puede estar a la vez a la izquierda del borde izquierdo y a la derecha del
derecho.

Pero **si alguien pone un margen por encima del límite**, los dos serían ciertos
a la vez. Con dos `if` sueltos se aplicarían las dos correcciones seguidas, cada
frame, y la cámara se dispararía. Con `else if`, el daño se queda en un temblor.

Es una defensa barata contra un valor mal elegido en un `#define`.

## 5.4 Por qué "exactamente lo que se ha salido"

Esta es la parte bonita del modelo, y es fácil pasarla por alto.

La corrección es `screen_x - right_edge`, es decir: **cuántos píxeles se ha
salido**. Ni más ni menos.

Sigamos un tanque que anda a 2 píxeles por frame hacia la derecha:

| Frame | Tanque (mundo) | Cámara | Tanque en pantalla | Qué pasa |
|---|---|---|---|---|
| 1 | 250 | 0 | 250 | 250 > 202: se sale 48. Cámara +48 |
| | | 48 | 202 | queda justo en el borde |
| 2 | 252 | 48 | 204 | se sale 2. Cámara +2 |
| | | 50 | 202 | otra vez en el borde |
| 3 | 254 | 50 | 204 | se sale 2. Cámara +2 |
| | | 52 | 202 | |

**El tanque anda 2, la cámara anda 2.** No hay salto y no hay retraso: el
tanque se queda pegado al borde de la zona muerta y el mundo se desliza detrás
de él a la misma velocidad exacta.

Y en el momento en que sueltas la tecla, el tanque deja de moverse, deja de
estar fuera de la zona muerta, y la cámara **para en seco** con él. Sin inercia
ni frenada.

Compara eso con un modelo de "cámara que persigue con suavizado" del tipo
`camera_x += (objetivo - camera_x) / 8`. Eso siempre va con retraso, siempre
sigue moviéndose un rato después de que pares, y en un juego donde apuntas con
el morro del tanque, molesta.

## 5.5 No salirse del mapa: el `clamp`

Si el tanque va hacia la esquina superior izquierda, la fórmula de arriba
querría poner `camera_x` en negativo. Y entonces la ventana leería memoria de
antes del principio del mapa.

```c
static void bmp_camera_clamp(){

	int limit_x;
	int limit_y;

	limit_x = map_width  - WIDTH;
	limit_y = map_height - HEIGHT;

	if (limit_x < 0){ limit_x = 0; }
	if (limit_y < 0){ limit_y = 0; }

	if (camera_x < 0){ camera_x = 0; }
	if (camera_y < 0){ camera_y = 0; }
	if (camera_x > limit_x){ camera_x = limit_x; }
	if (camera_y > limit_y){ camera_y = limit_y; }

}
```

`map_width - WIDTH` es la posición más a la derecha que puede tener la ventana
sin que su borde derecho se salga del mapa. Para 640: `640 - 320 = 320`.

Así que `camera_x` solo puede valer de 0 a 320, y `camera_y` de 0 a 200.

Cuando la cámara está topada, el tanque **sí** se sale de la zona muerta y se
acerca al borde de la pantalla. Es lo correcto: no hay más mapa que enseñar.

## 5.6 El `snap`: empezar una ronda

Cuando empieza una ronda, los tanques se teletransportan a sus esquinas. No
hay nada desde lo que "seguir suavemente": la cámara tiene que aparecer ya
puesta.

```c
void bmp_camera_snap(int target_x, int target_y, int target_width, int target_height){

	camera_x = target_x + (target_width  / 2) - (WIDTH  / 2);
	camera_y = target_y + (target_height / 2) - (HEIGHT / 2);

	bmp_camera_clamp();

}
```

Centra el objetivo: coge su centro y le resta media pantalla. Y aplica el mismo
`clamp`, así que si el tanque está pegado a una esquina, la cámara se queda en
el borde en vez de mostrarte el vacío de fuera del mapa.

## 5.7 A quién sigue la cámara

En `main.c`:

```c
void update_camera(int snap_to_target){

	struct player *target;

	target = &player1;

	if (network_mode == 1){
		if (local_player_is_1 == 0){
			target = &player2;
		}
	}

	if (snap_to_target == 1){
		bmp_camera_snap((int)target->position_x, (int)target->position_y, TANK_WIDTH, TANK_HEIGHT);
	}else{
		bmp_camera_follow((int)target->position_x, (int)target->position_y, TANK_WIDTH, TANK_HEIGHT);
	}

}
```

**Cada máquina sigue a su propio tanque.** Esa línea es la que hace que el modo
supernet tenga sentido: los dos jugadores ven trozos distintos del mapa y
tienen que buscarse.

Y por eso el mapa grande **solo existe en modo red**. Una cámara solo puede
seguir a un tanque; en un solo teclado con dos jugadores, uno de los dos se
quedaría conduciendo a ciegas. Por eso `/bigmap` sin `/net` se rechaza:

```c
	if (network_mode == 0){
		if (big_map_mode == 1){
			printf("\n/bigmap needs /net: it is the big map that has to be\n");
			...
		}
		big_map_mode = 0;
	}
```

## 5.8 Dónde encaja en el frame

En `draw_to_buffer()`, y el orden importa:

```c
void draw_to_buffer(){

	/* 1. Decidir donde esta la ventana ESTE frame */
	update_camera(0);

	/* 2. Pintar el trozo de mapa que toca */
	bmp_draw_world_window(buffer_background_image_data);

	/* 3. Pintar encima todo lo demas, restando la camara */
	draw_sprite_to_buffer(sprite_player1, TANK_WIDTH, TANK_HEIGHT,
	                      (int)player1.position_x - camera_x,
	                      (int)player1.position_y - camera_y,
	                      buffer_background_image_data);
	...
}
```

La cámara se decide **antes** de pintar nada, para que el fondo y todo lo que
va encima estén de acuerdo sobre el mismo frame. Si movieras la cámara entre
pintar el fondo y pintar los tanques, los tanques saldrían desplazados
respecto al suelo, flotando sobre él.

### La resta va en TODOS los objetos, y olvidarla tiene un síntoma muy claro

En el juego hay **cinco** llamadas a `draw_sprite_to_buffer()`: los dos tanques,
las dos balas y la explosión. **Las cinco** llevan la resta.

Si se te olvida en una sola:

> Ese objeto se queda **pegado a la pantalla** mientras todo lo demás se
> desliza.

Una bala que te sigue a todas partes en vez de quedarse donde la disparaste. Una
explosión que viaja contigo por el mapa. Es un fallo muy visual, y en cuanto lo
has visto una vez lo diagnosticas en dos segundos.

El error simétrico —**restarla dos veces**— hace que el objeto se mueva al doble
de velocidad y en sentido contrario al esperado.

---

# PARTE 6 — POR QUÉ EL JUEGO NORMAL USA EL MISMO CÓDIGO

Aquí hay una decisión de diseño que merece la pena entender, porque es la que
evita tener dos juegos que mantener.

Podrías haber escrito:

```c
if (big_map_mode == 1){
    /* codigo de camara */
}else{
    /* codigo de antes */
}
```

**No hay nada de eso.** El modo normal pasa por exactamente las mismas
funciones. Y funciona porque los números se encargan solos:

Con un mapa de 320x200:

- `limit_x = map_width - WIDTH = 320 - 320 = 0`
- `limit_y = map_height - HEIGHT = 200 - 200 = 0`

El `clamp` deja a `camera_x` y `camera_y` **clavados en 0 para siempre**. Da
igual lo que calcule `bmp_camera_follow()`: el clamp lo devuelve a 0.

Y entonces:

- `bmp_draw_world_window()` copia 200 filas de 320 bytes empezando en (0,0), que
  es exactamente el mismo `memcpy` de 64.000 bytes de toda la vida, partido en
  trozos.
- `(int)position_x - camera_x` es `position_x - 0`, o sea `position_x`.
- El recorte no recorta nada, porque nada se sale.

> **Un mundo de una pantalla no es un caso especial: es el caso general con la
> cámara topada en cero.**

Esto está probado: hay un test que llama a `bmp_camera_follow()` y a
`bmp_camera_snap()` con un mapa de 320x200 y valores absurdos, y comprueba que
la cámara sigue en (0,0).

---

# PARTE 7 — LAS COLISIONES

## 7.1 Nunca leas el dibujo para decidir

Antes de la cámara, esto ya era así, pero ahora es todavía más importante.

El juego tiene **dos** imágenes del mapa:

| Buffer | Qué es | Para qué |
|---|---|---|
| `buffer_original_background_bmp` | El dibujo bonito: ladrillos, arbustos | Solo para pintar |
| La máscara de colisión | Muro sí / muro no | Solo para decidir |

¿Por qué no usar el dibujo para las colisiones y ahorrarse un fichero?

**Porque el dibujo miente.** Un muro de ladrillo tiene líneas oscuras de mortero
entre ladrillo y ladrillo. Si preguntas "¿de qué color es este píxel?" en el
sitio equivocado, te contesta "negro", y el juego decide que ahí no hay pared.
En este mapa concreto hay **22.806 píxeles** que son negros en el dibujo y sí
son muro.

Y al revés: los arbustos verdes se dibujan pero se pueden pisar. **13.047
píxeles** que parecen sólidos y no lo son.

Por eso hay un `bigcol.bmp` aparte, con dos colores: azul (suelo) y amarillo
(muro, el índice de paleta 252).

Hay una tercera fuente que **nunca** debe usarse para decidir: la memoria de
vídeo. Para cuando lees de ahí, los tanques ya están pintados encima, así que
preguntar por el píxel de la punta del cañón te devuelve el color del propio
tanque.

## 7.2 Por qué la máscara tiene que cubrir el mundo entero

Esta es sutil, y es donde la cámara y la red se cruzan.

En una partida en red, **las dos máquinas simulan el juego completo**: los dos
tanques y las dos balas. Eso es lo que es el lockstep.

Así que tu máquina, que está mostrando la sala de arriba a la izquierda, tiene
que poder contestar: *"el tanque del otro, que está en la sala de abajo a la
derecha, ¿ha chocado con una pared?"*

Si la máscara solo tuviera la parte visible, tu máquina no tendría esos datos.
Contestaría distinto que la otra, y las dos partidas se separarían.

> **La máscara de colisión cubre el mundo entero, siempre, se vea o no.**

## 7.3 Un bit por píxel

Un mundo de 640x400 son 256.000 píxeles. A un byte por píxel serían 256.000
bytes, y no caben (la parte 9 explica por qué).

Pero fíjate en lo que se le pregunta a la máscara:

```c
if (bmp_is_wall(x, y) == 1){ ... }
```

Solo hay **dos respuestas posibles**. De los 256 valores que caben en un byte,
te importa uno. Estás gastando 8 bits para guardar un sí/no.

Un sí/no cabe en 1 bit, y en un byte caben 8 píxeles:

```
  Un byte por pixel (8 pixeles = 8 bytes):

    pixel:   0     1     2     3     4     5     6     7
    bytes: [00]  [00]  [FF]  [FF]  [00]  [00]  [00]  [FF]

  Un bit por pixel (8 pixeles = 1 byte):

    pixel:   0  1  2  3  4  5  6  7
    bits:    0  0  1  1  0  0  0  1
    byte:  [ 00110001 ]
```

Ocho veces menos memoria para la misma información exacta:

| | Píxeles | Bytes |
|---|---|---|
| Mundo 640x400, 1 byte/px | 256.000 | 256.000 |
| Mundo 640x400, **1 bit/px** | 256.000 | **32.000** |

32.000 bytes caben en un `malloc()` normal. Y es **la mitad** de lo que
costaba el mapa de colisión de una sola pantalla antes (64.000). O sea que el
mapa de colisión del mundo entero ocupa menos que el de una pantalla.

## 7.4 Cómo se lee un bit

```c
int bmp_is_wall(int x, int y){

	unsigned long bit_index;
	unsigned int  byte_index;
	unsigned char bit;

	if (x < 0){ return 1; }
	if (y < 0){ return 1; }
	if (x >= map_width){ return 1; }
	if (y >= map_height){ return 1; }

	if (buffer_collision_mask == NULL){ return 0; }

	bit_index  = ((unsigned long)y * (unsigned long)map_width) + (unsigned long)x;
	byte_index = (unsigned int)(bit_index >> 3);
	bit        = (unsigned char)(1 << (unsigned int)(bit_index & 7L));

	if ((buffer_collision_mask[byte_index] & bit) != 0){
		return 1;
	}

	return 0;

}
```

Paso a paso:

1. **`bit_index`** es el número de píxel contando desde el principio del mundo,
   igual que antes: `y * ancho + x`. Va de 0 a 255.999, así que necesita 32
   bits (`unsigned long`).
2. **`>> 3`** es dividir entre 8: en qué byte está ese bit. Un desplazamiento,
   no una división de verdad.
3. **`& 7`** es el resto de dividir entre 8: qué bit dentro de ese byte. Un AND,
   tampoco una división.
4. **`1 << bit`** construye una máscara con un solo bit encendido.
5. **`&`** comprueba si ese bit está puesto.

El coste extra frente a leer un byte suelto es un desplazamiento y dos ANDs.
Se llama 3 veces por tanque y por frame más una por bala. Es despreciable.

Un detalle de rendimiento que sí importa: `(unsigned long)y * map_width` parece
una multiplicación de 32 bits, que sería cara. Pero el 8086 tiene una
instrucción `MUL` que multiplica dos números de 16 bits y da un resultado de 32
bits **en una sola instrucción**. El compilador la usa. Sale barato.

## 7.5 Fuera del mapa cuenta como muro

Mira las primeras cuatro comprobaciones. Cualquier coordenada fuera del mundo
devuelve 1, o sea "hay pared".

Eso no es una regla del juego, es una **red de seguridad**. Los mapas están
dibujados con un borde macizo de 16 a 33 píxeles, así que nunca debería
llegarse ahí. Pero si algún día dibujas un mapa con un agujero en el borde, el
tanque se para en seco en vez de que el juego lea memoria que no es suya y se
cuelgue veinte segundos después por una razón incomprensible.

## 7.6 Cómo se construye la máscara

El fichero que dibujas en el Paint es un BMP normal de 640x400 con dos colores.
La máscara se empaqueta al arrancar, leyendo el fichero **una fila cada vez**:

```c
	for (row = map_height - 1; row >= 0; row = row - 1){

		fread(map_line, 1, map_width, file);

		for (column = 0; column < map_width; column++){

			if (map_line[column] == MAP_WALL_COLOR){

				bit_index  = ((unsigned long)row * (unsigned long)map_width) + (unsigned long)column;
				byte_index = (unsigned int)(bit_index >> 3);

				buffer_collision_mask[byte_index] =
					buffer_collision_mask[byte_index] | (unsigned char)(1 << (unsigned int)(bit_index & 7L));

			}

		}

	}
```

Dos cosas que explicar:

**El bucle cuenta hacia atrás.** Los BMP guardan sus filas **de abajo arriba**:
la primera fila del fichero es la última de la imagen. En vez de leerlo todo y
darle la vuelta después, se lee hacia delante y se escribe hacia atrás. Es el
mismo trabajo sin necesitar un buffer temporal.

**Nunca existen los 256.000 bytes.** Se lee una fila de 640 bytes, se empaquetan
sus bits, y esa fila se descarta. Solo llegan a existir los 32.000 bytes
empaquetados. Eso importa mucho en una máquina de 640 KB.

## 7.7 Un aviso sobre dibujar mapas

Hay una regla al dibujar el mapa de colisión que el código no puede hacer
cumplir y que te va a morder si la incumples:

> **El muro del borde tiene que tener al menos 8 píxeles de grosor.**

Porque el tanque avanza de `PIXEL_TO_MOVE` = 2 en 2 píxeles. Las posiciones que
puede ocupar son start, start+2, start+4... **nunca las intermedias**. Un muro
de 1 píxel de grosor puede caer justo en una coordenada que el tanque nunca
pisa, y **lo atraviesa sin enterarse**.

Y con las balas es peor: avanzan de 3 en 3, y se comprueban en **un solo
punto**, no en tres como el tanque.

Los mapas de este juego tienen de 16 a 33 píxeles de borde. De sobra.

Lo mismo para los pasos entre salas: un hueco tiene que ser **más ancho que el
tanque**, no igual. Durante el desarrollo hubo una puerta de exactamente 18
píxeles con un tanque de exactamente 18, y era imposible pasar: habría hecho
falta que la coordenada del tanque cayera en el único valor exacto que encaja.
Deja 25 o 30.

---

# PARTE 8 — LA RED: LA CÁMARA NO ES PARTE DEL JUEGO

## 8.1 Recordatorio de cómo funciona el lockstep

En dos frases, porque el manual de red lo cuenta entero: no se envían
posiciones. Se envía **qué teclas ha pulsado cada jugador**, y las dos máquinas
simulan la partida completa desde el mismo estado inicial. Si las dos hacen
exactamente las mismas cuentas, llegan al mismo resultado.

La palabra clave es **exactamente**. Si una máquina calcula un solo número
distinto que la otra, las dos partidas se separan y ya no se recuperan. A eso
se le llama desincronización.

## 8.2 Y ahora las dos cámaras están en sitios distintos

En supernet, la máquina A sigue al tanque 1 y la máquina B sigue al tanque 2.
Sus `camera_x` valen cosas distintas. **A propósito.**

¿No rompe eso el determinismo?

**No, y la razón es la regla de oro de la parte 2.** La cámara no decide nada
del juego. Solo decide qué se pinta. Las dos máquinas hacen las mismas cuentas
sobre las mismas coordenadas de mundo, obtienen el mismo resultado, y luego
cada una pinta un trozo distinto de ese resultado idéntico.

Es como dos personas mirando el mismo tablero de ajedrez desde lados opuestos.
Ven cosas distintas. La partida es la misma.

## 8.3 La regla, en negativo

> **Si un valor lo calcula `bmp_camera_follow()`, no puede entrar en el checksum
> ni influir en ninguna decisión del juego.**

El checksum es la comprobación que cada máquina hace cada 30 frames: resume su
estado en un número y lo compara con el de la otra. Si difieren, hay
desincronización.

```c
unsigned int compute_state_checksum(){

	unsigned int checksum;

	checksum = 0;

	checksum = checksum + (player1.position_x * 3);
	checksum = checksum + (player1.position_y * 5);
	...
	checksum = checksum + (explosion_pause_counter * 71);

	checksum = checksum + ((unsigned int)map_width * 73);
	checksum = checksum + ((unsigned int)map_height * 79);

	return checksum;

}
```

Posiciones sí. Direcciones sí. Balas sí. **`camera_x` y `camera_y` no
aparecen**, y no pueden aparecer: si los metieras, las dos máquinas darían
números distintos en el frame 1 de toda partida y el juego reportaría
desincronización siempre.

## 8.4 El cable trampa: `map_width` en el checksum

Fíjate en las dos últimas líneas. `map_width` no es estado del juego: no cambia
nunca durante una partida. ¿Qué hace ahí?

Es una **trampa deliberada**. Si arrancas una máquina con `/bigmap` y la otra
sin él, los dos mundos tienen tamaños distintos, los muros están en sitios
distintos, y las dos simulaciones se separan de una forma muy difícil de leer
desde fuera: los tanques hacen cosas que no tienen sentido y no sabes por qué.

Metiendo `map_width` en el checksum, esa situación se convierte en un **informe
de desincronización limpio en el log** en el primer chequeo. El juego te dice
que las dos máquinas no están jugando al mismo juego, en vez de dejarte
mirando dos pantallas que divergen.

Es barato: dos sumas cada 30 frames.

## 8.5 La bala que llega de otra sala

Este es el caso que parece que necesita código especial y no lo necesita.

Situación: el jugador 1 está en la sala de arriba a la izquierda. El jugador 2,
en la de arriba a la derecha, dispara hacia la izquierda. La bala vuela hacia
el jugador 1, cruzando una frontera que ninguno de los dos ve.

**¿Qué hay que programar para esto?** Nada.

- La máquina del jugador 1 **ya está simulando esa bala**. La ha calculado ella,
  frame a frame, desde que se disparó, porque simula la partida entera.
- La bala tiene una posición de mundo, por ejemplo (400, 150).
- Al pintar, se pregunta lo mismo que para todo: `400 - camera_x`. Si eso cae
  entre 0 y 319, se pinta. Si no, no.
- Cuando la bala llega a x=390 y tu cámara está en 80, sale en pantalla en la
  x=310: **entra por el borde derecho**, a la vista.

No existe "la bala de la sala B". Existe una bala en una posición. La cámara
decide si la ves.

Y la colisión de esa bala contra los muros de la sala que no ves también
funciona, porque la máscara cubre el mundo entero (parte 7.2).

Esto se comprobó de verdad: en la prueba de la cámara se dispara una bala desde
x=540 hacia la izquierda mientras el tanque está en la parte de abajo, y en los
fotogramas renderizados se la ve entrar por el borde derecho de la ventana y
cruzar la pantalla.

## 8.6 Lo que sí es local en cada máquina

Para dejarlo claro, un resumen de qué lado está cada cosa:

| Dato | ¿Idéntico en las dos máquinas? |
|---|---|
| `player1.position_x` / `position_y` | **Sí**, obligatorio |
| `bullet_position_x` / `bullet_is_flying` | **Sí**, obligatorio |
| La máscara de colisión | **Sí**, obligatorio |
| `map_width` / `map_height` | **Sí**, y el checksum lo vigila |
| `camera_x` / `camera_y` | **No**, y es correcto que no |
| `local_player_is_1` | **No**: es justo lo contrario en cada una |
| Lo que hay en `buffer_background_image_data` | **No**: cada una pinta su trozo |

---

# PARTE 9 — LA MEMORIA: 640 KB Y UNA TARDE PERDIDA

Esta parte no va de cámaras. Va de por qué en DOS una cosa que "cabe" puede no
caber, y es donde se perdió más tiempo del proyecto. Si vas a tocar código de
DOS alguna vez, esto vale más que el resto del manual.

## 9.1 El presupuesto

DOS en modo real tiene 640 KB de memoria convencional, y de ahí sale todo: el
propio DOS, los drivers, los TSR de IPX, el código de tu programa, su pila, y
todo lo que reserves.

Lo que este juego mide al arrancar, en la máquina de pruebas:

```
Memory at start: near 571344  far 571344
```

**571.344 bytes.** Ese es el presupuesto entero.

Y lo que hay que meter dentro, en modo supernet:

| Qué | Bytes |
|---|---:|
| El mapa 640x400 | 256.000 |
| Buffer de pantalla | 64.000 |
| Máscara de colisión | 32.000 |
| Paleta | 309 |
| Buffer DMA del sonido | 8.192 |
| Los 4 WAV | 180.731 |
| Sprites de los tanques | ~6.300 |
| **Total** | **547.532** |

Sobran 23.812 bytes. Eso es lo justo que va.

## 9.2 `malloc` no puede pedir más de 65.535 bytes

Primera sorpresa de Turbo C: `malloc()` recibe un `size_t`, que es un entero de
**16 bits**. El número más grande que cabe ahí es 65.535.

```c
malloc(64000);    /* bien: 64000 < 65535 */
malloc(256000);   /* imposible: no se puede ni pedir */
```

Para bloques más grandes hay `farmalloc()`, que recibe un `unsigned long`:

```c
buffer_original_background_bmp = (unsigned char huge *)farmalloc(world_size);
```

Por eso el mapa se pide con `farmalloc` y todo lo demás con `malloc`: el mapa es
lo único que pasa de 64 KB.

**Un aviso, porque yo me equivoqué en esto durante este proyecto:** durante un
rato di por hecho que `malloc` y `farmalloc` usaban depósitos de memoria
distintos, y construí un razonamiento entero encima. El log lo desmintió:

```
Memory at start: near 571344  far 571344
```

`coreleft()` y `farcoreleft()` devuelven **el mismo número**. En el modelo huge
son el mismo depósito. Si te encuentras razonando sobre cómo se comporta el
sistema, mide antes de construir sobre ello.

## 9.3 Punteros `far` y `huge`

Segunda sorpresa. En el 8086 una dirección se forma con dos números de 16 bits:

```
   direccion fisica = segmento * 16 + desplazamiento
```

Un puntero `far` guarda los dos. El problema es que la aritmética de punteros
`far` **solo toca el desplazamiento**. Y el desplazamiento son 16 bits, así que
cuando pasa de 65.535 **da la vuelta a cero** en vez de llevarse una a la parte
del segmento.

En un buffer de 64.000 bytes eso da igual, nunca llegas. En uno de 256.000, das
la vuelta cuatro veces y lees basura.

La solución es el modificador `huge`:

```c
extern unsigned char huge *buffer_original_background_bmp;
```

Un puntero `huge` se **normaliza** en cada operación: el compilador reajusta
segmento y desplazamiento para que el desplazamiento quede siempre entre 0 y
15. Así nunca hay vuelta, y puedes recorrer bloques de cualquier tamaño.

Cuesta unas instrucciones más por operación, así que se usa solo donde hace
falta.

**Y ahora se entiende el detalle del bucle de la parte 3.4:**

```c
	for (row = 0; row < HEIGHT; row++){

		source = buffer_original_background_bmp
		       + ((unsigned long)(camera_y + row) * (unsigned long)map_width)
		       + (unsigned long)camera_x;

		memcpy(destination + destination_offset, source, WIDTH);
	}
```

`source` se reconstruye desde la base en cada vuelta. Al hacerlo, Turbo C lo
normaliza: el desplazamiento queda entre 0 y 15. Entonces el `memcpy` de 320
bytes que viene después llega como mucho al desplazamiento 335, muy lejos de
65.535, y **es imposible que dé la vuelta a mitad de la copia**.

Si en lugar de eso hicieras `source = source + map_width` al final del bucle,
el desplazamiento iría creciendo y en algún momento el `memcpy` cruzaría la
frontera del segmento y copiaría de otro sitio.

## 9.4 La historia de `died.wav`

Este es el bug del proyecto, y es un caso de estudio perfecto.

**El síntoma:** todo el juego funcionaba en modo supernet menos el sonido de la
muerte. En el log:

```
Sound: could not load died.wav
```

**El primer razonamiento (equivocado):** no cabe. Hagamos sitio.

Se recuperaron 61.440 bytes arreglando el buffer del DMA del sonido, que pedía
69.632 bytes para usar 4.096 (pedía alinearse a una frontera de 64 KB cuando lo
que hace falta es *no cruzarla*, que se consigue pidiendo el doble del tamaño).

No bastó. Y la aritmética decía que **debería sobrar**: al llegar a `died.wav`
quedaban **129.982 bytes libres** y el fichero pide **42.090**.

**Lo que pasaba de verdad:** no faltaba memoria. La memoria libre estaba en el
sitio equivocado.

El orden de arranque era así:

```
  1. Se pide el MAPA:              farmalloc(256000)
  2. Se piden los buffers de pantalla, incluida la HOJA DE SPRITES (64000)
  3. Se recortan los sprites
  4. Se LIBERA la hoja de sprites:  free(64000)      <- deja un HUECO
  5. Se cargan los WAV:             farmalloc(42090) <- no lo encuentra
```

Dibujado, el montón de memoria quedaba así:

```
   +----------------------------------------------------------+
   |  MAPA 256000  | hueco 64000 | pantalla | mascara | libre  |
   +----------------------------------------------------------+
                    ^^^^^^^^^^^^
                    libre, pero enterrado en medio
```

Los 129.982 bytes libres eran: 64.000 en ese hueco, y el resto arriba del todo.
Pero **el hueco y el espacio de arriba no están pegados**. Y `farcoreleft()`,
que es lo que mide, solo cuenta lo que hay **por encima de la reserva más
alta**: el hueco de abajo ni lo ve.

> **Memoria libre total no es lo mismo que memoria libre contigua.**
> A esto se le llama **fragmentación**, y en DOS te muerde constantemente.

**El segundo intento (que rompió el juego):** cargar el sonido *antes* que el
mapa. El sonido cargó perfectamente... y entonces **el mapa no cupo**. Quedaban
247.371 bytes contiguos y el mapa pedía 256.000. Faltaban 8.629.

Como `farmalloc` devolvió NULL y el código seguía adelante con un puntero nulo,
el resultado en pantalla fue el fondo desaparecido y los tanques dibujados
sobre basura. El log lo dijo con toda claridad, pero había que saber leerlo:

```
Sound: loaded, 375680 bytes left        <- los cuatro WAV cargaron
Map 640x400  memory now: near 246800    <- pero init_graphics solo gasto 128880
```

`init_graphics` solo consumió 128.880 bytes cuando el mapa solo ya son 256.000.
La resta no engaña: el mapa nunca se reservó.

**El arreglo bueno:** en vez de mover cosas de sitio, **quitar el hueco**.

La hoja de sprites era un buffer de 64.000 bytes que contenía `sprites.bmp`
entero, del que se recortaban los 24 sprites al arrancar y que después no se
volvía a leer nunca. Se eliminó del todo: ahora `sprites.bmp` se abre y cada
sprite se lee **directamente del fichero**, una fila cada vez.

```c
void bmp_extract_sprite(unsigned int src_x, unsigned int src_y,
                        unsigned int sprite_width, unsigned int sprite_height,
                        unsigned char *sprite_dest)
{
	unsigned int y;
	long file_offset;

	for (y = 0; y < sprite_height; y++){

		file_offset = 1078L
		            + ((long)(HEIGHT - 1 - (src_y + y)) * (long)WIDTH)
		            + (long)src_x;

		fseek(file_sprites_game_open, file_offset, SEEK_SET);
		fread(sprite_dest + (y * sprite_width), 1, sprite_width, file_sprites_game_open);

	}
}
```

(El `HEIGHT - 1 - (src_y + y)` es otra vez el bottom-up del BMP, hecho como una
resta en vez de dando la vuelta a un buffer entero.)

Resultado:

- Se ahorran 64.000 bytes, que es más de lo que le faltaba a `died.wav`.
- **No se libera nada nunca**, así que no hay hueco y no hay fragmentación.
- El orden de arranque se queda como estaba, sin tocar.
- Cuesta unos 340 `fseek` al arrancar y nada más nunca.

Y el reparto final:

```
  mapa      256.000  ->  quedan 315.344
  pantalla   64.000  ->  quedan 251.344
  mascara    32.000  ->  quedan 219.035
  DMA         8.192  ->  quedan 210.843
  fire       25.699  ->  quedan 185.144
  engip1     55.472  ->  quedan 129.672
  engip2     57.470  ->  quedan  72.202
  died       42.090  ->  quedan  30.112   <- entra
```

## 9.5 Las lecciones

1. **Mide, no supongas.** Dos líneas de `coreleft()` y `farcoreleft()` en el log
   valieron más que todo mi razonamiento sobre cómo debería comportarse el
   asignador.
2. **Total libre no es contiguo libre.** Esta es la lección de verdad.
3. **La reserva más grande, la primera**, sobre un montón limpio. Y si puedes,
   no liberes nada durante la ejecución: en DOS un `free` a mitad de partida es
   un agujero que se queda ahí.
4. **Un fallo silencioso cuesta más que un fallo ruidoso.** `farmalloc` devolvió
   NULL, el código hizo un `printf` sobre una pantalla que estaba en modo
   gráfico (o sea, invisible) y siguió adelante. Por eso el síntoma fue "se ha
   roto todo" en vez de "el mapa no cupo".

## 9.6 Cómo medirlo tú

```c
sprintf(log_message_text, "Memory at start: near %lu  far %lu",
        (unsigned long)coreleft(), (unsigned long)farcoreleft());
tanks_log(log_message_text);
```

Al principio de `main()` y otra vez después de reservar todo. Dos líneas que
convierten una tarde de conjeturas en una resta.

---

# PARTE 10 — CÓMO LLEVARTE ESTO A OTRO PROYECTO

Supón que tienes otro juego en DOS con pantalla fija y quieres ponerle un mapa
grande. Estos son los pasos, en orden, y ninguno depende del anterior más de lo
imprescindible.

## Paso 1 — El recorte de sprites, primero de todo

Antes de tocar nada de cámaras, arregla tu función de dibujar sprites:
coordenadas **con signo** y recorte.

Hazlo el primero porque **no cambia absolutamente nada** del comportamiento
actual (si nada se sale de la pantalla, no hay nada que recortar) y es lo que
te salva de corromper memoria en cuanto empieces a restar la cámara. Puedes
compilar y comprobar que todo sigue igual antes de seguir.

## Paso 2 — Separa mundo de pantalla en tu cabeza

Recorre tu código y clasifica cada uso de una coordenada:

- ¿Decide algo? (colisiones, límites de movimiento, impactos) → **mundo**.
- ¿Pinta algo? → **pantalla**, y lleva la resta de la cámara.

Este paso es de lectura, no de escritura, y es el que evita los bugs raros.

## Paso 3 — Variables de mundo, no constantes

Donde tengas `WIDTH` y `HEIGHT` haciendo de "tamaño del mapa", cámbialo por
variables:

```c
extern int map_width;
extern int map_height;
```

Y deja `WIDTH`/`HEIGHT` significando **solo la pantalla**, para siempre.

Ojo con no cambiarlos por inercia en los sitios que sí son de pantalla: el
volcado a la VGA, el buffer de la imagen, el recorte de sprites. Esos son 320 y
200 eternamente.

## Paso 4 — La máscara de colisión del mundo entero

Si tu juego lee un mapa de colisión, empaquétalo a un bit por píxel y cárgalo
completo. Si tu juego es de un solo jugador y no hay red, podrías tener solo la
parte visible, pero no te lo recomiendo: la complicación no compensa los bytes.

## Paso 5 — Los límites de movimiento

Si tus límites eran la pantalla, ahora son el mapa. O, mejor todavía, quítalos y
que sea el muro pintado en el mapa quien pare al jugador. Eso es lo que hace
este juego: **el límite dejó de ser código y pasó a ser dato.**

Si haces eso, dos avisos:

- **Deja las guardas de desbordamiento.** Los `if (position >= PASO)` antes de
  restar no son límites de pantalla, son protección contra que un `unsigned`
  dé la vuelta a 65.535. Con un borde bien pintado nunca se disparan, pero
  están ahí para el día que dibujes mal un mapa.
- **Borde grueso**, mínimo 8 píxeles (parte 7.7).

## Paso 6 — La ventana y la cámara

Ahora sí: `bmp_draw_world_window()`, `bmp_camera_follow()`,
`bmp_camera_snap()` y el `clamp`. Son unas 80 líneas en total, y llegados aquí
ya no tienen sorpresas.

## Paso 7 — Mide la memoria antes de elegir el tamaño del mapa

Antes de decidir si tu mapa es 640x400 o 1280x800, mete las dos líneas de
`coreleft()`/`farcoreleft()` de la parte 9.6, compila y mira el número. Es lo
único que te dice si te lo puedes permitir.

Recuerda el techo: el mapa a un byte por píxel más la máscara a un bit por
píxel. Un mundo de 1280x800 son 1.024.000 bytes de dibujo. No caben ni de
lejos.

---

# PARTE 11 — REFERENCIA

## 11.1 Variables globales

| Variable | Tipo | Qué es |
|---|---|---|
| `map_width` | `int` | Ancho del mundo en píxeles. 320 o 640 |
| `map_height` | `int` | Alto del mundo en píxeles. 200 o 400 |
| `camera_x` | `int` | Esquina izquierda de la ventana, en mundo. 0..(map_width-320) |
| `camera_y` | `int` | Esquina superior de la ventana, en mundo. 0..(map_height-200) |
| `buffer_original_background_bmp` | `unsigned char huge *` | El mapa entero. `farmalloc` |
| `buffer_background_image_data` | `unsigned char *` | El frame que se está construyendo. Siempre 64.000 |
| `buffer_collision_mask` | `unsigned char *` | Los muros del mundo, 1 bit por píxel |
| `file_sprites_game_open` | `FILE *` | `sprites.bmp`, abierto y nunca cargado |

## 11.2 Constantes

| Constante | Valor | Qué es |
|---|---|---|
| `WIDTH` | 320 | Ancho de la **pantalla**. Nunca cambia |
| `HEIGHT` | 200 | Alto de la **pantalla**. Nunca cambia |
| `SCREEN_SIZE` | 64000 | 320*200, el buffer de pantalla |
| `MAP_MAX_WIDTH` | 640 | Solo dimensiona el buffer de una fila del cargador |
| `MAP_WALL_COLOR` | 252 | Índice de paleta que significa muro |
| `CAMERA_DEAD_ZONE_X` | 100 | Margen horizontal de la zona muerta. Máximo 150 |
| `CAMERA_DEAD_ZONE_Y` | 70 | Margen vertical de la zona muerta. Máximo 90 |

## 11.3 Funciones

### Arranque

| Función | Qué hace |
|---|---|
| `bmp_init_buffers(width, height)` | Reserva todo y fija `map_width`/`map_height` |
| `bmp_fill_background_in_main_buffer(file)` | Carga el dibujo del mapa |
| `bmp_fill_background_collision_in_buffer(file)` | Carga y empaqueta la máscara |
| `bmp_open_sprite_sheet(file)` | Abre `sprites.bmp` y lo deja abierto |
| `bmp_close_sprite_sheet()` | Lo cierra, ya recortados los sprites |

### Cámara

| Función | Qué hace |
|---|---|
| `bmp_camera_follow(x, y, w, h)` | Zona muerta: mueve la ventana solo si hace falta |
| `bmp_camera_snap(x, y, w, h)` | Centra la ventana en el objetivo, de golpe |

Las dos reciben **enteros**, no un puntero a jugador, para que `bmp.c` no
dependa de la estructura de tu juego.

### Dibujo

| Función | Qué hace |
|---|---|
| `bmp_draw_world_window(destino)` | Copia la ventana visible al buffer de pantalla |
| `bmp_extract_sprite(sx, sy, w, h, destino)` | Recorta un sprite del fichero |
| `draw_sprite_to_buffer(...)` | Pinta un sprite con recorte. Coordenadas de **pantalla**, con signo |
| `bmp_paint_image_data_to_vga(buffer)` | Vuelca el buffer de pantalla a la VGA |

### Consulta

| Función | Coordenadas | Qué hace |
|---|---|---|
| `bmp_is_wall(x, y)` | **Mundo** | 1 si es muro. **La única válida para decidir** |
| `bmp_get_map_pixel(x, y)` | **Mundo** | Color del dibujo. Solo para depurar |
| `bmp_get_vga_pixel(x, y)` | **Pantalla** | Lo que hay en la VGA. Con los tanques ya encima |

## 11.4 El frame completo, en orden

```
  1. Leer teclado / recibir teclas del otro por la red
  2. Mover tanques y balas          <- coordenadas de MUNDO
  3. Comprobar colisiones           <- bmp_is_wall(), coordenadas de MUNDO
  4. update_camera(0)               <- decide donde esta la ventana
  5. bmp_draw_world_window()        <- fondo
  6. draw_sprite_to_buffer() x N    <- todo lo demas, mundo MENOS camara
  7. wait_retrace()
  8. bmp_paint_image_data_to_vga()  <- 64000 bytes a la pantalla
  9. Calcular checksum y avanzar frame   <- SIN la camara dentro
```

---

# PARTE 12 — ERRORES TÍPICOS

Los que de verdad ocurrieron, y cómo se ven.

### "El tanque se para y no puede salir de la primera pantalla"

Se te quedó un límite de movimiento contra `WIDTH` en vez de contra
`map_width`. El tanque frena en x=302 y no hay forma de que llegue al resto del
mapa.

### "Se cuelga el juego, o pasan cosas raras minutos después"

Coordenadas `unsigned` en la función de dibujar sprites. Un `-9` se convierte en
`65527` y escribes muy lejos del buffer. Como en DOS no hay protección de
memoria, el daño aparece más tarde y en otro sitio.

### "El fondo es basura y los tanques se ven horribles"

Alguna reserva de memoria devolvió NULL y el código siguió adelante. Mira el
log: si `init_graphics` consumió menos bytes de los que debería, algo no se
reservó.

### "Va bien hasta que me acerco a un borde y ahí se vuelve loco"

Falta el `clamp`, o le falta un caso. La ventana está leyendo fuera del mapa.

### "La pantalla parpadea entre dos vistas"

Estás en el modelo de habitaciones (parte 1.3) y el jugador está justo en la
frontera. Necesitas histéresis, o pasarte a la zona muerta.

### "Desincronización en el frame 1 de toda partida en red"

Metiste `camera_x` en el checksum. Cada máquina tiene la suya y es correcto que
sean distintas.

### "En red los tanques hacen cosas sin sentido"

Las dos máquinas no arrancaron en el mismo modo, o cargaron mapas distintos. El
checksum debería cantarlo gracias a `map_width` (parte 8.4).

### "El tanque atraviesa una pared fina"

El muro tiene menos píxeles de grosor que el paso del tanque (2) o el de la
bala (3). Píntalo de 8 como mínimo.

### "No puedo pasar por una puerta"

El hueco mide lo mismo que el tanque. Necesita ser más ancho, porque el tanque
solo ocupa posiciones de 2 en 2 y casi nunca cae en el único valor que encaja.

### "Los colores del mapa salen mal"

Cada BMP lleva su propia paleta. Si cargas la paleta de un fichero y el dibujo
de otro con paleta distinta, verás colores equivocados. Todos los BMP del juego
comparten paleta, byte a byte.

---

# GLOSARIO

**Buffer**. Un trozo de memoria reservado para guardar algo. Aquí, imágenes.

**Clamp**. Recortar un valor para que se quede dentro de un rango. La cámara se
"clampea" para que la ventana no salga del mapa.

**Doble buffer**. Construir el frame en memoria y volcarlo a la pantalla de una
vez, en lugar de pintar directamente sobre lo que se está viendo. Evita el
parpadeo.

**Fragmentación**. Que la memoria libre esté partida en trozos separados por
memoria ocupada. Puede haber 130 KB libres y no caber un bloque de 42 KB.

**Huge (puntero)**. Puntero de DOS que se normaliza en cada operación
aritmética, para poder recorrer bloques de más de 64 KB sin dar la vuelta.

**Máscara de bits**. Guardar un sí/no por elemento usando un bit en vez de un
byte. Ocho veces menos memoria.

**Modo 13h**. El modo gráfico de VGA de 320x200 con 256 colores y un byte por
píxel. El que usa este juego.

**Normalizar (un puntero)**. Reajustar segmento y desplazamiento para que el
desplazamiento quede entre 0 y 15.

**Paleta**. La tabla de 256 colores. Los píxeles no guardan color, guardan un
número del 0 al 255 que es una posición en esta tabla.

**Recorte (clipping)**. Dibujar solo la parte de una cosa que cae dentro de la
pantalla, descartando el resto sin escribirlo.

**Retrazo vertical (retrace)**. El instante en que el haz del monitor vuelve
arriba y no está dibujando. Volcar el buffer justo entonces evita ver medio
frame viejo y medio nuevo.

**Stride (paso de fila)**. Cuántos bytes hay que avanzar en memoria para bajar
una fila en la imagen. Es el ancho de la imagen, no el de la pantalla, y
confundirlos es el error clásico.

**Zona muerta**. El rectángulo del centro de la pantalla dentro del cual el
jugador se puede mover sin que la cámara reaccione.

---

# APÉNDICE — QUÉ SE PROBÓ Y CÓMO

Este código nunca se compiló en la máquina DOS para probarlo: se verificó
antes, en Linux, compilando `bmp.c` tal cual y ejecutándolo con datos reales.
Merece la pena saber que se puede hacer.

| Prueba | Qué comprueba | Resultado |
|---|---|---|
| Recorte exhaustivo | Un sprite 18x18 en 112.681 posiciones, de (-40,-40) a (360,240), con bytes centinela alrededor del buffer | 0 escrituras fuera |
| Conteos de recorte | Entero dentro = 324 px; medio fuera = 162; esquina = 81; fuera = 0 | Correctos |
| Máscara de colisión | Los 256.000 píxeles del mundo comparados uno a uno contra `bigcol.bmp` | 0 diferencias |
| Fuera del mapa | `bmp_is_wall()` en las cuatro direcciones fuera de rango | Devuelve muro |
| Zona muerta | Que no se mueva dentro, ni en el borde justo, y que empuje exactamente lo que se sale | Correcto |
| Clamp | Que no se salga por ninguno de los cuatro lados | Correcto |
| Mapa de una pantalla | Que con 320x200 la cámara quede clavada en (0,0) | Correcto |
| Sprites del fichero | Los 24 sprites comparados byte a byte contra el método antiguo | 0 diferencias |
| Paseo completo | 200 frames de tanque andando por el mapa real, con fotogramas renderizados a PNG | Cámara quieta el 88% |

Los fotogramas renderizados fueron especialmente útiles: dejan **ver** el
resultado sin arrancar DOSBox, y fue así como se comprobó que una bala
disparada fuera de la ventana entra de verdad por el borde.
