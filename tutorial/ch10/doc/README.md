# Capítulo 10 — Impacto, explosión y ronda

*[English version](README-EN.md)*

**Qué vas a conseguir:** el juego local terminado. Impactos, explosión, marcador
y rondas.

```
make
chap10
```

---

## 1. Detectar el impacto: un punto contra una caja

```c
	if (bullet_x < other->position_x){ return 0; }
	if (bullet_x > other->position_x + TANK_WIDTH - 1){ return 0; }
	if (bullet_y < other->position_y){ return 0; }
	if (bullet_y > other->position_y + TANK_HEIGHT - 1){ return 0; }

	return 1;
```

Cuatro comparaciones. Si el centro de la bala está dentro del rectángulo del
otro tanque, ha dado.

Es la prueba de colisión más barata que existe. Para este juego sobra: no hace
falta geometría de verdad ni comprobar formas.

## 2. Una máquina de estados de dos estados

```c
unsigned int explosion_pause_counter;
```

Una variable, y ya es una máquina de estados:

| Valor | Estado | Qué pasa |
|---|---|---|
| 0 | Ronda normal | Teclado, movimiento, balas |
| >0 | Ardiendo | **Todo congelado** menos la animación de la explosión |

```c
		if (explosion_pause_counter == 0){
			/* ---- ESTADO NORMAL ---- */
		}else{
			/* ---- ESTADO ARDIENDO ---- */
		}
```

No hace falta nada más elaborado. La mayoría de los juegos de esta época
funcionaban así.

## 3. Por qué congelar y no reiniciar de golpe

Al recibir un impacto podrías reiniciar la ronda inmediatamente. Sería más
fácil y quedaría fatal: los tanques saltarían a sus esquinas sin que te diera
tiempo a ver qué ha pasado.

La pausa de 40 frames (medio segundo) le da al jugador tiempo de **entender el
resultado**. Es diseño, no técnica.

Y fíjate en un detalle: **los tanques no se reinician al recibir el impacto**.
Se quedan donde les dieron, para poder dibujar la explosión encima. El
`restart_round()` ocurre al final de la pausa.

## 4. Los dos impactos del mismo frame tienen que contar

```c
			if (update_bullet(&player1, &player2) == 1){
				player_start_explosion(&player2);
				tank_was_hit = 1;
			}

			if (update_bullet(&player2, &player1) == 1){
				player_start_explosion(&player1);
				tank_was_hit = 1;
			}

			if (tank_was_hit == 1){ ... }
```

Las **dos** balas se comprueban, y solo después se mira si hubo impacto.

Si salieras del bloque en cuanto una acierta, un doble impacto simultáneo
contaría solo uno. Así los dos se matan a la vez y los dos suman punto, que es
lo justo.

Ese patrón — **recoger todos los eventos y luego reaccionar** — evita un montón
de casos raros.

## 5. Apagar las balas en vuelo

```c
				player1.bullet_is_flying = 0;
				player2.bullet_is_flying = 0;
```

Sin esto, una bala que estuviera en el aire cuando alguien muere se quedaría
**congelada en mitad de la pantalla** durante toda la pausa, porque el estado
"ardiendo" no mueve balas.

Detalles como este son los que separan un juego que funciona de uno que se ve
bien.

## 6. La explosión mide 13x13, no 18x18

```c
		draw_sprite_to_buffer(boom, EXPLOSION_WIDTH, EXPLOSION_HEIGHT,
		                      p->position_x + EXPLOSION_OFFSET_X,
		                      p->position_y + EXPLOSION_OFFSET_Y, ...);
```

No todos los sprites son del tamaño del tanque. La explosión es más pequeña,
así que hay que meterla 2 píxeles por cada lado para que quede centrada en el
hueco que ocupaba el tanque. De ahí los `EXPLOSION_OFFSET_*` de `players.h`.

Y recortarla con `TANK_WIDTH` se llevaría el trozo de hoja de al lado.

## 7. El marcador sobrevive a la ronda

`player_reset()` **no toca `wins`**. Por eso el marcador se acumula entre
rondas. Lo único que reinicia es lo que pertenece a una ronda: posición,
dirección, bala.

Esa separación entre "estado de la ronda" y "estado de la partida" hay que
hacerla a conciencia.

## 8. Experimentos

1. **Pon la pausa a 1 frame.** Los tanques saltan y no te enteras de nada.
2. **Pon la pausa a 300.** Media eternidad.
3. **Quita el apagado de balas.** Dispara, muere, y mira la bala congelada.
4. **Sal del bloque tras el primer impacto** (pon un `else` entre los dos
   `if (update_bullet(...))`). Los dobles impactos dejan de contar doble.
5. **Reinicia la ronda inmediatamente** en vez de congelar. Se ve el desastre.

## 9. Lo que hay que llevarse

| | |
|---|---|
| Punto contra caja: 4 comparaciones | La colisión más barata que hay |
| **Una variable ya es una máquina de estados** | Congelado / normal |
| Congelar antes de reiniciar | Para que el jugador entienda qué pasó |
| **Recoger todos los eventos, luego reaccionar** | Los dobles impactos cuentan |
| El marcador no es estado de ronda | Separar los dos ámbitos |

---

**Anterior:** [Capítulo 9](../../ch09/doc/README.md) ·
**Siguiente:** [Capítulo 11 — La Sound Blaster](../../ch11/doc/README.md)
