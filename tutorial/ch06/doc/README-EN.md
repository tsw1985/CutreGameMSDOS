# Chapter 6 — Animation: making the tracks turn

*[Versión en español](README.md)*

**What you will get:** the chapter 5 tank, but with its tracks turning while it
moves and still when it does not.

**Which real code is used:** `src/bmp.c`, `src/players.c`, `header/players.h`.

```
make
chap06
```

---

## 1. An animation is two drawings and a counter

Nothing more. In `sprites.bmp` each tank direction has **two** versions, with the
tracks in different positions:

```
   +-----------------+-----------------+
   |  up, frame 0    |  up, frame 1    |
   |     (2, 5)      |     (23, 5)     |
   +-----------------+-----------------+
```

Alternating between them, the eye sees a turning track. Eight sprites in total:
four directions times two frames.

## 2. Why you cannot change every frame

Here is the lesson.

The loop runs at **70 times a second** (set by the retrace, chapter 3). If you
changed drawing on every turn, the tracks would turn 70 times a second.

That does not read as movement: it reads as a **blur**. Too fast for the eye to
separate the frames.

The answer is to count:

```c
	tank.speed_counter = tank.speed_counter + 1;

	if (tank.speed_counter >= tank.speed_total){

		tank.speed_counter = 0;

		tank.current_frame = tank.current_frame + 1;

		if (tank.current_frame >= tank.total_frames){
			tank.current_frame = 0;
		}

	}
```

With `speed_total = 2`, which is what the game uses, the drawing changes once
every 2 frames: **35 changes per second**. That sounds like a lot, but it works
because the two track frames look alike: what you see is the movement, not the
change.

| `speed_total` | Changes per second | How it looks |
|---|---|---|
| 1 | 70 | Too much: it is lost |
| **2** | **35** | **What the game uses** |
| 5 | 14 | You can tell the two drawings apart |
| 15 | 4.6 | Slow motion |

### And watch out: you have to set them by hand

```c
	tank.total_frames = 2;
	tank.speed_total  = 2;
```

**`player_init()` does NOT set them.** In the game, `main.c` sets them itself
(lines 1684-1685), and here you have to do the same.

If they stay at 0 — which is how they are born, being a global — the counter does
this in a single pass:

```
	speed_counter = 1      and  1 >= 0  is true  ->  change drawing
	current_frame = 1      and  1 >= 0  is true  ->  back to 0
```

Frame 1 is **set and unset in the same pass**, so it never gets drawn. The tracks
freeze on frame 0 for good, and there is no error anywhere: the tank just slides
around like a sticker.

It is a very common bug with fields that *look* initialised and are not.

## 3. It only turns if the tank moves

The second detail, and it is what makes it look right:

```c
		if (is_moving == 1){
			update_animation();
		}
```

A stopped tank has still tracks. It sounds obvious, but call `update_animation()`
unconditionally and a stopped tank keeps its tracks turning as if it were
skidding in place. It looks awful.

Chapter 7 refines this further: the tracks will only turn if the tank
**actually moved**, not if it tried and hit a wall.

## 4. `struct player` appears

This is the first chapter that uses the game's real structure, from
`header/players.h`:

```c
struct player tank;
```

Inside is everything a tank owns: where it is, which way it faces, its sprite
pointers, its bullet, its explosion and its score.

And two functions from `src/players.c`:

| | |
|---|---|
| `player_init(&tank)` | Reserves the sprite buffers |
| `player_reset(&tank, x, y, dir)` | Places the tank to start a round |

That it is a `struct` and not a pile of loose variables is what will let chapter
8 have **two** tanks without duplicating a single line.

## 5. Picking the drawing is two questions

```c
	if (tank.current_direction == MOVE_UP){
		if (tank.current_frame == 0){ return tank.sprite_tank_up; }
		return tank.sprite_tank_up_2;
	}
```

First which way it faces, then which frame is due. Two independent pieces of
data: `current_direction` is changed by the keyboard, `current_frame` by the
counter.

## 6. Experiments

1. **Set `speed_total` to 1.** Watch the blur.
2. **Set it to 20.** Slow motion.
3. **Call `update_animation()` unconditionally**, outside the `if`. The stopped
   tank skids.
4. **Remove the `if (tank.current_frame >= tank.total_frames)`.** The counter
   grows forever and `pick_sprite()` always returns frame 1: the animation
   freezes on the second pose.
5. **Set both frames of a direction to the same sprite coordinates.** The tank
   moves but the tracks do not. That shows how much the animation adds.

## 7. What to take away

| | |
|---|---|
| An animation = N drawings + a counter | Nothing else needed |
| **Do not change drawing every frame** | 70 a second is a blur |
| The counter separates game speed from animation speed | A reusable pattern |
| `total_frames` and `speed_total` are **not** set by `player_init()` | Forget them and the tracks freeze |
| The animation only advances if there was movement | Otherwise it skids |
| `struct player` holds everything about a tank | Which makes two tanks free |

---

**Previous:** [Chapter 5](../../ch05/doc/README-EN.md) ·
**Next:** [Chapter 7 — Collisions](../../ch07/doc/README-EN.md)
