# Capítulo 21 — La ventana: sacar un trozo de un mapa grande

**Qué vas a conseguir:** mover la ventana a mano por el mundo de 640x400, con un
tanque quieto, y entender exactamente qué es una ventana antes de que nada la
mueva por ti.

**Qué código real se usa:** `src/bmp.c` (`bmp_draw_world_window`),
`src/players.c`.

```
make
chap21
```

Flechas: mueven **la ventana**. MAYÚS para ir rápido. ESC para salir.

---

## 1. Aquí no hay cámara

Esto es importante para leer bien el capítulo.

**Las flechas no mueven ningún tanque.** Mueven `camera_x` y `camera_y`
directamente. El tanque está clavado en el mundo en **(300, 190)** y no se mueve
en ningún momento del programa:

```c
	tank.position_x = TANK_WORLD_X;    /* y nadie vuelve a tocarlo */
	tank.position_y = TANK_WORLD_Y;
```

**Y aun así lo vas a ver deslizarse por la pantalla.**

Esa contradicción aparente es toda la lección. Una cosa es **dónde está** algo y
otra **dónde se pinta**.

## 2. Recordatorio del capítulo 1: el stride

En el capítulo 1 dijimos que un píxel está en:

```
   posicion = y * 320 + x
```

y que ese **320 tiene nombre: el stride o paso de fila**, *cuántos bytes hay que
avanzar para bajar una fila*. Y avisé de que aquí dejaría de coincidir con el
ancho de la pantalla.

Ha llegado el momento.

Una imagen no está guardada como un rectángulo. Está guardada como **una tira
de bytes**, fila tras fila:

```
  memoria:  [ fila 0 (640 bytes) ][ fila 1 (640 bytes) ][ fila 2 ] ...
```

Para el mapa de 640×400, el stride es **640**. Para la pantalla sigue siendo
**320**. Son dos imágenes distintas con dos pasos de fila distintos, y ahí está
la dificultad.

## 3. Por qué un `memcpy` ya no vale

Hasta el capítulo 20, el fondo se copiaba así:

```c
	memcpy(destino, mapa, 64000);
```

Funcionaba porque los 64.000 bytes del mapa eran, **en el mismo orden**, los
64.000 bytes de la pantalla. Fila 0 del mapa → fila 0 de pantalla. Todo seguido.

Ahora mira lo que quieres copiar: una ventana de **320 de ancho** dentro de un
mapa de **640 de ancho**.

```
  EL MUNDO, 640 de ancho:

  fila 100:  ....................[XXXXXXXXXXXXXXXX]....................
  fila 101:  ....................[XXXXXXXXXXXXXXXX]....................
  fila 102:  ....................[XXXXXXXXXXXXXXXX]....................
                                  ^                ^
                                  camera_x         camera_x + 320


  Y EN MEMORIA, esas tres filas estan asi de separadas:

  [...320... XXXX ...320...][...320... XXXX ...320...][...320... XXXX ...]
             ^ lo quiero               ^ lo quiero               ^ lo quiero
             |<------------ 640 bytes ------------>|
```

**Los trozos que quieres no están pegados.** Entre el final de uno y el
principio del siguiente hay **320 bytes que no quieres**.

Un solo `memcpy` no sabe saltárselos. Copia bytes seguidos y punto.

## 4. La solución: una fila cada vez

Si no puedes copiarlo de una vez, lo copias en **200 veces**: una por cada fila
de la pantalla.

Esto es `bmp_draw_world_window()` de `src/bmp.c`, entera:

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

Línea por línea:

| | |
|---|---|
| `row` | Va de 0 a 199: las 200 filas de **la pantalla** |
| `camera_y + row` | La convierte en su fila **del mundo**. Si la cámara está en y=100, la fila 0 de pantalla es la fila 100 del mundo |
| `* map_width` | Salta esa cantidad de **filas enteras del mundo**, 640 bytes cada una |
| `+ camera_x` | Avanza dentro de la fila hasta donde empieza la ventana |
| `memcpy(..., WIDTH)` | Copia 320 bytes: **una fila de pantalla completa** |
| `destination_offset += WIDTH` | Avanza 320 en el destino, donde las filas **sí** van seguidas |

## 5. No es más trabajo

Esto sorprende: **se copian exactamente los mismos bytes que antes.**

```
   200 filas x 320 bytes = 64.000 bytes
```

Los mismos 64.000 del `memcpy` de siempre. No es más trabajo: es **el mismo
trabajo repartido en 200 llamadas** en lugar de una.

Lo único que se paga de más es la sobrecarga de llamar a `memcpy` 200 veces en
lugar de una, y en un 486 eso no se nota.

## 6. Un ejemplo con números

Cámara en **(320, 100)**. Quieres la fila **0 de la pantalla**.

```
   fila del mundo   = camera_y + row = 100 + 0   = 100
   salto de filas   = 100 * 640                 = 64.000
   avance en la fila= camera_x                  = 320
   ------------------------------------------------------
   posicion en el mapa                          = 64.320
```

Copias 320 bytes desde el byte 64.320 del mapa, al byte 0 de la pantalla.

Ahora la fila **1**:

```
   fila del mundo   = 100 + 1 = 101
   salto            = 101 * 640 = 64.640
   + 320
   -----------------------------------
   posicion         = 64.960
```

Fíjate: **64.320 → 64.960 son 640 bytes de salto**, aunque solo copiaste 320.
Ese hueco de 320 bytes es la parte del mundo que queda a la derecha de tu
ventana.

## 7. El detalle raro: `source` se reconstruye entera cada vuelta

Mira otra vez el bucle. La dirección se calcula **desde el principio** cada vez,
en lugar de hacer `source = source + map_width` al final, que sería lo natural.

Parece un desperdicio. **Es deliberado**, y tiene que ver con cómo funcionan los
punteros en DOS.

El resumen: un puntero `far` solo puede recorrer 64 KB antes de dar la vuelta, y
el mapa ocupa 256.000 bytes. Reconstruir la dirección desde la base obliga al
compilador a **normalizar** el puntero, y un puntero normalizado nunca se sale
de su segmento cuando le sumas 320.

Si lo fueras acumulando, en algún momento el `memcpy` cruzaría una frontera de
segmento y copiaría de otro sitio.

**El capítulo 23 lo explica entero**, con qué es un puntero `huge` y por qué el
mapa está declarado así. De momento quédate con que ese `source =` completo no
es torpeza.

## 8. El clamp, a mano

En este capítulo lo escribes tú:

```c
	if (camera_x < 0){ camera_x = 0; }
	if (camera_y < 0){ camera_y = 0; }
	if (camera_x > map_width  - WIDTH ){ camera_x = map_width  - WIDTH;  }
	if (camera_y > map_height - HEIGHT){ camera_y = map_height - HEIGHT; }
```

| | Rango de la ventana |
|---|---|
| `camera_x` | 0 … 640-320 = **320** |
| `camera_y` | 0 … 400-200 = **200** |

`map_width - WIDTH` es lo más a la derecha que puede estar la ventana sin que su
borde derecho se salga del mapa.

**Sin esto**, `bmp_draw_world_window()` leería de antes del principio del mapa o
de después del final. Y en DOS eso no da error: te dibuja basura, o cuelga.

Quítalo y vete a una esquina: lo vas a ver.

## 9. Lo que demuestra el programa

Al salir imprime tres líneas:

```
  El tanque, en el MUNDO    : (300, 190)   <- no ha cambiado nunca
  La ventana                : (128, 104)
  El tanque, en la PANTALLA : (172, 86)
```

El tanque **no se ha movido**. Lo único que cambió es la ventana. Y sin embargo
lo has visto recorrer la pantalla entera.

Si la de pantalla sale fuera de 0..319 / 0..199, el tanque estaba fuera de la
ventana y no lo veías — **y seguía exactamente en el mismo sitio del mundo**.

## 10. Experimentos

1. **Vete a una esquina y anota los tres números.** Comprueba la resta a mano.
2. **Quita el clamp** y sal por arriba a la izquierda. Basura en pantalla: estás
   leyendo de antes del mapa.
3. **Cambia el `* map_width` por `* WIDTH`** en `bmp_draw_world_window()` (en
   `src/bmp.c`, y luego deshazlo). El stride equivocado: la imagen sale
   inclinada y repetida. **Ese es el aspecto del bug del stride**, y conviene
   habérselo visto una vez.
4. **Cambia el `memcpy(..., WIDTH)` por `memcpy(..., 160)`.** Media pantalla se
   queda con lo del frame anterior.
5. **Pon `camera_x = 320` fijo** y no lo muevas. Estás viendo la mitad derecha
   del mundo, permanentemente. Eso es una "habitación" fija, el modelo que se
   descartó.

## 11. Lo que hay que llevarse

| | |
|---|---|
| **El stride es el ancho de LA IMAGEN**, no el de la pantalla | Aquí dejan de coincidir |
| Las filas de la ventana **no están pegadas** en memoria | Por eso no vale un `memcpy` |
| **200 `memcpy` de 320 bytes** | Los mismos 64.000 bytes, repartidos |
| `source` se reconstruye entera a propósito | Punteros `huge`, capítulo 23 |
| El clamp encierra la ventana en el mapa | Sin él, lees fuera |
| **Mover la ventana no mueve el mundo** | El tanque no se movió nunca |

---

**Anterior:** [Capítulo 20](../../ch20/doc/README.md) ·
**Siguiente:** [Capítulo 22 — La cámara](../../ch22/doc/README.md)
