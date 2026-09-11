# Capítulo 8 — Dos jugadores

**Qué vas a conseguir:** dos tanques en la misma pantalla, moviéndose a la vez.

**Y sobre todo:** la abstracción que hará posible el capítulo 18 sin tocar nada.

```
make
chap08
```

Jugador 1: flechas. Jugador 2: W A S D.

---

## 1. A partir de aquí, `tutlib.h`

Lo que ya aprendiste (recortar sprites, el teclado, la animación, mirar antes
de saltar) se ha movido a `tutorial/tutlib.h`, **una sola copia para todo el
curso**, para que cada capítulo contenga solo la idea nueva.

Está todo comentado y con el número de capítulo donde se explica. Ábrelo.

## 2. Dos tanques salen casi gratis

```c
	player_init(&player1);
	player_init(&player2);
	...
	process_player_input(&player1, input1);
	process_player_input(&player2, input2);
```

Las mismas llamadas, una por jugador. **Eso es lo que se ganó en el capítulo 6**
metiendo todo el estado de un tanque en una `struct`: si la posición, la
dirección y el resto fueran variables sueltas, cada jugador nuevo sería otra
tanda de variables y otra copia de todas las funciones.

El `0` y el `21` de `tut_load_tank_sprites()` son la fila de la hoja: el tanque
azul está arriba y el rojo justo debajo.

## 3. LA IDEA GRANDE DEL CURSO

Mira estas dos funciones y fíjate en lo que **no** hacen:

```c
static unsigned char read_input_from_keys(...){
	if (keys[key_up]){    bits = bits | 0x01; }
	...
	return bits;
}

static void process_player_input(struct player *p, unsigned char input_bits){
	if (input_bits & 0x01){ ... }
}
```

La primera lee el teclado y devuelve **un byte con cinco bits**.
La segunda recibe ese byte y **no menciona el teclado por ningún lado**.

| bit | significa |
|---|---|
| 0x01 | arriba |
| 0x02 | abajo |
| 0x04 | izquierda |
| 0x08 | derecha |
| 0x10 | disparar |

Parece un rodeo innecesario. Podrías consultar `keys[]` directamente desde el
movimiento y ahorrarte una función.

**No lo hagas.** Ese byte es la frontera entre "de dónde vienen las órdenes" y
"qué se hace con ellas". Y en el capítulo 18, los bits van a llegar **por un
cable de red** en vez de por el teclado, y `process_player_input()` no se va a
enterar. Ni una línea de movimiento, colisiones o dibujado tendrá que cambiar.

Cuando llegues al 18, vuelve a leer esto.

## 4. Por qué cada jugador necesita su propia cadena

```c
	if (input_bits & 0x01){ ... }
	else if (input_bits & 0x02){ ... }
```

Ese `if / else if` es lo que impide las diagonales: solo entra una dirección
por frame.

Y por eso **cada jugador llama a la función por separado**. Si compartieran la
cadena, el `else` del jugador 1 se comería el turno del jugador 2 y solo se
movería uno de los dos por frame.

## 5. Lo de las teclas a la vez ya estaba resuelto

Que los dos jugadores se muevan simultáneamente no necesita nada nuevo: lo
resolvió el manejador de teclado del capítulo 5, con el array de 128 casillas
donde cada tecla enciende la suya. Aquí solo se recoge.

## 6. Experimentos

1. **Cambia las teclas del jugador 2** a las del 1 en la segunda llamada. Los
   dos tanques se mueven igual, como un espejo.
2. **Une las dos llamadas** en una sola cadena `if/else if` compartida. Verás
   que solo uno se mueve por frame.
3. **Imprime `input1` en binario** al salir. Comprueba que mantener dos teclas
   enciende dos bits, aunque el tanque solo use uno.
4. **Quita los `else`.** Diagonales, y el sprite mirando a otro lado.

## 7. Lo que hay que llevarse

| | |
|---|---|
| Dos jugadores = dos `struct` | Casi cero código extra |
| **Las teclas se convierten en un byte de bits** | Esa es la frontera |
| Lo que viene después no sabe que existe un teclado | Y por eso la red será fácil |
| Cada jugador, su propia cadena `if/else if` | O solo se mueve uno |

---

**Anterior:** [Capítulo 7](../../ch07/doc/README.md) ·
**Siguiente:** [Capítulo 9 — Balas](../../ch09/doc/README.md)
