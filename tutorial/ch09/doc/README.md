# Capítulo 9 — Balas

*[English version](README-EN.md)*

**Qué vas a conseguir:** disparar. Una bala por tanque, que vuela recta y choca.

```
make
chap09
```

Jugador 1: flechas + **5 del teclado numérico**. Jugador 2: W A S D + **G**.

---

## 1. Una bala no es un objeto nuevo

Lo primero que uno piensa es "necesito una lista de balas". Aquí no:

```c
	unsigned int bullet_position_x;
	unsigned int bullet_position_y;
	unsigned int bullet_direction;
	unsigned int bullet_is_flying;
```

Cuatro campos **dentro del tanque**. Porque el juego tiene una regla:
**una bala en el aire por tanque**. Con esa regla no hace falta lista, ni
reservar memoria, ni liberar nada.

Elegir bien las reglas del juego te ahorra código. Si el juego permitiera diez
balas, harían falta un array y un gestor de huecos.

## 2. La dirección se congela al disparar

```c
	_player->bullet_direction = _player->current_direction;
```

En el momento del disparo, la dirección se **copia** del tanque a la bala. A
partir de ahí la bala tiene la suya propia y el tanque puede girar libremente.

Pruébalo: dispara y gira inmediatamente. La bala sigue recta.

Si la bala leyera `current_direction` cada frame, giraría contigo. Sería un
misil teledirigido, no una bala.

## 3. El detector de flanco

Esta es la lección del capítulo, y el patrón se usa en todas partes.

El bit de disparo está puesto **todo el rato** que mantienes la tecla. Si
dispararas con el valor del bit a secas, en cuanto la bala muriera saldría otra
sola, y otra: **una ametralladora**.

Hay que detectar el **flanco**: el frame exacto en que el bit pasa de 0 a 1.

```c
	if (input_bits & 0x10){

		if (p->fire_was_pressed == 0){    /* estaba suelto -> es un flanco */
			player_fire_bullet(p);
		}

		p->fire_was_pressed = 1;

	}else{

		p->fire_was_pressed = 0;

	}
```

Para saber si es un flanco hay que recordar cómo estaba el frame anterior, y
para eso está `fire_was_pressed` dentro de la `struct`.

**Este patrón vale para cualquier acción de "una vez por pulsación":** abrir una
puerta, cambiar de arma, pausar el juego, el TAB del capítulo 7.

## 4. Fuera de la cadena de direcciones

El bloque de disparo va **fuera** del `if / else if` del movimiento, a
propósito. Disparar no es una dirección, y el tanque tiene que poder moverse y
disparar en el mismo frame.

## 5. La colisión de una bala: un punto

```c
	bmp_is_wall(p->bullet_position_x + BULLET_CENTER_X,
	            p->bullet_position_y + BULLET_CENTER_Y)
```

El tanque necesitaba **tres** puntos porque es grande y tiene un frente ancho.
La bala mide 4x3: su centro basta.

⚠️ **Consecuencia peligrosa:** la bala avanza `BULLET_PIXEL_TO_MOVE` = **3
píxeles** por frame y se comprueba en **un solo punto**. Un muro de menos de 3
píxeles de grosor **se lo salta**.

Por eso la regla de los mapas es todavía más estricta para las balas que para
los tanques. 8 píxeles como mínimo.

## 6. El orden dentro de `update_bullet()`

```c
	player_move_bullet(p);

	if (p->bullet_is_flying == 0){   /* puede haberla matado por salirse */
		return;
	}

	if (bmp_is_wall(...)){ ... }
```

Esa comprobación en medio no es paranoia: `player_move_bullet()` mata la bala
si se sale del mapa. Si leyeras el mapa después sin comprobarlo, estarías
preguntando por una coordenada que ya no es válida.

**Solo se lee el mapa mientras la bala está viva.**

## 7. La bala cargada existe pero no se ve

```c
	if (p->bullet_is_flying == 0){
		player_update_bullet_position(p);   /* se queda pegada al cañón */
		return;
	}
```

Sin disparar, la bala sigue al tanque pegada a la punta del cañón. Existe, tiene
coordenadas, y **no se dibuja**: si se dibujara, el tanque pasearía una bala
visible en el morro todo el rato.

Eso hace que al disparar salga exactamente de la boca del cañón, sin un frame
de retraso.

## 8. Experimentos

1. **Quita el detector de flanco** (dispara con `if (input_bits & 0x10)` a
   secas). Ametralladora.
2. **Dibuja la bala cargada** quitando el `if (bullet_is_flying == 1)` del
   pintado. Verás la bala paseando en el morro.
3. **Pon `bullet_direction = current_direction` dentro de `player_move_bullet`**
   (en `players.c`, y luego deshazlo). Misil teledirigido.
4. **Sube `BULLET_PIXEL_TO_MOVE` a 20** en `players.h`. Las balas atraviesan
   muros: el problema del punto 5, en vivo.

## 9. Lo que hay que llevarse

| | |
|---|---|
| La bala es estado dentro del tanque | Una regla bien elegida ahorra código |
| **La dirección se congela al disparar** | Si no, es un misil |
| **Detector de flanco** | Sin él, ametralladora. Patrón reutilizable |
| Un punto basta para la bala | Pero los muros tienen que ser gruesos |

---

**Anterior:** [Capítulo 8](../../ch08/doc/README.md) ·
**Siguiente:** [Capítulo 10 — Impacto y ronda](../../ch10/doc/README.md)
