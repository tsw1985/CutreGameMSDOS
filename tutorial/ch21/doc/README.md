# Capítulo 21 — La cámara

**Qué vas a conseguir:** moverte por el mundo de 640x400 viéndolo. Y comparar
los **tres modelos de cámara** con una tecla.

```
make
chap21
```

Flechas para moverte. **`C` cambia de modelo:** 0 sin cámara, 1 zona muerta,
2 siempre centrada.

---

## 1. Una cámara son dos números

```c
extern int camera_x;
extern int camera_y;
```

Dicen **dónde está la esquina superior izquierda de la ventana**, en coordenadas
de mundo. No hay zoom, ni rotación, ni objeto, ni clase.

Y convertir de mundo a pantalla es **una resta**:

```
   pantalla = mundo - camara
```

```c
	screen_x = (int)tank.position_x - camera_x;
	screen_y = (int)tank.position_y - camera_y;
```

**Eso es la cámara entera.** El resto son matices sobre dónde poner esos dos
números.

## 2. Los tres modelos, con la tecla `C`

### Modo 0 — Sin cámara

El capítulo 20. Está aquí para comparar.

### Modo 2 — Siempre centrada

El tanque clavado en el centro y el mundo moviéndose debajo.

Es lo primero que se le ocurre a uno **y es peor**. Pruébalo un rato: el mundo
se mueve en **todos y cada uno de los frames**, y te quita la sensación de estar
moviendo tu tanque. En un juego donde haces muchos ajustes pequeños de posición,
marea.

### Modo 1 — Zona muerta (el del juego)

La cámara **no se mueve** mientras el tanque esté dentro de un rectángulo en el
centro:

```
   +----------------------------------+
   |                                  |
   |     +----------------------+     |  <- 70 px de margen
   |     |     ZONA MUERTA      |     |
   |     |  la camara no se     |     |
   |     |     mueve aqui       |     |
   |     +----------------------+     |
   |                                  |
   +----------------------------------+
      ^ 100 px                 100 px ^
```

| | Rango |
|---|---|
| En X | 100 a 320-100-18 = **202** |
| En Y | 70 a 200-70-18 = **112** |

Midiendo un paseo de 200 frames, **la cámara está quieta el 88% del tiempo**.
Eso es lo que se busca: que el mundo solo se mueva cuando de verdad vas a algún
sitio.

## 3. "Exactamente lo que se ha salido"

Esta es la parte elegante, y es fácil pasarla por alto.

```c
	}else if (screen_x > right_edge){
		camera_x = camera_x + (screen_x - right_edge);
	}
```

La corrección es **cuántos píxeles se ha salido**. Ni más ni menos.

| Frame | Tanque | Cámara | En pantalla | |
|---|---|---|---|---|
| 1 | 250 | 0 | 250 | se sale 48 → cámara +48 |
| | | 48 | 202 | justo en el borde |
| 2 | 252 | 48 | 204 | se sale 2 → cámara +2 |
| | | 50 | 202 | |
| 3 | 254 | 50 | 204 | se sale 2 → cámara +2 |

**El tanque anda 2, la cámara anda 2.** Ni salto ni retraso: el tanque se queda
pegado al borde de la zona muerta y el mundo se desliza detrás a la misma
velocidad exacta.

Y cuando sueltas la tecla, **la cámara para en seco** con él.

Compara con un suavizado del tipo `camera_x += (objetivo - camera_x) / 8`: eso
siempre va con retraso y siempre sigue moviéndose un rato después de que pares.
En un juego donde apuntas con el morro, molesta.

## 4. El clamp: no salirse del mapa

```c
	limit_x = map_width  - WIDTH;     /* 640 - 320 = 320 */
	limit_y = map_height - HEIGHT;    /* 400 - 200 = 200 */
```

`camera_x` solo puede valer de 0 a 320. Sin eso, la ventana leería memoria de
antes del principio del mapa.

Cuando la cámara está topada, el tanque **sí** se sale de la zona muerta y se
acerca al borde. Es lo correcto: no hay más mapa que enseñar.

## 5. Y aquí está lo bonito: el modo normal es el mismo código

Con un mapa de 320x200:

- `limit_x = 320 - 320 = 0`
- `limit_y = 200 - 200 = 0`

El clamp deja la cámara **clavada en (0,0) para siempre**. Da igual lo que
calcule `bmp_camera_follow()`.

Y entonces `mundo - camara` es `mundo - 0`, y el recorte no recorta nada.

> **Un mundo de una pantalla no es un caso especial: es el caso general con la
> cámara topada en cero.**

Por eso el juego **no tiene ningún `if (big_map_mode)`** en el dibujado. No hay
dos juegos que mantener.

## 6. El `snap`

```c
	bmp_camera_snap(...)
```

Centra el objetivo de golpe, sin suavizado. Para el arranque de una ronda: el
tanque acaba de aparecer en su esquina y no hay nada desde lo que seguir
suavemente.

Aplica el mismo clamp, así que en una esquina se queda en el borde en vez de
enseñarte el vacío.

## 7. El orden dentro del frame

```c
	update_camera();                                   /* 1. dónde está la ventana */
	bmp_draw_world_window(buffer);                     /* 2. el fondo */
	draw_sprite_to_buffer(..., mundo - camara, ...);   /* 3. todo lo demás */
```

La cámara se decide **antes** de pintar nada. Si la movieras entre pintar el
fondo y pintar los tanques, los tanques saldrían desplazados respecto al suelo.

## 8. En red, cada máquina sigue a su tanque

```c
	if (local_player_is_1 == 0){ target = &player2; }
```

Los dos `camera_x` valen cosas distintas, **a propósito**, y eso **no rompe el
determinismo** porque la cámara no decide nada del juego (capítulo 19).

Es como dos personas mirando el mismo tablero de ajedrez desde lados opuestos:
ven cosas distintas, la partida es la misma.

Y por eso `/bigmap` **solo existe en modo red**: una cámara solo puede seguir a
un tanque, y en un teclado compartido uno de los dos jugadores conduciría a
ciegas.

## 9. Experimentos

1. **Pulsa `C` y pasea con los tres modelos.** El 2 marea; compruébalo.
2. **Sube `CAMERA_DEAD_ZONE_X` a 160** en `header/bmp.h`. Se pasa del límite
   (151) y la cámara tiembla: los dos empujes se pelean.
3. **Ponlo a 0.** Se convierte en el modo 2.
4. **Quita el clamp.** Vete a la esquina de arriba a la izquierda y mira la
   basura que aparece: estás leyendo de antes del mapa.

## 10. Lo que hay que llevarse

| | |
|---|---|
| **Una cámara son dos números y una resta** | Nada más |
| **Zona muerta**: quieta el 88% del tiempo | Ni salto ni mareo |
| Empuja **exactamente lo que se ha salido** | Tanque 2, cámara 2 |
| El clamp la encierra en el mapa | |
| **Un mundo de una pantalla es el caso general con clamp 0** | Un solo código |
| En red cada máquina tiene la suya, y está bien | Nunca en el checksum |

---

**Anterior:** [Capítulo 20](../../ch20/doc/README.md) ·
**Siguiente:** [Capítulo 22 — La máscara de bits y la memoria](../../ch22/doc/README.md)
