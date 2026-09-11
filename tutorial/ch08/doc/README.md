# Capítulo 8 — Dos jugadores, y la idea que sostiene el curso

**Qué vas a conseguir:** dos tanques moviéndose a la vez en la misma pantalla.

**Y sobre todo:** la abstracción que hará que el capítulo 18 meta la red **sin
tocar ni una línea** de movimiento, colisiones o dibujado.

```
make
chap08
```

Jugador 1: flechas. Jugador 2: W A S D.

---

## 1. A partir de aquí, `tutlib.h`

Lo que ya aprendiste (recortar sprites, el teclado, la animación, mirar antes de
saltar) se ha movido a `tutorial/tutlib.h`, **una sola copia para todo el
curso**, para que cada capítulo contenga solo la idea nueva.

Está todo comentado y con el número de capítulo donde se explica cada cosa.
Ábrelo una vez y olvídate.

## 2. Dos tanques salen casi gratis

```c
	player_init(&player1);
	player_init(&player2);
	...
	process_player_input(&player1, input1);
	process_player_input(&player2, input2);
```

Las mismas llamadas, una por jugador. **Eso es lo que se ganó en el capítulo 6**
metiendo todo el estado de un tanque en una `struct`.

Si la posición, la dirección, la bala y el resto fueran variables sueltas
(`tank_x`, `tank_y`, `tank_dir`…), cada jugador nuevo sería otra tanda de
variables **y una copia de todas las funciones**. Con la struct, es un parámetro.

El `0` y el `21` de `tut_load_tank_sprites()` son la fila de la hoja de sprites:
el tanque azul está arriba y el rojo justo debajo.

## 3. LA IDEA GRANDE DEL CURSO

Mira estas dos funciones, y fíjate sobre todo en lo que **no** hacen:

```c
static unsigned char read_input_from_keys(...){

	bits = 0;
	if (keys[key_up]){    bits = bits | 0x01; }
	if (keys[key_down]){  bits = bits | 0x02; }
	...
	return bits;

}


static void process_player_input(struct player *p, unsigned char input_bits){

	if (input_bits & 0x01){ ... }
	else if (input_bits & 0x02){ ... }

}
```

La primera lee el teclado y devuelve **un byte con cinco bits**.

La segunda recibe ese byte y **no menciona el teclado por ningún lado**. No hay
un solo `keys[]` dentro. No sabe que existe un teclado.

| bit | valor | significa |
|---|---|---|
| 0 | `0x01` | arriba |
| 1 | `0x02` | abajo |
| 2 | `0x04` | izquierda |
| 3 | `0x08` | derecha |
| 4 | `0x10` | disparar |

### Por qué no leer `keys[]` directamente

Sería más corto. Podrías escribir `if (keys[KEY_UP])` dentro del movimiento y
ahorrarte una función y un parámetro.

**Y sería la decisión que te impediría meter red más adelante.**

Ese byte es la **frontera** entre *"de dónde vienen las órdenes"* y *"qué se
hace con ellas"*. Todo lo que está por debajo de la frontera — movimiento,
colisiones, animación, disparo, dibujado — deja de saber de dónde salió nada.

## 4. La prueba: el diff con el capítulo 18

No te pido que me creas. El capítulo 18 es el juego en red, y esta es la
**diferencia real** de `process_player_input()` entre los dos:

```
$ diff ch08/chap08.c ch18/chap18.c   (solo esa función)

<	if (input_bits & 0x01){            >	if (input_bits & NET_INPUT_UP){
<	}else if (input_bits & 0x02){      >	}else if (input_bits & NET_INPUT_DOWN){
<	}else if (input_bits & 0x04){      >	}else if (input_bits & NET_INPUT_LEFT){
<	}else if (input_bits & 0x08){      >	}else if (input_bits & NET_INPUT_RIGHT){
```

**Cuatro líneas, y son puramente cosméticas.** `NET_INPUT_UP` está definido en
`header/lockstep.h` como… `0x01`:

```c
#define NET_INPUT_UP		0x01
#define NET_INPUT_DOWN		0x02
#define NET_INPUT_LEFT		0x04
#define NET_INPUT_RIGHT		0x08
#define NET_INPUT_FIRE		0x10
```

**Los mismos valores.** El capítulo 18 usa los nombres en vez de los números
porque son los bits que viajan por el cable, y así se lee mejor. Pero
funcionalmente `process_player_input()` es **byte a byte la misma función**.

Y `tut_try_move()`, `tut_update_animation()`, `tut_pick_sprite()`,
`bmp_is_wall()`, el dibujado: **idénticos**. Cero cambios.

Lo único que cambia en el ch18 es de dónde sale `input_bits`:

```c
	/* capitulo 8 */
	input1 = read_input_from_keys(KEY_UP, KEY_DOWN, ...);

	/* capitulo 18 */
	player1_input = net_get_remote_input();     /* viene del cable */
```

Compruébalo tú cuando llegues allí:

```
diff tutorial/ch08/chap08.c tutorial/ch18/chap18.c
```

## 5. Y esto no es un truco de este juego

Es el patrón general, y tiene nombre: **separar la fuente de entrada de la
lógica**.

Con esa frontera puesta, los bits pueden venir de:

| | |
|---|---|
| El teclado | Capítulo 8 |
| **Un cable de red** | Capítulo 18 |
| Un fichero grabado | Repeticiones, *replays* |
| Una IA | Un jugador controlado por el ordenador |
| Un mando | Un dispositivo nuevo |

**Y ninguna de esas cosas obliga a tocar el juego.** Las repeticiones son
especialmente bonitas: guarda el byte de cada frame en un fichero y ya tienes
una grabación de la partida completa, porque el juego es determinista
(capítulo 18).

> Si haces un juego y crees que algún día querrás meterle red, repeticiones o
> IA, **haz esta separación desde el principio**. Es lo único que hay que
> prever, y luego es gratis.

## 6. Por qué cada jugador necesita su propia cadena

```c
	if (input_bits & 0x01){ ... }
	else if (input_bits & 0x02){ ... }
```

El `if / else if` es lo que impide las diagonales: **solo entra una dirección
por frame**, la primera que encuentra.

Y por eso **cada jugador llama a la función por separado**. Si compartieran una
sola cadena, el `else` del jugador 1 se comería el turno del jugador 2 y solo se
movería uno de los dos por frame.

Es un fallo sutil, porque el juego *casi* funciona: parece que el segundo
jugador va a tirones.

## 7. Lo de las teclas simultáneas ya estaba resuelto

Que los dos jugadores se muevan a la vez no necesita nada nuevo aquí. Lo
resolvió el manejador de teclado del capítulo 5, con el array de 128 casillas
donde cada tecla enciende la suya al pulsarse y la apaga al soltarse.

Aquí solo se recoge lo que ya estaba.

## 8. Por qué un byte de bits y no cinco variables

Podrías devolver una struct con cinco `int`. Funcionaría. Pero:

- **Un byte es lo que se va a mandar por la red.** En el capítulo 18 se envía
  literalmente ese byte, uno por frame y jugador. Con cinco enteros serían 10
  bytes en vez de 1.
- **Cabe en el paquete de golpe**, y el capítulo 18 manda ocho frames de
  entradas pasadas por redundancia: 8 bytes, no 80.
- Y **comprobar un bit es un AND**, que es lo más barato que hay.

Elegir el byte no fue por elegancia: fue porque diez capítulos después hay que
meterlo en un paquete.

## 9. Experimentos

1. **Une las dos llamadas** en una sola cadena `if / else if` compartida. El
   segundo jugador se mueve a tirones.
2. **Imprime `input1` al salir** en binario. Comprueba que mantener dos teclas
   enciende dos bits, aunque el tanque solo use uno.
3. **Quita los `else`.** Diagonales, y el sprite mirando a otro lado.
4. **Dale a los dos jugadores las mismas teclas.** Se mueven como un espejo, y
   demuestra que `process_player_input()` no distingue quién es quién.
5. **Escribe un `read_input_from_script()`** que devuelva una secuencia fija de
   bytes (arriba 30 frames, derecha 30…) y pásasela al jugador 2. Acabas de
   hacer una IA rudimentaria **sin tocar nada del juego**. Eso es la frontera
   funcionando.

## 10. Lo que hay que llevarse

| | |
|---|---|
| Dos jugadores = dos `struct` | Casi cero código extra |
| **Las teclas se convierten en un byte de bits** | Ahí está la frontera |
| Lo que viene después **no sabe que existe un teclado** | |
| **El diff con el ch18 son 4 líneas cosméticas** | Y `NET_INPUT_UP` vale `0x01` |
| Con esa frontera: red, repeticiones, IA, mandos | Todo gratis |
| Cada jugador, su propia cadena `if/else if` | O uno va a tirones |

---

**Anterior:** [Capítulo 7](../../ch07/doc/README.md) ·
**Siguiente:** [Capítulo 9 — Balas](../../ch09/doc/README.md)
