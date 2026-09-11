# Capítulo 3 — Doble buffer y retrazo vertical

*[English version](README-EN.md)*

**Qué vas a conseguir:** ver el mismo movimiento tres veces, cada vez mejor, y
entender por qué.

**Qué código real se usa:** `src/bmp.c`.

```
make
chap03
```

---

## 1. El problema: la pantalla se lee mientras la escribes

El monitor no muestra una imagen fija: la **redibuja** unas 70 veces por
segundo, leyendo los 64.000 bytes de la VGA de arriba abajo, sin parar.

Y tú estás escribiendo en esos mismos bytes al mismo tiempo.

Para mover una barra tienes que hacer dos cosas:

1. Borrar la barra de donde estaba
2. Pintarla donde está ahora

Entre las dos hay un instante en el que **la barra no está en ningún sitio**.
Si el monitor pasa por ahí justo en ese momento, dibuja una pantalla sin barra.
A 70 veces por segundo, eso es un **parpadeo**.

Eso es la pasada 1 del programa.

## 2. La solución: construir el frame donde nadie mira

El **doble buffer**: en vez de dibujar en la pantalla, dibujas en un trozo de
memoria normal y corriente. Nadie lo está mirando, así que puedes tardar lo que
quieras y dejarlo a medias todas las veces que haga falta.

Cuando el frame está **terminado**, lo copias entero a la VGA de un golpe.

```c
	bmp_draw_world_window(buffer_background_image_data);   /* fondo */
	draw_bar(buffer_background_image_data, x);             /* la barra */
	bmp_paint_image_data_to_vga(buffer_background_image_data); /* de golpe */
```

Ese `buffer_background_image_data` es exactamente eso: el frame en
construcción. Ya lo reservaba `bmp_init_buffers()` en el capítulo 2.

Adiós al parpadeo. Esa es la pasada 2.

## 3. Pero todavía queda el desgarro

La copia de 64.000 bytes no es instantánea. Tarda. Y el monitor sigue leyendo
mientras tanto.

Si el monitor va por la fila 100 y tú vas copiando por la fila 50, entonces:

- las filas 0 a 50 que ya ha leído son del frame **viejo**
- las filas 100 en adelante que va a leer son del frame **nuevo**

Ves media pantalla de cada, con una costura horizontal. Se llama **tearing**, y
en la pasada 2 se nota en la barra: parece partida.

## 4. El retrazo vertical

El monitor, cuando termina la última fila, **no empieza otra vez de inmediato**.
El haz tiene que volver físicamente arriba del todo, y eso lleva un rato: unos
1,4 milisegundos en modo 13h.

Durante ese rato **no está leyendo nada**. Es el momento perfecto para copiar.

```c
static void wait_retrace(void){

	while (inp(0x3DA) & 0x08);      /* esperar a que termine el actual */
	while (!(inp(0x3DA) & 0x08));   /* esperar a que empiece el siguiente */

}
```

El puerto `0x3DA` es un registro de estado de la VGA. Su bit `0x08` vale 1
mientras hay retrazo.

**Los dos bucles son necesarios**, y el primero sorprende:

- Si llegas aquí y el retrazo ya está ocurriendo, te queda muy poco de él.
  Empezar a copiar ahora es llegar tarde.
- Así que el primer bucle espera a que **termine**, y el segundo a que empiece
  el siguiente, entero, desde el principio.

Esa es la pasada 3, y es lo que hace el juego en `main.c`.

## 5. Y de paso te da la velocidad

Efecto lateral: como el retrazo ocurre unas 70 veces por segundo, esperarlo
hace que tu bucle gire a **70 frames por segundo**, clavado, y sin usar ningún
reloj.

El juego se apoya en eso: la velocidad de los tanques está calibrada contra el
retrazo. Si lo quitas, el juego va tan rápido como pueda el procesador y en un
486 es injugable.

## 6. Experimentos

1. **Quita el `wait_retrace()` de la pasada 3.** Vuelve a ser la pasada 2.
2. **En la pasada 1, quita el repintado del fondo** (`bmp_paint_image_data_to_vga(buffer_original_background_bmp)`).
   La barra deja un rastro. Eso demuestra para qué está la copia limpia del
   mapa.
3. **Cambia el paso de `x = x + 2` a `x = x + 8`.** Más rápido, y el desgarro
   de la pasada 2 se nota mucho más.
4. **Deja solo el segundo bucle** de `wait_retrace()`. Casi siempre va bien, y
   de vez en cuando se cuela un desgarro. Los fallos intermitentes son así.

## 7. Lo que hay que llevarse

| | |
|---|---|
| Dibujar directamente en la VGA parpadea | Borras y pintas delante del espectador |
| **Doble buffer**: montar el frame aparte y volcarlo entero | Quita el parpadeo |
| **Retrazo**: volcar cuando el monitor no lee | Quita el desgarro |
| Esperar el retrazo también fija los 70 fps | Sin usar reloj |

---

**Anterior:** [Capítulo 2](../../ch02/doc/README.md) ·
**Siguiente:** [Capítulo 4 — Sprites](../../ch04/doc/README.md)
