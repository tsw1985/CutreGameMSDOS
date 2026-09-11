# Chapter 22 — The camera: making the window move by itself

*[Versión en español](README.md)*

**What you will get:** chapter 21's window, but following the tank without you
touching it. And comparing the **three models** with one key.

```
make
chap22
```

Arrows: move **the tank** (no longer the window).
**`C` changes model:** 0 no camera, 1 dead zone, 2 always centred.

---

## 1. What you already have

From chapter 21 you already know:

- A window is **two numbers**: `camera_x`, `camera_y`
- `screen = world − camera`
- `bmp_draw_world_window()` copies the piece, with its 200 `memcpy`s and its
  *stride*
- The **clamp** keeps it inside the map

There you moved those two numbers **by hand**. This chapter is one single thing:

> **who moves those two numbers, and on what basis**

Nothing more. The window already worked.

## 2. The three models, with the `C` key

### Mode 0 — No camera

Chapter 20. It is here so you can compare at a glance.

### Mode 2 — Always centred

```c
	bmp_camera_snap((int)tank.position_x, (int)tank.position_y, TANK_WIDTH, TANK_HEIGHT);
```

The tank nailed to the centre and the world moving underneath. It is the first
thing anyone thinks of, and of the three it is the worst.

**Try it properly, not for five seconds.** The world moves on **every single
frame**, even when you take a 2-pixel step to line up a shot. And it takes away
the sense of moving your tank: you see it still and what moves is the ground.

### Mode 1 — Dead zone (the game's)

The camera **does not move** while the tank stays inside a rectangle in the
middle of the screen:

```
   THE SCREEN, 320x200

   +----------------------------------------+
   |                                        |
   |          <-- 70 px -->                 |
   |     +----------------------------+     |
   |     |                            |     |
   |     |         DEAD ZONE          |     |
   |     |   the camera does NOT      |     |
   |     |        move here           |     |
   |     |                            |     |
   |     +----------------------------+     |
   |          <-- 70 px -->                 |
   |                                        |
   +----------------------------------------+
    <- 100 px ->                <- 100 px ->
```

Measured over a 200-frame walk across the real map, **the camera is still for 88%
of the frames**. That is exactly what you want: the world only moves when you are
actually going somewhere.

## 3. Where the 100 and the 70 come from

```c
#define CAMERA_DEAD_ZONE_X 	100
#define CAMERA_DEAD_ZONE_Y 	 70
```

They are the **margins** from the screen edge to the edge of the dead zone. And
they are not the same number for a concrete reason: **the screen is not square.**

| | Screen | Maximum possible | Used | Resulting lane |
|---|---:|---:|---:|---:|
| **X** | 320 | (320−18)/2 = **151** | 100 | 320−100−100−18 = **102 px** |
| **Y** | 200 | (200−18)/2 = **91** | 70 | 200−70−70−18 = **42 px** |

There are 120 fewer pixels of screen to share out vertically, so the margin has to
be smaller or there would be no dead zone left.

### The limit you must not cross

Look at the "maximum possible" column. The margin has to be **less than half the
screen minus the tank**.

Go past it and the left edge of the dead zone ends up **to the right** of the
right one. Both pushes fire at once and **the camera fights itself**, trembling
in place.

Try it: set `CAMERA_DEAD_ZONE_X` to 160 and rebuild.

## 4. The code, line by line

```c
void bmp_camera_follow(int target_x, int target_y, int target_width, int target_height){

	int screen_x;
	int screen_y;
	int right_edge;
	int bottom_edge;

	/* 1. Where the target is INSIDE the window right now */
	screen_x = target_x - camera_x;
	screen_y = target_y - camera_y;

	/* 2. Where the dead zone's edges are */
	right_edge  = WIDTH  - CAMERA_DEAD_ZONE_X - target_width;
	bottom_edge = HEIGHT - CAMERA_DEAD_ZONE_Y - target_height;

	/* 3. Push only if it has left */
	if (screen_x < CAMERA_DEAD_ZONE_X){
		camera_x = camera_x - (CAMERA_DEAD_ZONE_X - screen_x);
	}else if (screen_x > right_edge){
		camera_x = camera_x + (screen_x - right_edge);
	}

	if (screen_y < CAMERA_DEAD_ZONE_Y){
		camera_y = camera_y - (CAMERA_DEAD_ZONE_Y - screen_y);
	}else if (screen_y > bottom_edge){
		camera_y = camera_y + (screen_y - bottom_edge);
	}

	bmp_camera_clamp();

}
```

### Why `- target_width`

```c
	right_edge = WIDTH - CAMERA_DEAD_ZONE_X - target_width;
```

Because **`position_x` is the TOP LEFT corner** of the sprite, not its centre. The
tank's right edge is 18 pixels beyond.

Without subtracting it, the right margin would be measured against the left
corner and the tank would push 18 pixels further into the push zone: the dead
zone would end up off centre.

### Why `else if` and not two `if`s

With the dead zone sized properly (point 3) the two cases are mutually exclusive:
you cannot be both left of the left edge and right of the right one.

But **if someone sets too large a margin**, both would be true at once, and with
two loose `if`s both corrections would apply, one after the other, every frame.
The `else if` limits the damage to a tremble rather than a runaway camera.

### Why it takes integers and not a `struct player *`

```c
	bmp_camera_follow((int)tank.position_x, (int)tank.position_y, TANK_WIDTH, TANK_HEIGHT);
```

On purpose: that way **`bmp.c` still does not know what a tank is**. Tomorrow the
camera can follow a spaceship, a mouse pointer or the midpoint of two things, and
`bmp.c` never finds out.

It is the same decision taken with `sound.c` (you hand it the log) and `net.c`. A
library should not know its client.

## 5. "Exactly as much as it left by"

This is the elegant part, and it is easy to miss.

The correction is `screen_x - right_edge`: **how many pixels it went over by**. No
more, no less.

Follow a tank walking right at 2 pixels a frame, camera at 0:

| Frame | Tank (world) | Camera | On screen | What happens |
|---|---:|---:|---:|---|
| 1 | 250 | 0 | 250 | 250 > 202: over by **48** → camera +48 |
| | | 48 | **202** | sits exactly on the edge |
| 2 | 252 | 48 | 204 | over by **2** → camera +2 |
| | | 50 | **202** | back on the edge |
| 3 | 254 | 50 | 204 | over by **2** → camera +2 |
| | | 52 | **202** | |

**The tank walks 2, the camera walks 2.** The tank stays pinned to the edge of the
dead zone and the world slides behind it at exactly the same speed.

No jump (the movement is continuous) and no lag (the correction is exact). And
when you let go of the key, **the camera stops dead** with it.

### Compare with smoothing

The usual thing people write:

```c
	camera_x = camera_x + (target - camera_x) / 8;     /* NOT what the game does */
```

It feels "cinematic" and for this game it is worse:

- **It always lags**: it never reaches the target, only approaches it
- **It keeps moving after you stop**, with inertia
- And in a game where you aim with the tank's nose, that inertia gets in the way

The dead zone has neither problem because it chases nothing: **it only pushes when
it must, and only as much as it must**.

## 6. The subtraction goes on EVERY object

This chapter has one tank, so you see one subtraction. The real game has **five**:

```c
	draw_sprite_to_buffer(sprite1, ..., (int)player1.position_x - camera_x, ...);
	draw_sprite_to_buffer(sprite2, ..., (int)player2.position_x - camera_x, ...);
	draw_sprite_to_buffer(bullet1, ..., (int)player1.bullet_position_x - camera_x, ...);
	draw_sprite_to_buffer(bullet2, ..., (int)player2.bullet_position_x - camera_x, ...);
	draw_sprite_to_buffer(boom,    ..., (int)(p->position_x + OFFSET) - camera_x, ...);
```

**Forget it on just one** and the symptom is very characteristic and very easy to
recognise:

> That object stays **glued to the screen** while everything else slides past.

A bullet that follows you everywhere instead of staying where you fired it. An
explosion that travels with you. Once you have seen it, you diagnose it in two
seconds.

And the mirror-image mistake: **subtracting it twice** makes the object move at
double speed and in the opposite direction.

## 7. Here chapter 4's clipping pays off

```c
	screen_x = (int)tank.position_x - camera_x;
```

That subtraction **goes negative constantly**: every time the tank approaches the
left edge of the screen, or when the camera is clamped and the tank moves away.

In chapter 4, `-9` turned into an `unsigned int` was **65,527** and wrote 65,000
bytes past the buffer. Here that would happen **all the time**, not as a rare
case.

Clipping stopped being caution and became the normal way this works.

## 8. The clamp, now automatic

```c
	limit_x = map_width  - WIDTH;     /* 640 - 320 = 320 */
	limit_y = map_height - HEIGHT;    /* 400 - 200 = 200 */
```

The same thing you wrote by hand in chapter 21, now inside `bmp_camera_clamp()`
and called at the end of both `follow` and `snap`.

When the camera is clamped, **the tank does leave the dead zone** and approaches
the screen edge. That is correct: there is no more map to show, so what moves is
the tank again.

## 9. And here is the nice part: normal mode is the same code

With a 320x200 map:

```
   limit_x = 320 - 320 = 0
   limit_y = 200 - 200 = 0
```

The clamp pins the camera **to (0,0) for ever**, whatever `bmp_camera_follow()`
computes.

And then:

- `world − camera` is `world − 0`, i.e. `world`
- `bmp_draw_world_window()` copies 200 rows of 320 starting at (0,0), which is
  **exactly the old 64,000-byte `memcpy`**
- Clipping clips nothing, because nothing spills

> **A one-screen world is not a special case: it is the general case with the
> camera clamped to zero.**

Which is why the game has **not a single `if (big_map_mode)` in the drawing**. No
two paths to maintain, no two places to get it wrong.

It is covered by a test: it calls `follow` and `snap` with absurd values on a
320x200 map and checks the camera is still at (0,0).

## 10. The `snap`

```c
	camera_x = target_x + (target_width  / 2) - (WIDTH  / 2);
	camera_y = target_y + (target_height / 2) - (HEIGHT / 2);
	bmp_camera_clamp();
```

Centres the target at once: take its middle and subtract half a screen.

It is for the start of a round. When the tanks teleport to their corners **there
is nothing to follow smoothly from**: the camera has to appear already in place.

And it applies the same clamp, so in a corner it stays at the edge rather than
showing you the void outside the map.

## 11. The order inside the frame

```c
	update_camera();                                  /* 1. where the window is */
	bmp_draw_world_window(buffer);                    /* 2. the background */
	draw_sprite_to_buffer(..., world - camera, ...);  /* 3. everything else */
	wait_retrace();                                   /* 4. wait for the monitor */
	bmp_paint_image_data_to_vga(buffer);              /* 5. blit */
```

The camera is decided **before** anything is painted.

Move it between step 2 and step 3 and the background would be from one position
and the tanks from another: they would come out **offset from the ground**,
floating. One frame yes, one frame no, depending on when it changed.

## 12. On the network, each machine has its own

```c
	if (local_player_is_1 == 0){ target = &player2; }
```

Each machine follows **its own tank**. The two `camera_x` hold different values,
**on purpose**.

Does that not break chapter 18's determinism? **No**, and the reason is the golden
rule: the camera **decides nothing about the game**. Both machines do the same
arithmetic on the same world coordinates and get the same result; then each paints
a different piece of that identical result.

It is two people looking at the same chessboard from opposite sides. They see
different things. The game is the same.

Which is why `camera_x` **never** goes into the checksum (chapter 19): it would
give a false desync on frame 1 of every match.

And why `/bigmap` **only exists in network mode**: a camera can only follow one
tank, and on a shared keyboard one of the two players would be driving blind.

## 13. Experiments

1. **Press `C` and walk around with all three models**, a good minute each. Mode 2
   really does make you queasy; it needs time.
2. **Raise `CAMERA_DEAD_ZONE_X` to 160** in `header/bmp.h`. It passes the limit of
   151: the camera trembles.
3. **Set it to 0.** It becomes mode 2.
4. **Set it to 145**, almost the limit. A 12-pixel dead zone: the camera moves
   nearly always but without the nausea. An interesting middle ground.
5. **Change the `else if` to two `if`s** and set the margin to 160. Now it really
   runs away.
6. **Remove the `- target_width`.** The dead zone ends up 18 pixels off centre and
   you feel it going right.
7. **Replace `bmp_camera_follow()` with the smoothing** from point 5
   (`camera_x += (target - camera_x) / 8`) and compare how it feels when lining up
   a shot.

## 14. What to take away

| | |
|---|---|
| The window already worked (ch21). This is **who moves it** | |
| **Dead zone**: still 88% of the time | No jump, no nausea |
| Pushes **exactly as much as it left by** | Tank 2, camera 2 |
| The margins have a ceiling | Above it, the camera trembles |
| `- target_width` because the position is the **corner** | |
| **The subtraction goes on EVERY object** | Forget it = object glued to the screen |
| **A one-screen world is the general case with clamp 0** | One single code path |
| On the network each machine has its own, and that is right | Never in the checksum |

---

**Previous:** [Chapter 21](../../ch21/doc/README-EN.md) ·
**Next:** [Chapter 23 — The bit mask and memory](../../ch23/doc/README-EN.md)
