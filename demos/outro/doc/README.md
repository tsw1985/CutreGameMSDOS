# outro — La pantalla de despedida

*[English version](README-EN.md)*

**No es uno de los diecinueve efectos.** Es el epílogo: se ve una sola vez, al
final, y no entra en el recorrido.

El código está en [`../outro.c`](../outro.c). La imagen es
[`res/demo/fin.bmp`](../../../res/demo/fin.bmp).

---

## Para qué sirve, que no es decoración

Sin esto la intro se apaga y el juego arranca **en el mismo instante**. Delante
de gente eso es un salto: no da tiempo a decir *«bueno, pues esto es el juego»*
ni a que nadie se prepare.

El epílogo convierte ese corte en una parada. La intro acaba, aparece el cartel,
y el juego empieza cuando tú quieres.

```
   antes:   ...ultima foto  ->  [fundido]  ->  el juego, de golpe
   ahora:   ...ultima foto  ->  [fundido]  ->  cartel  ->  tecla  ->  el juego
```

## Por qué no está en `demo_effects[]`

Porque no termina sola. Los veinte efectos reciben un `end_tick` y se paran
solos; este espera a que alguien pulse una tecla, y metido en la tabla colgaría
la presentación en la foto que le tocara.

Por eso tiene **otra firma** — sin `end_tick` — y lo llama `demo_run()` a mano,
al final y una sola vez:

```c
	if (finished == 1){
		if (demo_load_image(DEMO_ENDING_IMAGE, image, palette) == 1){
			...
			demo_outro(image, screen, palette);
			...
		}
	}
```

**Solo si la presentación llegó al final.** Si saliste con ESC es porque quieres
jugar *ya*, y plantarte entonces un cartel que pide otra tecla sería justo lo
contrario de lo que has pedido.

Y reutiliza **los mismos tres bloques de memoria** que ya estaban pedidos: ni un
byte más que los 128.768 de siempre.

## Por qué el zoom es tan corto

El resto de la carpeta deforma la imagen a base de bien, que es de lo que va.
Aquí no se puede: esto es un cartel, y un cartel hay que poder **leerlo**. Un
rotozoom sobre un texto lo hace ilegible en dos frames.

Así que se coge el [zoom](../../zoom/doc/README.md), que es la única deformación
que **no retuerce nada** — todo crece igual en las dos direcciones, las letras
siguen siendo letras — y se le recorta el recorrido a casi nada:

```
   zoom normal:   de 0,6 a 2,2 aumentos
   epilogo:       de 1,00 a 1,09
```

Lo justo para que la pantalla no parezca congelada.

## Y solo hacia dentro, nunca hacia fuera

La escala nunca pasa de 256, que es el 1:1. Si se alejara, la imagen ocuparía
menos de 320 píxeles y aparecería **un marco negro pulsando** alrededor del
cartel, que es justo el tipo de cosa que distrae de lo que pone.

Acercándose siempre, la pantalla está llena siempre, no hay bordes que rellenar
y el bucle interior se queda sin un solo `memset`. El zoom normal se gasta media
función en eso.

## La zona segura de la imagen

A 1,09 aumentos solo se ve el **91% central** de la imagen. Todo lo que tenga que
leerse tiene que caber ahí dentro:

```
   320 x 200 completos
   +--------------------------------+
   |   lo que se ve a 1,09 aumentos |
   |   +------------------------+   |   x de 14 a 306
   |   |  zona segura del texto |   |   y de  8 a 192
   |   |  x 32..288  y 28..172  |   |
   |   +------------------------+   |
   +--------------------------------+
```

`fin.bmp` está dibujado contra esos números: el marco cae en x 24..295 e
y 20..179, dentro de lo visible, y el texto bastante más adentro. **Si cambias
la imagen, respeta el rectángulo interior** o el zoom se comerá las letras.

Y si subes `OUTRO_SCALE_AMP`, la cuenta cambia. El recorte de la tabla de
columnas evita que se lea fuera del buffer, pero no evita que el texto se salga
de la pantalla.

## Cambiar el cartel

Cualquier BMP de **320x200, 256 colores y 65.078 bytes exactos**, llamado
`fin.bmp` y puesto en `res/demo/`. El índice 0 tiene que ser negro, como en toda
la carpeta.

Si el fichero no está, no pasa nada: `demo_run()` se lo salta y el juego arranca
como arrancaba antes.

## Cualquier tecla, y también la del otro

`demo_any_key_pressed()` vale por cualquiera, no solo por el ESC. Y mira
`demo_aborted` por lo mismo que `demo_escape_pressed()`: en red, la máquina que
está **esperando** a la que pone la intro solo tiene el ESC para decir *«tira
ya»*, y ese ESC llega hasta aquí por el callback de `demo_sound()`.

Esa es también la razón de que la espera pueda durar lo que haga falta sin
tirar la partida: `demo_show()` llama a `demo_sound()` cada frame, y ahí dentro
está el latido de red.

---

**Índice:** [Los diecinueve efectos](../../README.md)
