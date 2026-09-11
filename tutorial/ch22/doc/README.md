# Capítulo 22 — La cámara: que la ventana se mueva sola

*[English version](README-EN.md)*

**Qué vas a conseguir:** la ventana del capítulo 21, pero siguiendo al tanque sin
que tú la toques. Y comparar los **tres modelos** con una tecla.

```
make
chap22
```

Flechas: mueven **el tanque** (ya no la ventana).
**`C` cambia de modelo:** 0 sin cámara, 1 zona muerta, 2 siempre centrada.

---

## 1. Lo que ya tienes

Del capítulo 21 ya sabes:

- Una ventana son **dos números**: `camera_x`, `camera_y`
- `pantalla = mundo − cámara`
- `bmp_draw_world_window()` copia el trozo, con sus 200 `memcpy` y su *stride*
- El **clamp** la encierra en el mapa

Allí movías esos dos números **a mano**. Este capítulo es solo una cosa:

> **quién mueve esos dos números, y con qué criterio**

Nada más. La ventana ya funcionaba.

## 2. Los tres modelos, con la tecla `C`

### Modo 0 — Sin cámara

El capítulo 20. Está aquí para comparar de un vistazo.

### Modo 2 — Siempre centrada

```c
	bmp_camera_snap((int)tank.position_x, (int)tank.position_y, TANK_WIDTH, TANK_HEIGHT);
```

El tanque clavado en el centro y el mundo moviéndose debajo. Es lo primero que
se le ocurre a uno, y es de los tres el peor.

**Pruébalo un rato de verdad, no cinco segundos.** El mundo se mueve en **todos
y cada uno de los frames**, incluso cuando das un pasito de 2 píxeles para
ajustar la puntería. Y te quita la sensación de estar moviendo tu tanque: lo ves
quieto y lo que se mueve es el suelo.

### Modo 1 — Zona muerta (el del juego)

La cámara **no se mueve** mientras el tanque esté dentro de un rectángulo en el
centro de la pantalla:

```
   LA PANTALLA, 320x200

   +----------------------------------------+
   |                                        |
   |          <-- 70 px -->                 |
   |     +----------------------------+     |
   |     |                            |     |
   |     |        ZONA MUERTA         |     |
   |     |   aqui la camara NO se     |     |
   |     |          mueve             |     |
   |     |                            |     |
   |     +----------------------------+     |
   |          <-- 70 px -->                 |
   |                                        |
   +----------------------------------------+
    <- 100 px ->                <- 100 px ->
```

Midiendo un paseo de 200 frames por el mapa real, **la cámara está quieta el 88%
de los frames**. Eso es exactamente lo que se busca: que el mundo solo se mueva
cuando de verdad vas a algún sitio.

## 3. De dónde salen el 100 y el 70

```c
#define CAMERA_DEAD_ZONE_X 	100
#define CAMERA_DEAD_ZONE_Y 	 70
```

Son los **márgenes** desde el borde de la pantalla hasta el borde de la zona
muerta. Y no son el mismo número por un motivo concreto: **la pantalla no es
cuadrada.**

| | Pantalla | Máximo posible | Usado | Carril resultante |
|---|---:|---:|---:|---:|
| **X** | 320 | (320−18)/2 = **151** | 100 | 320−100−100−18 = **102 px** |
| **Y** | 200 | (200−18)/2 = **91** | 70 | 200−70−70−18 = **42 px** |

En vertical hay 120 píxeles menos de pantalla para repartir, así que el margen
tiene que ser más pequeño o no quedaría zona muerta.

### El límite que no puedes pasar

Fíjate en la columna "máximo posible". El margen tiene que ser **menor que la
mitad de la pantalla menos el tanque**.

Si lo pasas, el borde izquierdo de la zona muerta queda **a la derecha** del
derecho. Los dos empujes se disparan a la vez y **la cámara se pelea consigo
misma**, temblando en el sitio.

Pruébalo: pon `CAMERA_DEAD_ZONE_X` a 160 y compila.

## 4. El código, línea por línea

```c
void bmp_camera_follow(int target_x, int target_y, int target_width, int target_height){

	int screen_x;
	int screen_y;
	int right_edge;
	int bottom_edge;

	/* 1. Donde esta el objetivo DENTRO de la ventana ahora mismo */
	screen_x = target_x - camera_x;
	screen_y = target_y - camera_y;

	/* 2. Donde estan los bordes de la zona muerta */
	right_edge  = WIDTH  - CAMERA_DEAD_ZONE_X - target_width;
	bottom_edge = HEIGHT - CAMERA_DEAD_ZONE_Y - target_height;

	/* 3. Empujar solo si se ha salido */
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

### Por qué `- target_width`

```c
	right_edge = WIDTH - CAMERA_DEAD_ZONE_X - target_width;
```

Porque **`position_x` es la esquina superior IZQUIERDA** del sprite, no su
centro. El borde derecho del tanque está 18 píxeles más allá.

Sin restarlo, el margen derecho se mediría contra la esquina izquierda y el
tanque se metería 18 píxeles de más en la zona de empuje: la zona muerta
quedaría descentrada.

### Por qué `else if` y no dos `if`

Con la zona muerta bien dimensionada (punto 3) los dos casos son excluyentes: no
puedes estar a la vez a la izquierda del borde izquierdo y a la derecha del
derecho.

Pero **si alguien pone un margen demasiado grande**, los dos serían ciertos a la
vez y con dos `if` sueltos se aplicarían las dos correcciones, una detrás de
otra, cada frame. El `else if` limita el daño a un temblor en vez de a una
cámara disparada.

### Por qué recibe enteros y no un `struct player *`

```c
	bmp_camera_follow((int)tank.position_x, (int)tank.position_y, TANK_WIDTH, TANK_HEIGHT);
```

A propósito: así **`bmp.c` sigue sin saber qué es un tanque**. Mañana la cámara
puede seguir a una nave, a un ratón o al punto medio de dos cosas, y `bmp.c` no
se entera.

Es la misma decisión que se tomó con `sound.c` (el log se lo pasas tú) y con
`net.c`. Una librería no debe conocer a su cliente.

## 5. "Exactamente lo que se ha salido"

Esta es la parte elegante, y es fácil pasarla por alto.

La corrección es `screen_x - right_edge`: **cuántos píxeles se ha salido**. Ni
más ni menos.

Sigue un tanque andando a la derecha a 2 píxeles por frame, con la cámara en 0:

| Frame | Tanque (mundo) | Cámara | En pantalla | Qué pasa |
|---|---:|---:|---:|---|
| 1 | 250 | 0 | 250 | 250 > 202: se sale **48** → cámara +48 |
| | | 48 | **202** | queda justo en el borde |
| 2 | 252 | 48 | 204 | se sale **2** → cámara +2 |
| | | 50 | **202** | otra vez en el borde |
| 3 | 254 | 50 | 204 | se sale **2** → cámara +2 |
| | | 52 | **202** | |

**El tanque anda 2, la cámara anda 2.** El tanque se queda pegado al borde de la
zona muerta y el mundo se desliza detrás a la misma velocidad exacta.

Ni salto (porque el desplazamiento es continuo) ni retraso (porque la corrección
es exacta). Y al soltar la tecla, **la cámara para en seco** con él.

### Compara con un suavizado

Lo típico es escribir algo así:

```c
	camera_x = camera_x + (objetivo - camera_x) / 8;     /* NO es lo que hace el juego */
```

Se siente "cinematográfico" y para este juego es peor:

- **Siempre va con retraso**: nunca alcanza el objetivo, solo se le acerca
- **Sigue moviéndose después de que pares**, con inercia
- Y en un juego donde apuntas con el morro del tanque, esa inercia molesta al
  ajustar

La zona muerta no tiene ninguno de esos dos problemas porque no persigue nada:
**solo empuja cuando debe, y lo justo**.

## 6. La resta va en TODOS los objetos

En este capítulo solo hay un tanque, así que se ve una sola resta. En el juego
real hay **cinco**:

```c
	draw_sprite_to_buffer(sprite1, ..., (int)player1.position_x - camera_x, ...);
	draw_sprite_to_buffer(sprite2, ..., (int)player2.position_x - camera_x, ...);
	draw_sprite_to_buffer(bala1,   ..., (int)player1.bullet_position_x - camera_x, ...);
	draw_sprite_to_buffer(bala2,   ..., (int)player2.bullet_position_x - camera_x, ...);
	draw_sprite_to_buffer(boom,    ..., (int)(p->position_x + OFFSET) - camera_x, ...);
```

**Si se te olvida en uno solo**, el síntoma es muy característico y muy fácil de
reconocer:

> Ese objeto se queda **pegado a la pantalla** mientras todo lo demás se desliza.

Una bala que te sigue a todas partes en vez de quedarse donde la disparaste. Una
explosión que viaja contigo. Una vez lo has visto, lo diagnosticas en dos
segundos.

Y el error simétrico: **restarla dos veces** hace que el objeto se mueva al
doble de velocidad y en sentido contrario.

## 7. Aquí se cobra el recorte del capítulo 4

```c
	screen_x = (int)tank.position_x - camera_x;
```

Esa resta **da negativo constantemente**: cada vez que el tanque se acerca al
borde izquierdo de la pantalla, o cuando la cámara está topada y el tanque se
aleja.

En el capítulo 4, `-9` convertido a `unsigned int` valía **65.527** y escribía a
65.000 bytes del buffer. Aquí eso pasaría **todo el rato**, no como caso raro.

El recorte dejó de ser prudencia y pasó a ser el mecanismo normal de
funcionamiento.

## 8. El clamp, ahora automático

```c
	limit_x = map_width  - WIDTH;     /* 640 - 320 = 320 */
	limit_y = map_height - HEIGHT;    /* 400 - 200 = 200 */
```

Lo mismo que escribiste a mano en el capítulo 21, ahora dentro de
`bmp_camera_clamp()`, y llamado al final de `follow` y de `snap`.

Cuando la cámara está topada, **el tanque sí se sale de la zona muerta** y se
acerca al borde de la pantalla. Es lo correcto: no hay más mapa que enseñar, así
que lo que se mueve vuelve a ser el tanque.

## 9. Y aquí está lo bonito: el modo normal es el mismo código

Con un mapa de 320x200:

```
   limit_x = 320 - 320 = 0
   limit_y = 200 - 200 = 0
```

El clamp deja la cámara **clavada en (0,0) para siempre**, calcule lo que
calcule `bmp_camera_follow()`.

Y entonces:

- `mundo − cámara` es `mundo − 0`, o sea `mundo`
- `bmp_draw_world_window()` copia 200 filas de 320 empezando en (0,0), que es
  **exactamente el `memcpy` de 64.000 bytes de toda la vida**
- El recorte no recorta nada, porque nada se sale

> **Un mundo de una pantalla no es un caso especial: es el caso general con la
> cámara topada en cero.**

Por eso el juego **no tiene ni un solo `if (big_map_mode)` en el dibujado**. No
hay dos caminos que mantener ni dos sitios donde meter la pata.

Está comprobado con un test: llama a `follow` y a `snap` con valores absurdos y
un mapa de 320x200, y verifica que la cámara sigue en (0,0).

## 10. El `snap`

```c
	camera_x = target_x + (target_width  / 2) - (WIDTH  / 2);
	camera_y = target_y + (target_height / 2) - (HEIGHT / 2);
	bmp_camera_clamp();
```

Centra el objetivo de golpe: coge su centro y le resta media pantalla.

Es para el arranque de una ronda. Cuando los tanques se teletransportan a sus
esquinas **no hay nada desde lo que seguir suavemente**: la cámara tiene que
aparecer ya puesta.

Y aplica el mismo clamp, así que en una esquina se queda en el borde en vez de
enseñarte el vacío de fuera del mapa.

## 11. El orden dentro del frame

```c
	update_camera();                                   /* 1. donde esta la ventana */
	bmp_draw_world_window(buffer);                     /* 2. el fondo */
	draw_sprite_to_buffer(..., mundo - camara, ...);   /* 3. todo lo demas */
	wait_retrace();                                    /* 4. esperar al monitor */
	bmp_paint_image_data_to_vga(buffer);               /* 5. volcar */
```

La cámara se decide **antes** de pintar nada.

Si la movieras entre el paso 2 y el 3, el fondo sería de una posición y los
tanques de otra: saldrían **desplazados respecto al suelo**, flotando. Un frame
sí y otro no, según cuándo cambiara.

## 12. En red, cada máquina tiene la suya

```c
	if (local_player_is_1 == 0){ target = &player2; }
```

Cada máquina sigue a **su propio tanque**. Los dos `camera_x` valen cosas
distintas, **a propósito**.

¿No rompe eso el determinismo del capítulo 18? **No**, y la razón es la regla de
oro: la cámara **no decide nada del juego**. Las dos máquinas hacen las mismas
cuentas sobre las mismas coordenadas de mundo y obtienen el mismo resultado;
luego cada una pinta un trozo distinto de ese resultado idéntico.

Es como dos personas mirando el mismo tablero de ajedrez desde lados opuestos.
Ven cosas distintas. La partida es la misma.

Por eso `camera_x` **nunca** entra en el checksum (capítulo 19): daría una
desincronización falsa en el frame 1 de todas las partidas.

Y por eso `/bigmap` **solo existe en modo red**: una cámara solo puede seguir a
un tanque, y en un teclado compartido uno de los dos jugadores conduciría a
ciegas.

## 13. Experimentos

1. **Pulsa `C` y pasea con los tres modelos**, un minuto largo con cada uno. El
   2 marea de verdad; hay que darle tiempo.
2. **Sube `CAMERA_DEAD_ZONE_X` a 160** en `header/bmp.h`. Se pasa del límite de
   151: la cámara tiembla.
3. **Ponlo a 0.** Se convierte en el modo 2.
4. **Ponlo a 145**, casi el límite. Zona muerta de 12 píxeles: la cámara se
   mueve casi siempre pero sin mareo. Interesante punto medio.
5. **Cambia el `else if` por dos `if`** y pon el margen a 160. Ahora sí se
   dispara de verdad.
6. **Quita el `- target_width`.** La zona muerta queda descentrada 18 píxeles y
   se nota al ir hacia la derecha.
7. **Sustituye `bmp_camera_follow()` por el suavizado** del punto 5
   (`camera_x += (objetivo - camera_x) / 8`) y compara la sensación al ajustar
   la puntería.

## 14. Lo que hay que llevarse

| | |
|---|---|
| La ventana ya funcionaba (ch21). Esto es **quién la mueve** | |
| **Zona muerta**: quieta el 88% del tiempo | Ni salto ni mareo |
| Empuja **exactamente lo que se ha salido** | Tanque 2, cámara 2 |
| Los márgenes tienen un techo | Por encima, la cámara tiembla |
| `- target_width` porque la posición es la **esquina** | |
| **La resta va en TODOS los objetos** | Olvidarla = objeto pegado a la pantalla |
| **Un mundo de una pantalla es el caso general con clamp 0** | Un solo código |
| En red cada máquina tiene la suya, y está bien | Nunca en el checksum |

---

**Anterior:** [Capítulo 21](../../ch21/doc/README.md) ·
**Siguiente:** [Capítulo 23 — La máscara de bits y la memoria](../../ch23/doc/README.md)
