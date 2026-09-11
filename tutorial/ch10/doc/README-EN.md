# Chapter 10 — Hit, explosion and round

*[Versión en español](README.md)*

**What you will get:** the local game finished. Hits, explosion, score and
rounds.

```
make
chap10
```

---

## 1. Detecting the hit: a point against a box

```c
	if (bullet_x < other->position_x){ return 0; }
	if (bullet_x > other->position_x + TANK_WIDTH - 1){ return 0; }
	if (bullet_y < other->position_y){ return 0; }
	if (bullet_y > other->position_y + TANK_HEIGHT - 1){ return 0; }

	return 1;
```

Four comparisons. If the bullet's centre is inside the other tank's rectangle, it
hit.

It is the cheapest collision test there is. For this game it is plenty: no real
geometry needed, no shape checking.

## 2. A two-state machine

```c
unsigned int explosion_pause_counter;
```

One variable, and it is already a state machine:

| Value | State | What happens |
|---|---|---|
| 0 | Normal round | Keyboard, movement, bullets |
| >0 | Burning | **Everything frozen** but the explosion animation |

```c
		if (explosion_pause_counter == 0){
			/* ---- NORMAL STATE ---- */
		}else{
			/* ---- BURNING STATE ---- */
		}
```

Nothing more elaborate is needed. Most games of this era worked exactly like
this.

## 3. Why freeze rather than restart at once

On a hit you could restart the round immediately. It would be easier and it would
look terrible: the tanks would jump back to their corners before you had time to
see what happened.

The 40-frame pause (half a second) gives the player time to **understand the
result**. That is design, not technique.

And note a detail: **the tanks are not reset on the hit.** They stay where they
were shot, so the explosion can be drawn over them. The `restart_round()` happens
at the end of the pause.

## 4. Both hits in the same frame have to count

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

**Both** bullets are checked, and only afterwards do we look at whether there was
a hit.

If you bailed out as soon as one connected, a simultaneous double hit would count
as one. This way both die together and both score, which is fair.

That pattern — **gather every event, then react** — avoids a lot of odd cases.

## 5. Putting out bullets in flight

```c
				player1.bullet_is_flying = 0;
				player2.bullet_is_flying = 0;
```

Without this, a bullet that was in the air when somebody died would stay
**frozen in mid-screen** for the whole pause, because the burning state does not
move bullets.

Details like this are what separate a game that works from one that looks right.

## 6. The explosion is 13x13, not 18x18

```c
		draw_sprite_to_buffer(boom, EXPLOSION_WIDTH, EXPLOSION_HEIGHT,
		                      p->position_x + EXPLOSION_OFFSET_X,
		                      p->position_y + EXPLOSION_OFFSET_Y, ...);
```

Not every sprite is tank-sized. The explosion is smaller, so it has to be pushed
in 2 pixels on each side to sit centred in the box the tank occupied. Hence
`EXPLOSION_OFFSET_*` in `players.h`.

And cutting it with `TANK_WIDTH` would drag in the neighbouring cell of the
sheet.

## 7. The score survives the round

`player_reset()` **does not touch `wins`**. That is why the score accumulates
across rounds. It only resets what belongs to one round: position, direction,
bullet.

That separation between "round state" and "match state" has to be made
deliberately.

## 8. Experiments

1. **Set the pause to 1 frame.** The tanks jump and you see nothing.
2. **Set it to 300.** Half an eternity.
3. **Remove the bullet cut-off.** Fire, die, and watch the frozen bullet.
4. **Bail out after the first hit** (put an `else` between the two
   `if (update_bullet(...))`). Double hits stop counting double.
5. **Restart the round immediately** instead of freezing. Watch the mess.

## 9. What to take away

| | |
|---|---|
| Point against box: 4 comparisons | The cheapest collision there is |
| **One variable is already a state machine** | Frozen / normal |
| Freeze before restarting | So the player understands what happened |
| **Gather every event, then react** | Double hits count |
| The score is not round state | Keep the two scopes apart |

---

**Previous:** [Chapter 9](../../ch09/doc/README-EN.md) ·
**Next:** [Chapter 11 — The Sound Blaster](../../ch11/doc/README-EN.md)
