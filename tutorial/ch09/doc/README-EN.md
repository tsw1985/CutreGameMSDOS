# Chapter 9 — Bullets

*[Versión en español](README.md)*

**What you will get:** firing. One bullet per tank, flying straight and hitting
things.

```
make
chap09
```

Player 1: arrows + **numpad 5**. Player 2: W A S D + **G**.

---

## 1. A bullet is not a new object

The first thing anyone thinks is "I need a list of bullets". Not here:

```c
	unsigned int bullet_position_x;
	unsigned int bullet_position_y;
	unsigned int bullet_direction;
	unsigned int bullet_is_flying;
```

Four fields **inside the tank**. Because the game has a rule: **one bullet in the
air per tank**. With that rule there is no list, no allocation, nothing to free.

Choosing your game rules well saves you code. If the game allowed ten bullets,
you would need an array and a slot manager.

## 2. The direction is frozen on firing

```c
	_player->bullet_direction = _player->current_direction;
```

At the moment of firing, the direction is **copied** from the tank to the bullet.
From then on the bullet has its own and the tank can turn freely.

Try it: fire and immediately turn. The bullet carries straight on.

If the bullet read `current_direction` each frame, it would turn with you. That
would be a guided missile, not a bullet.

## 3. The edge detector

This is the lesson of the chapter, and the pattern is used everywhere.

The fire bit stays set **the whole time** you hold the key. If you fired on the
bit's value alone, a new bullet would come out the moment the last one died, and
another, and another: **a machine gun**.

You have to detect the **edge**: the exact frame the bit goes from 0 to 1.

```c
	if (input_bits & 0x10){

		if (p->fire_was_pressed == 0){    /* was released -> this is an edge */
			player_fire_bullet(p);
		}

		p->fire_was_pressed = 1;

	}else{

		p->fire_was_pressed = 0;

	}
```

To know whether it is an edge you have to remember how it was last frame, and
that is what `fire_was_pressed` inside the `struct` is for.

**This pattern works for any "once per press" action:** opening a door, switching
weapon, pausing the game, chapter 7's TAB.

## 4. Outside the direction chain

The firing block sits **outside** the movement's `if / else if`, deliberately.
Firing is not a direction, and the tank has to be able to move and fire in the
same frame.

## 5. A bullet's collision: one point

```c
	bmp_is_wall(p->bullet_position_x + BULLET_CENTER_X,
	            p->bullet_position_y + BULLET_CENTER_Y)
```

The tank needed **three** points because it is big and has a wide leading edge.
The bullet is 4x3: its centre is enough.

⚠️ **A dangerous consequence:** the bullet moves `BULLET_PIXEL_TO_MOVE` = **3
pixels** per frame and is checked at **one single point**. A wall less than 3
pixels thick **gets skipped**.

That is why the map rule is even stricter for bullets than for tanks. 8 pixels
minimum.

## 6. The order inside `update_bullet()`

```c
	player_move_bullet(p);

	if (p->bullet_is_flying == 0){   /* it may have been killed for leaving */
		return;
	}

	if (bmp_is_wall(...)){ ... }
```

That check in the middle is not paranoia: `player_move_bullet()` kills the bullet
if it leaves the map. If you read the map afterwards without checking, you would
be asking about a coordinate that is no longer valid.

**The map is only read while the bullet is alive.**

## 7. The loaded bullet exists but is not drawn

```c
	if (p->bullet_is_flying == 0){
		player_update_bullet_position(p);   /* stays stuck to the cannon */
		return;
	}
```

Unfired, the bullet follows the tank glued to the cannon tip. It exists, it has
coordinates, and **it is not drawn**: if it were, the tank would carry a visible
bullet on its nose all the time.

That makes it leave exactly from the cannon's mouth when fired, with no frame of
lag.

## 8. Experiments

1. **Remove the edge detector** (fire on `if (input_bits & 0x10)` alone). Machine
   gun.
2. **Draw the loaded bullet** by removing the `if (bullet_is_flying == 1)` from
   the painting. You will see the bullet riding on the nose.
3. **Put `bullet_direction = current_direction` inside `player_move_bullet`** (in
   `players.c`, then undo it). Guided missile.
4. **Raise `BULLET_PIXEL_TO_MOVE` to 20** in `players.h`. Bullets go through
   walls: the problem from point 5, live.

## 9. What to take away

| | |
|---|---|
| The bullet is state inside the tank | A well-chosen rule saves code |
| **The direction is frozen on firing** | Otherwise it is a missile |
| **Edge detector** | Without it, a machine gun. Reusable pattern |
| One point is enough for a bullet | But walls must be thick |

---

**Previous:** [Chapter 8](../../ch08/doc/README-EN.md) ·
**Next:** [Chapter 10 — Hit and round](../../ch10/doc/README-EN.md)
