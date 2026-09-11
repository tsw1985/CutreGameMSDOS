# Chapter 7 — Collisions against the map

*[Versión en español](README.md)*

**What you will get:** a tank that does not walk through walls. And, by pressing
TAB, **the map the game sees** instead of the one you see.

**Which real code is used:** `src/bmp.c` (`bmp_is_wall`), `src/players.c`
(`player_update_future_collision_points`).

```
make
chap07
```

Press **TAB** inside the game. Half the lesson is there.

---

## 1. Two maps, not one

The game loads **two** different files:

| File | What it is | What for |
|---|---|---|
| `cutre.bmp` | The pretty picture: bricks, bushes | **Looking only** |
| `cutrecol.bmp` | Two colours: blue floor, yellow wall | **Deciding only** |

Having two looks wasteful. It is not.

## 2. Why the picture cannot be used to decide

Here is the concept of the chapter, and why the program lets you see both.

**The picture lies.** A brick wall has dark mortar lines between the bricks. Ask
"what colour is this pixel?" and land on a joint, and it answers *black*, and the
game concludes there is no wall there.

In the big map there are **22,806 pixels** that are black in the picture and are
wall.

And the other way round: the bushes are drawn but can be driven over. **13,047
pixels** that look solid and are not.

Press TAB and compare. The collision walls are **clean rectangles** that do not
follow the exact outline of the drawing. That is deliberate: the picture is for
the eye, the collision is for the rules.

There is a third source you must **never** use: reading video memory. By the time
you do, the tanks are already painted on top, and asking for the pixel at the
cannon tip gives you the tank's own colour.

## 3. Look before you leap

The second idea of the chapter, and the one to copy into any game you write.

There are two ways to handle a collision:

**The bad one:** move the tank, check whether it ended up inside a wall, and if
so put it back. That leaves an instant where the tank is **inside** the wall.
Anything that looks at the state at that moment sees something impossible: the
AI, the sound, the network code.

**The good one, which is what the game does:**

```c
static int try_move(int direction){

	tank.current_direction = direction;          /* 1. turning is free */

	if (is_blocked_by_wall(direction) == 1){     /* 2. ask */
		return 0;
	}

	/* 3. and only now, move */
	...
}
```

You work out **where it would be**, check that, and only apply it if it is clear.
The tank never ends up inside a wall, not even for a frame.

Note that turning happens **before** the check: changing direction can never
collide, and you want the tank to face where you are pushing even if it cannot
advance.

## 4. Three points, not 324

```c
	player_update_future_collision_points(&tank, direction);

	if (bmp_is_wall(tank.future_cannon_tip_x, tank.future_cannon_tip_y)) return 1;
	if (bmp_is_wall(tank.future_track1_x,     tank.future_track1_y))     return 1;
	if (bmp_is_wall(tank.future_track2_x,     tank.future_track2_y))     return 1;
```

The tank is 18x18 = 324 pixels. Why only three?

```
   Going UP:

        T           <- cannon tip
      O   O         <- the two tracks
      +-------+
      |       |     the rest of the tank follows behind,
      |       |     over ground it has already cleared
      +-------+
```

Because **the tank only moves forwards**. What can touch something first is its
leading edge, and that edge is three points: the cannon tip and the two track
corners.

Checking all 324 would cost 324 reads per tank per frame. On a 486 that shows.
And it would not change a single decision.

`player_update_future_collision_points()` knows which three points matter for
each direction and leaves them in the `future_*` fields of the structure.

## 5. A rule for drawing maps

This one the code cannot enforce, and it will bite you:

> **A wall has to be thicker than the tank's step.**

The tank moves `PIXEL_TO_MOVE` = **2 pixels** at a time. The coordinates it can
occupy are start, start+2, start+4… **never the ones in between**.

A wall 1 pixel thick can sit at a coordinate the tank never lands on, and **it
walks straight through it**.

The maps in this game have walls of 8 pixels or more. And on the big map the
outer border has 16 to 33.

## 6. The tracks, again

```c
		if (moved == 1){
			update_animation();
		}
```

`try_move()` returns whether the tank **actually advanced**. So pushing against a
wall leaves the tracks still instead of skidding.

It is a tiny detail that shows enormously while playing.

## 7. How the collision map is drawn

When you press TAB, the program paints the background by asking `bmp_is_wall()`
pixel by pixel:

```c
			for (py = 0; py < HEIGHT; py++){
				for (px = 0; px < WIDTH; px++){
					if (bmp_is_wall(px, py) == 1){
						buffer_background_image_data[offset] = 252;
					}else{
						buffer_background_image_data[offset] = 3;
					}
				}
			}
```

64,000 calls per frame. It is desperately slow and it does not matter: it is a
learning aid, not part of the game.

What is interesting is that it is **not reading `cutrecol.bmp`**. That file no
longer exists in memory: on loading,
`bmp_fill_background_collision_in_buffer()` turned it into a **one-bit-per-pixel
mask**. Chapter 23 explains why.

## 8. Experiments

1. **Press TAB and cross the whole map.** Seeing the world as the game sees it
   changes how you think about it.
2. **Comment out the call to `bmp_fill_background_collision_in_buffer()`.** With
   no mask, `bmp_is_wall()` returns 0 everywhere and you walk through walls.
3. **Invert the `if` in `is_blocked_by_wall()`** (`== 0` instead of `== 1`). Now
   you can only move *inside* the walls.
4. **Check only the cannon tip**, dropping the two tracks. You will see the tank
   push its corners into the walls when passing close.
5. **Raise `PIXEL_TO_MOVE`** to 10 in `header/players.h` and rebuild. You will
   start going through thin walls: the problem from point 5, live.

## 9. What to take away

| | |
|---|---|
| **Two maps: one to look at, one to decide with** | The picture lies |
| **Look before you leap** | Work out where you would be, check, then move |
| Three points, not the rectangle | Only the leading edge can touch first |
| Walls thicker than the step | Otherwise they get walked through |
| The animation only advances if you advanced | Pushing a wall does not turn the tracks |

---

**Previous:** [Chapter 6](../../ch06/doc/README-EN.md) ·
**Next:** [Chapter 8 — Two players](../../ch08/doc/README-EN.md)
