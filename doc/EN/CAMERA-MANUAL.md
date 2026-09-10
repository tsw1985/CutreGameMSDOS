# Manual: how the camera and the big map were built

From a fixed 320x200 screen to a 640x400 world two tanks hunt each other
across, each one seeing its own piece of it.

---

## Who this is for

For you in six months, when you open `bmp.c`, find two hundred `memcpy` calls
in a row, and cannot remember why.

I assume you know C, that you are comfortable in DOS, and that you
**understand your own game**: the main loop, how a sprite gets drawn, how a
tank moves.

I assume you know **nothing at all about cameras, scrolling or DOS memory**.
Not what a window is, not what clipping is, not why a pointer can be `far` or
`huge`. All of that is explained here from scratch.

### How to read it

It is in **learning order**, not file order. Each part leans on the one before:

| Part | What it covers |
|---|---|
| **1** | The problem. What happens when the map does not fit on the screen |
| **2** | The idea that solves it: two coordinate systems |
| **3** | Drawing the background: the window |
| **4** | Drawing sprites: clipping |
| **5** | Moving the camera: the dead zone |
| **6** | Why the normal game runs through exactly the same code |
| **7** | Collisions: the bitmask |
| **8** | The network: why the camera cannot be part of the game |
| **9** | Memory: 640 KB, and the afternoon lost to 8,600 bytes |
| **10** | Taking this to another project |
| **11** | Reference |
| **12** | Common mistakes and glossary |

**Do not skip part 2.** It is 80% of the understanding. Parts 3, 4 and 5 are
mechanics; without part 2 they are noise.

---

# PART 1 — THE PROBLEM

## 1.1 The screen is not the world

Until now, in this game, the map and the screen were **the same thing**.

VGA mode 13h gives a screen 320 pixels wide by 200 tall, one byte per pixel.
That is exactly 64,000 bytes. And the map, `cutre.bmp`, was 320x200. One byte
per pixel. Exactly 64,000 bytes.

That coincidence made drawing the background the simplest operation there is:

```c
memcpy(buffer_background_image_data, buffer_original_background_bmp, 64000);
```

Copy the whole map over the whole screen buffer. Nothing to decide, because the
map **is** the screen.

Drawing a tank was just as direct. If the tank is at (110, 164) in the map, it
gets painted at (110, 164) on the screen. The same coordinate serves both,
because they are the same space.

That is very comfortable and it is a trap: you get used to "the tank's
position" having one single meaning. The moment the map grows it stops having
one, and if you do not notice, everything else fails in very strange ways.

## 1.2 What happens when the map grows

The map is now `big.bmp`: **640 x 400**. Four times bigger.

```
        640 pixels
   +---------------------------+
   |                           |
   |                           |  400
   |                           |  pixels
   |                           |
   +---------------------------+

        And the screen:

   +--------------+
   |              |  200
   |              |
   +--------------+
        320
```

The physical screen has not changed. It is still 320x200, because that is what
the hardware gives. So the question is immediate:

> **Out of those 640x400 pixels, which 320x200 do you see?**

That question is, literally, the whole camera. Everything else follows from it.

## 1.3 The three ways of answering it

There are three classic models, and choosing wrong is expensive. I go through
them because understanding them is understanding why the code does what it
does.

### Model A — Rooms (fixed screens)

You cut the map into exact 320x200 pieces. A 640x400 map is four "rooms":

```
   +------+------+
   |  0   |  1   |
   +------+------+
   |  2   |  3   |
   +------+------+
```

One whole room shows at a time. When the tank crosses an edge, the screen
**jumps** to the next room, all at once.

That is how NES Zelda worked, and Bomberman, and half the consoles of the
eighties.

- **For:** dead simple. The background is still one 64,000 byte `memcpy`,
  because each room is exactly one screen.
- **Against:** the jump looks wrong; the tank goes from hugging the right edge
  to appearing on the left one. And if the tank sits right on the boundary
  moving back and forth, the screen **flickers** between two rooms several
  times a second.

### Model B — Always centred

The tank is nailed to the middle of the screen and what moves is the world,
underneath it.

- **For:** perfectly smooth, never a jump.
- **Against:** it is dizzying. You think you are moving your tank, and what you
  see moving is **everything else**. In a tank game, where you spend a lot of
  time making small positional adjustments, it wears you out.

### Model C — Dead zone (the one chosen)

The camera stays **still** while the tank moves inside a rectangle in the
middle of the screen. When it leaves that rectangle, the camera starts
pushing, pixel by pixel, exactly enough to put it back inside.

- **For:** no jump, because the movement is continuous. And no dizziness,
  because in normal play **the camera is stopped nearly all the time**.
- **Against:** you have to write it. About twenty lines.

In the real game, measured over a 200 frame walk, **the camera is still for 88%
of the frames**. That is exactly what was wanted: the world only moves when you
are really going somewhere.

Model C is what is implemented, and the rest of this manual explains it.

---

# PART 2 — THE IDEA: TWO COORDINATE SYSTEMS

This is **the** important part. If you take one thing away from this manual,
take this.

## 2.1 The same point, two different numbers

The moment the world is bigger than the screen, "the tank is at position 400"
becomes ambiguous. 400 of what?

Two separate spaces have to be kept apart:

**WORLD coordinates.** Where things really are, on the complete map. 0 to 639
across and 0 to 399 down. **Everything that belongs to the game lives here**:
tank positions, bullet positions, the wall map. These do not depend on what you
happen to be looking at.

**SCREEN coordinates.** Where a thing gets painted this frame. 0 to 319 and 0
to 199, because that is the screen. Only the drawing code uses them, and only
for as long as it is drawing.

```
   WORLD 640x400
   +--------------------------------+
   |                                |
   |      +--------------+          |
   |      |   SCREEN     |          |
   |      |              |   T      |   <- tank at world (500, 150),
   |      |    T         |          |      outside the window
   |      |              |          |
   |      +--------------+          |
   |                                |
   +--------------------------------+
```

The same tank has, at the same time, a world position (which does not change
when you move the camera) and a screen position (which changes with the camera
even when the tank does not move).

**Mixing the two is mistake number one.** It produces baffling bugs: bullets hit
walls that are not there, tanks walk through walls, and everything works fine
as long as you stay near the origin.

## 2.2 The camera is two numbers. That is all

The camera is not an object, or a class, and it has no zoom or rotation. It is
two integers saying **where the top left corner of the window is**, in world
coordinates:

```c
extern int camera_x;
extern int camera_y;
```

If `camera_x = 0` and `camera_y = 0`, you are looking at the top left corner of
the map. If `camera_x = 320` and `camera_y = 200`, you are looking at the
bottom right quarter.

That is all a 2D camera is. Really.

## 2.3 The formula

Converting from world to screen is a subtraction:

```
screen_x = world_x - camera_x
screen_y = world_y - camera_y
```

That is it. That subtraction is the entire camera.

An example with numbers, which is how it lands:

| | |
|---|---|
| The tank is in the world at | (500, 150) |
| The camera is at | (320, 100) |
| So on screen it is drawn at | (500-320, 150-100) = **(180, 50)** |

And if the camera moves to (400, 100) without the tank moving:

| | |
|---|---|
| The tank is still in the world at | (500, 150) |
| The camera is now at | (400, 100) |
| On screen it is drawn at | (500-400, 150-100) = **(100, 50)** |

The tank has not moved a single pixel in the world, but it is drawn 80 pixels
further left. That is scrolling.

## 2.4 The golden rule

> **Game state lives in world coordinates.**
> **The camera only decides what gets painted.**

A surprising number of hard-looking things fall out of that sentence for free:

- **A bullet fired in a room you cannot see arrives anyway.** There is no such
  thing as "the bullet in room B": there is a bullet at position (400, 150).
  When that position enters your window, you see it. Nothing special to write.
- **The two tanks collide correctly even when far apart.** The box check uses
  world coordinates; if they are in different rooms their coordinates are 300
  pixels apart and the boxes do not overlap. You never have to ask "are they in
  the same room?".
- **On the network each machine can put its camera wherever it likes** without
  the two games drifting apart. Part 8 goes into this properly.

And the negative form of the rule, which matters just as much:

> **If a value is worked out by the camera, it cannot decide anything in the
> game.**

---

# PART 3 — DRAWING THE BACKGROUND: THE WINDOW

## 3.1 Why one memcpy no longer works

Before:

```c
memcpy(buffer_background_image_data, buffer_original_background_bmp, 64000);
```

That `memcpy` works because the map's 64,000 bytes are, in the same order, the
screen's 64,000 bytes. Map row 0, screen row 0. Map row 1, screen row 1. All
contiguous.

With a map 640 wide that stops being true, and the reason is that **memory is
linear and an image is rectangular**.

## 3.2 How an image sits in memory

A 640x400 image is not stored as a rectangle. It is stored as one single strip
of 256,000 bytes, row after row:

```
  memory:  [ row 0 (640 bytes) ][ row 1 (640 bytes) ][ row 2 ] ...
```

To find the byte holding pixel (x, y):

```
  position = y * map_width + x
```

That `map_width` is called the **stride**: how many bytes to step forward to go
down one row. Here it is 640.

Now look at what you want to copy: a window 320 wide inside a map 640 wide.

```
  WORLD, 640 wide:

  row 100:  ....................[XXXXXXXXXXXXXXXX]....................
  row 101:  ....................[XXXXXXXXXXXXXXXX]....................
  row 102:  ....................[XXXXXXXXXXXXXXXX]....................
                                 ^                ^
                                 camera_x         camera_x + 320

  In memory those three rows sit this far apart:

  [ ...320... XXXX ...320... ][ ...320... XXXX ...320... ][ ... ]
              ^-- what I want       ^-- what I want
                        <-- 640 bytes apart -->
```

**The pieces you want are not next to each other.** Between the end of one
piece and the start of the next there are 320 bytes you do not want. A single
`memcpy` cannot skip them.

## 3.3 The answer: one row at a time

If you cannot copy it in one go, you copy it in 200 goes: one per screen row.

```c
void bmp_draw_world_window(unsigned char *destination){

	int row;
	unsigned char huge *source;
	unsigned int destination_offset;

	destination_offset = 0;

	for (row = 0; row < HEIGHT; row++){

		source = buffer_original_background_bmp
		       + ((unsigned long)(camera_y + row) * (unsigned long)map_width)
		       + (unsigned long)camera_x;

		memcpy(destination + destination_offset, source, WIDTH);

		destination_offset = destination_offset + WIDTH;

	}

}
```

Read it slowly, because it is the heart of the system:

- `row` goes 0 to 199: the 200 rows of the **screen**.
- `camera_y + row` turns that screen row into its **world** row. If the camera
  is at y=100, screen row 0 is world row 100.
- `* map_width` skips that many whole world rows, 640 bytes each.
- `+ camera_x` steps along the row to the column where the window starts.
- `memcpy(..., WIDTH)` copies 320 bytes: one complete screen row.
- `destination_offset` advances 320 in the screen buffer, where rows *are*
  contiguous.

**The total number of bytes copied is the same as before**: 200 rows x 320
bytes = 64,000. It is not more work, it is the same work split into 200 calls
instead of one. The extra cost is the overhead of calling `memcpy` 200 times,
which on a 486 does not show.

## 3.4 Why `source` is rebuilt from scratch every time

Notice that inside the loop the address is built **from the base** each time,
instead of doing `source = source + map_width` at the end.

That looks wasteful. It is deliberate, and it has to do with how pointers work
in DOS. The full explanation is in part 9.3, but the short version: rebuilding
it from the base forces the compiler to **normalize** the pointer, and a
normalized pointer can never run off its segment when you add 320 to it. If you
accumulated it instead, at some point it would wrap around and copy garbage.

---

# PART 4 — DRAWING SPRITES: CLIPPING

## 4.1 The problem

The background always fills the whole screen. A sprite does not: it can be
**half off**.

```
   SCREEN
   +--------------------------+
 T |                          |     <- tank half off to the left
 T |                          |
   |                          |
   |                          |
   |                       T  T     <- and another half off to the right
   +--------------------------+
```

And with a camera this is not a rare case. It happens **every time** somebody
goes near an edge of the screen, which is constantly.

## 4.2 The bug that nearly hung the machine

Here is the original function:

```c
void draw_sprite_to_buffer(unsigned char *sprite,
                           unsigned int sprite_width,
                           unsigned int sprite_height,
                           unsigned int dest_x,      /* <-- UNSIGNED */
                           unsigned int dest_y,      /* <-- UNSIGNED */
                           unsigned char *dest_buffer)
{
    for(y = 0; y < sprite_height; y++) {
        for(x = 0; x < sprite_width; x++) {
            dest_offset = ((dest_y + y) * 320) + (dest_x + x);
            pixel = sprite[src_offset];
            if(pixel != 0) {
                dest_buffer[dest_offset] = pixel;   /* checking NOTHING */
            }
        }
    }
}
```

Two problems, and the second is serious.

**One: it checks nothing.** It writes wherever it is told.

**Two: `dest_x` is `unsigned int`.** And this function is now called like this:

```c
draw_sprite_to_buffer(..., (int)player1.position_x - camera_x, ...);
```

If the tank is at world x=311 and the camera at x=320, that subtraction gives
**-9**. Perfectly reasonable: the tank is 9 pixels left of the screen edge.

But putting -9 into a 16 bit `unsigned int` does not give -9. It gives
**65527**.

And then:

```
  dest_offset = (dest_y + y) * 320 + 65527
```

That is an offset about 65,000 bytes outside the screen buffer. DOS has no
memory protection: that write **happens**, and it flattens whatever is there.
It might be another buffer, it might be your own code, it might be the
interrupt vector table. The symptom is that the game misbehaves or hangs,
minutes later, somewhere unrelated to the bug.

## 4.3 The fix, in two halves

**Half one: signed coordinates.**

```c
int dest_x,
int dest_y,
```

An `int` in Turbo C reaches 32,767, plenty for a 640 wide world, and now -9 is
-9.

A question that comes up on its own here: **why `int` and not `long`?** Because
`long` is 32 bits, the 8086 has no 32 bit arithmetic, and every operation
becomes several instructions (and multiplies and divides become calls to
library routines). You would pay all of that for a range of two billion you are
never going to use. `int` gives you 32,767, which is 102 screens across.

**Half two: clip.**

```c
    /* Completely off the screen: nothing to do */
    if (dest_x >= WIDTH){ return; }
    if (dest_y >= HEIGHT){ return; }
    if (dest_x + (int)sprite_width <= 0){ return; }
    if (dest_y + (int)sprite_height <= 0){ return; }

    /* Which part of the sprite actually lands on the screen */
    start_x = 0;
    if (dest_x < 0){ start_x = -dest_x; }

    start_y = 0;
    if (dest_y < 0){ start_y = -dest_y; }

    end_x = (int)sprite_width;
    if (dest_x + end_x > WIDTH){ end_x = WIDTH - dest_x; }

    end_y = (int)sprite_height;
    if (dest_y + end_y > HEIGHT){ end_y = HEIGHT - dest_y; }

    for(y = start_y; y < end_y; y++) {
        for(x = start_x; x < end_x; x++) {
            ...
        }
    }
```

Worked through: an 18x18 sprite at `dest_x = -9`.

- Not completely off (`-9 + 18 = 9 > 0`), so carry on.
- `start_x = 9`. Columns 0 to 8 of the sprite fall outside and are never
  visited.
- `end_x = 18`. Nothing spills off the right.
- Columns 9 to 17 get painted: 9 columns x 18 rows = 162 pixels.

## 4.4 Why the bounds are computed before the loops

You could have written it shorter:

```c
for(y = 0; y < sprite_height; y++) {
    if (dest_y + y < 0 || dest_y + y >= HEIGHT) continue;
    for(x = 0; x < sprite_width; x++) {
        if (dest_x + x < 0 || dest_x + x >= WIDTH) continue;
        ...
    }
}
```

Same result, but it does **two comparisons per pixel**, always, even when the
sprite is entirely on screen. A tank is 324 pixels, and there are two tanks and
two bullets per frame.

Computing the bounds once before the loops means the normal case (sprite fully
inside) pays nothing at all, and the clipped case does not either. On a 486
that matters.

## 4.5 What was tested

This is the function that can hang the machine, so it was tested brutally: an
18x18 sprite was placed at **every** position from (-40, -40) to (360, 240) ---
112,681 positions --- with **guard bytes** (the value 0xAA) surrounding the
screen buffer on both sides.

Result: **not one byte written outside the buffer**. And the exact counts:

| Case | Pixels painted |
|---|---|
| Sprite fully inside | 324 (18x18) |
| Half off to the left (-9) | 162 (9x18) |
| Half off to the right | 162 (9x18) |
| Top left corner (-9,-9) | 81 (9x9) |
| Completely off | 0 |

---

# PART 5 — MOVING THE CAMERA: THE DEAD ZONE

We can now paint the window wherever it is. What is left is deciding **where to
put it** each frame.

## 5.1 The idea, in one sentence

> While the tank moves inside a rectangle in the middle of the screen, the
> camera does not move. When it leaves, the camera pushes it back by exactly
> as much as it left by.

```
   SCREEN 320x200

   +----------------------------------+
   |                                  |
   |     +----------------------+     |  <- 70 px margin at the top
   |     |                      |     |
   |     |      DEAD ZONE       |     |
   |     |   the camera does    |     |
   |     |    not move here     |     |
   |     |                      |     |
   |     +----------------------+     |  <- 70 px margin at the bottom
   |                                  |
   +----------------------------------+
      ^                            ^
      100 px                    100 px
```

## 5.2 The two numbers that define it

```c
#define CAMERA_DEAD_ZONE_X 	100
#define CAMERA_DEAD_ZONE_Y 	 70
```

They are the margins from the screen edge to the edge of the dead zone. With
those values and an 18x18 tank, the dead zone runs:

- Horizontally, x=100 to x = 320 - 100 - 18 = **202**. A 102 px lane.
- Vertically, y=70 to y = 200 - 70 - 18 = **112**. A 42 px lane.

**There is a limit you must not cross** when picking these: the margin has to be
less than half the screen minus the sprite. Otherwise the left edge of the dead
zone would sit to the right of the right edge, both pushes would fire at once,
and the camera would fight itself, trembling in place.

| | Limit | Value used |
|---|---|---|
| `CAMERA_DEAD_ZONE_X` | under (320-18)/2 = 151 | 100 |
| `CAMERA_DEAD_ZONE_Y` | under (200-18)/2 = 91 | 70 |

## 5.3 The code

```c
void bmp_camera_follow(int target_x, int target_y, int target_width, int target_height){

	int screen_x;
	int screen_y;
	int right_edge;
	int bottom_edge;

	/* Where the target is INSIDE the window right now */
	screen_x = target_x - camera_x;
	screen_y = target_y - camera_y;

	right_edge  = WIDTH  - CAMERA_DEAD_ZONE_X - target_width;
	bottom_edge = HEIGHT - CAMERA_DEAD_ZONE_Y - target_height;

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

Note that it takes **integers, not a `struct player`**. That is on purpose:
`bmp.c` still does not know what a tank is, and tomorrow the camera can follow
whatever you like in another project. Same approach as was taken with `sound.c`
and `net.c`.

## 5.4 Why "exactly as much as it left by"

This is the elegant part of the model, and it is easy to miss.

The correction is `screen_x - right_edge`, that is: **how many pixels it went
over by**. No more, no less.

Follow a tank walking right at 2 pixels per frame:

| Frame | Tank (world) | Camera | Tank on screen | What happens |
|---|---|---|---|---|
| 1 | 250 | 0 | 250 | 250 > 202: over by 48. Camera +48 |
| | | 48 | 202 | sits exactly on the edge |
| 2 | 252 | 48 | 204 | over by 2. Camera +2 |
| | | 50 | 202 | back on the edge |
| 3 | 254 | 50 | 204 | over by 2. Camera +2 |
| | | 52 | 202 | |

**The tank walks 2, the camera walks 2.** No jump and no lag: the tank stays
pinned to the edge of the dead zone and the world slides behind it at exactly
the same speed.

And the moment you let go of the key, the tank stops moving, stops being
outside the dead zone, and the camera **stops dead** with it. No inertia, no
coasting.

Compare that with a "camera that chases with smoothing" of the form
`camera_x += (target - camera_x) / 8`. That always lags, always keeps moving
for a while after you stop, and in a game where you aim with the nose of your
tank, it gets in the way.

## 5.5 Not leaving the map: the clamp

If the tank heads for the top left corner, the formula above would want to put
`camera_x` negative. The window would then read memory from before the start of
the map.

```c
static void bmp_camera_clamp(){

	int limit_x;
	int limit_y;

	limit_x = map_width  - WIDTH;
	limit_y = map_height - HEIGHT;

	if (limit_x < 0){ limit_x = 0; }
	if (limit_y < 0){ limit_y = 0; }

	if (camera_x < 0){ camera_x = 0; }
	if (camera_y < 0){ camera_y = 0; }
	if (camera_x > limit_x){ camera_x = limit_x; }
	if (camera_y > limit_y){ camera_y = limit_y; }

}
```

`map_width - WIDTH` is the furthest right the window can sit without its right
edge leaving the map. For 640: `640 - 320 = 320`.

So `camera_x` can only be 0 to 320, and `camera_y` only 0 to 200.

When the camera is clamped, the tank **does** leave the dead zone and approach
the screen edge. That is correct: there is no more map to show.

## 5.6 The snap: starting a round

When a round starts, the tanks teleport back to their corners. There is nothing
to follow smoothly from: the camera has to appear already in place.

```c
void bmp_camera_snap(int target_x, int target_y, int target_width, int target_height){

	camera_x = target_x + (target_width  / 2) - (WIDTH  / 2);
	camera_y = target_y + (target_height / 2) - (HEIGHT / 2);

	bmp_camera_clamp();

}
```

It centres the target: take its middle and subtract half a screen. And it
applies the same clamp, so if the tank is hugging a corner the camera stays at
the edge instead of showing you the void outside the map.

## 5.7 Who the camera follows

In `main.c`:

```c
void update_camera(int snap_to_target){

	struct player *target;

	target = &player1;

	if (network_mode == 1){
		if (local_player_is_1 == 0){
			target = &player2;
		}
	}

	if (snap_to_target == 1){
		bmp_camera_snap((int)target->position_x, (int)target->position_y, TANK_WIDTH, TANK_HEIGHT);
	}else{
		bmp_camera_follow((int)target->position_x, (int)target->position_y, TANK_WIDTH, TANK_HEIGHT);
	}

}
```

**Each machine follows its own tank.** That line is what makes supernet mode
make sense: the two players see different parts of the map and have to go
looking for each other.

And that is why the big map **only exists over the network**. A camera can only
follow one tank; on one keyboard with two players, one of them would be driving
blind. So `/bigmap` without `/net` is refused:

```c
	if (network_mode == 0){
		if (big_map_mode == 1){
			printf("\n/bigmap needs /net: it is the big map that has to be\n");
			...
		}
		big_map_mode = 0;
	}
```

## 5.8 Where it fits in the frame

In `draw_to_buffer()`, and the order matters:

```c
void draw_to_buffer(){

	/* 1. Decide where the window is THIS frame */
	update_camera(0);

	/* 2. Paint the piece of map that goes with it */
	bmp_draw_world_window(buffer_background_image_data);

	/* 3. Paint everything else on top, subtracting the camera */
	draw_sprite_to_buffer(sprite_player1, TANK_WIDTH, TANK_HEIGHT,
	                      (int)player1.position_x - camera_x,
	                      (int)player1.position_y - camera_y,
	                      buffer_background_image_data);
	...
}
```

The camera is decided **before** anything is painted, so the background and
everything on top of it agree about the same frame. If you moved the camera
between painting the background and painting the tanks, the tanks would come
out offset from the ground.

---

# PART 6 — WHY THE NORMAL GAME USES THE SAME CODE

There is a design decision here worth understanding, because it is what stops
you having two games to maintain.

You could have written:

```c
if (big_map_mode == 1){
    /* camera code */
}else{
    /* old code */
}
```

**There is none of that.** The normal mode goes through exactly the same
functions. It works because the numbers take care of themselves:

With a 320x200 map:

- `limit_x = map_width - WIDTH = 320 - 320 = 0`
- `limit_y = map_height - HEIGHT = 200 - 200 = 0`

The clamp pins `camera_x` and `camera_y` **to 0 for ever**. Whatever
`bmp_camera_follow()` computes, the clamp puts it back to 0.

And then:

- `bmp_draw_world_window()` copies 200 rows of 320 bytes starting at (0,0),
  which is exactly the old 64,000 byte `memcpy`, chopped into pieces.
- `(int)position_x - camera_x` is `position_x - 0`, i.e. `position_x`.
- Clipping clips nothing, because nothing spills off.

> **A one screen world is not a special case: it is the general case with the
> camera clamped to zero.**

This is tested: there is a check that calls `bmp_camera_follow()` and
`bmp_camera_snap()` with a 320x200 map and absurd values, and confirms the
camera is still at (0,0).

---

# PART 7 — COLLISIONS

## 7.1 Never read the picture to decide

This was already true before the camera, but it matters more now.

The game has **two** images of the map:

| Buffer | What it is | What it is for |
|---|---|---|
| `buffer_original_background_bmp` | The pretty picture: bricks, bushes | Painting only |
| The collision mask | Wall / not wall | Deciding only |

Why not use the picture for collisions and save a file?

**Because the picture lies.** A brick wall has dark mortar lines between the
bricks. Ask "what colour is this pixel?" in the wrong place and it answers
"black", and the game decides there is no wall there. In this particular map
there are **22,806 pixels** that are black in the picture and are wall.

And the other way round: the green bushes are drawn but can be driven over.
**13,047 pixels** that look solid and are not.

That is why there is a separate `bigcol.bmp` with two colours: blue (floor) and
yellow (wall, palette index 252).

There is a third source that must **never** be used to decide: video memory. By
the time you read from there the tanks are already painted on top, so asking
for the pixel at the cannon tip gives you the tank's own colour.

## 7.2 Why the mask has to cover the whole world

This one is subtle, and it is where the camera and the network meet.

In a network game **both machines simulate the whole game**: both tanks and
both bullets. That is what lockstep is.

So your machine, currently showing the top left room, has to be able to answer:
*"the other tank, over in the bottom right room, has it hit a wall?"*

If the mask only held the visible part, your machine would not have that data.
It would answer differently from the other one, and the two games would drift
apart.

> **The collision mask covers the whole world, always, seen or not.**

## 7.3 One bit per pixel

A 640x400 world is 256,000 pixels. At one byte per pixel that is 256,000 bytes,
and it does not fit (part 9 explains why).

But look at what the mask is ever asked:

```c
if (bmp_is_wall(x, y) == 1){ ... }
```

There are only **two possible answers**. Of the 256 values that fit in a byte,
you care about one. You are spending 8 bits to store a yes/no.

A yes/no fits in 1 bit, and 8 pixels fit in a byte:

```
  One byte per pixel (8 pixels = 8 bytes):

    pixel:   0     1     2     3     4     5     6     7
    bytes: [00]  [00]  [FF]  [FF]  [00]  [00]  [00]  [FF]

  One bit per pixel (8 pixels = 1 byte):

    pixel:   0  1  2  3  4  5  6  7
    bits:    0  0  1  1  0  0  0  1
    byte:  [ 00110001 ]
```

Eight times less memory for exactly the same information:

| | Pixels | Bytes |
|---|---|---|
| World 640x400, 1 byte/px | 256,000 | 256,000 |
| World 640x400, **1 bit/px** | 256,000 | **32,000** |

32,000 bytes fits in a plain `malloc()`. And it is **half** what the single
screen collision map used to cost (64,000). So the collision map of the whole
world takes less room than the one for a single screen did.

## 7.4 How a bit is read

```c
int bmp_is_wall(int x, int y){

	unsigned long bit_index;
	unsigned int  byte_index;
	unsigned char bit;

	if (x < 0){ return 1; }
	if (y < 0){ return 1; }
	if (x >= map_width){ return 1; }
	if (y >= map_height){ return 1; }

	if (buffer_collision_mask == NULL){ return 0; }

	bit_index  = ((unsigned long)y * (unsigned long)map_width) + (unsigned long)x;
	byte_index = (unsigned int)(bit_index >> 3);
	bit        = (unsigned char)(1 << (unsigned int)(bit_index & 7L));

	if ((buffer_collision_mask[byte_index] & bit) != 0){
		return 1;
	}

	return 0;

}
```

Step by step:

1. **`bit_index`** is the pixel number counted from the start of the world, the
   same as always: `y * width + x`. It goes 0 to 255,999, so it needs 32 bits
   (`unsigned long`).
2. **`>> 3`** is divide by 8: which byte holds that bit. A shift, not a real
   division.
3. **`& 7`** is the remainder of dividing by 8: which bit inside that byte. An
   AND, also not a real division.
4. **`1 << bit`** builds a mask with a single bit set.
5. **`&`** tests whether that bit is on.

The extra cost over reading a plain byte is one shift and two ANDs. It is
called 3 times per tank per frame plus once per bullet. Negligible.

One performance detail that does matter: `(unsigned long)y * map_width` looks
like a 32 bit multiply, which would be expensive. But the 8086 has a `MUL`
instruction that multiplies two 16 bit numbers into a 32 bit result **in a
single instruction**. The compiler uses it. It comes out cheap.

## 7.5 Off the map counts as wall

Look at the first four checks. Any coordinate outside the world returns 1, i.e.
"there is a wall".

That is not a game rule, it is a **safety net**. The maps are drawn with a
solid border 16 to 33 pixels thick, so it should never come up. But if you ever
draw a map with a hole in the border, the tank stops dead instead of the game
reading memory that is not ours and hanging twenty seconds later for an
incomprehensible reason.

## 7.6 How the mask is built

The file you draw in Paint is a normal 640x400 BMP with two colours. The mask
is packed at startup, reading the file **one row at a time**:

```c
	for (row = map_height - 1; row >= 0; row = row - 1){

		fread(map_line, 1, map_width, file);

		for (column = 0; column < map_width; column++){

			if (map_line[column] == MAP_WALL_COLOR){

				bit_index  = ((unsigned long)row * (unsigned long)map_width) + (unsigned long)column;
				byte_index = (unsigned int)(bit_index >> 3);

				buffer_collision_mask[byte_index] =
					buffer_collision_mask[byte_index] | (unsigned char)(1 << (unsigned int)(bit_index & 7L));

			}

		}

	}
```

Two things to explain:

**The loop counts backwards.** BMP files store their rows **bottom-up**: the
first row in the file is the last row of the picture. Instead of reading it all
and flipping it afterwards, it is read forwards and written backwards. Same
work, no temporary buffer needed.

**The 256,000 bytes never exist.** One 640 byte row is read, its bits are
packed, and the row is thrown away. Only the 32,000 packed bytes are ever
allocated. On a 640 KB machine that matters a great deal.

## 7.7 A warning about drawing maps

There is a rule about drawing the collision map that the code cannot enforce
and that will bite you if you break it:

> **The border wall has to be at least 8 pixels thick.**

Because the tank moves `PIXEL_TO_MOVE` = 2 pixels at a time. The positions it
can occupy are start, start+2, start+4... **never the ones in between**. A wall
1 pixel thick can sit at a coordinate the tank never lands on, and **it walks
straight through it**.

Bullets are worse: they move 3 at a time, and they are checked at **one single
point**, not three like the tank.

The maps in this game have 16 to 33 pixels of border. Plenty.

The same goes for gaps between rooms: a gap has to be **wider than the tank**,
not equal to it. During development there was a doorway of exactly 18 pixels
with a tank of exactly 18, and it was impossible to get through: it would have
needed the tank's coordinate to land on the one exact value that fits. Leave 25
or 30.

---

# PART 8 — THE NETWORK: THE CAMERA IS NOT PART OF THE GAME

## 8.1 A reminder of how lockstep works

In two sentences, because the network manual covers it in full: positions are
not sent. **Which keys each player pressed** is sent, and both machines
simulate the complete game from the same starting state. If both do exactly the
same arithmetic, they reach the same result.

The key word is **exactly**. If one machine computes a single number
differently from the other, the two games drift apart and never recover. That
is called a desync.

## 8.2 And now the two cameras are in different places

In supernet, machine A follows tank 1 and machine B follows tank 2. Their
`camera_x` hold different values. **On purpose.**

Does that not break determinism?

**No, and the reason is the golden rule from part 2.** The camera decides
nothing about the game. It only decides what gets painted. Both machines do the
same arithmetic on the same world coordinates, get the same result, and then
each paints a different piece of that identical result.

It is two people looking at the same chessboard from opposite sides. They see
different things. The game is the same.

## 8.3 The rule, in the negative

> **If a value is computed by `bmp_camera_follow()`, it cannot go into the
> checksum or influence any game decision.**

The checksum is the check each machine runs every 30 frames: it boils its state
down to one number and compares it with the other machine's. If they differ,
there is a desync.

```c
unsigned int compute_state_checksum(){

	unsigned int checksum;

	checksum = 0;

	checksum = checksum + (player1.position_x * 3);
	checksum = checksum + (player1.position_y * 5);
	...
	checksum = checksum + (explosion_pause_counter * 71);

	checksum = checksum + ((unsigned int)map_width * 73);
	checksum = checksum + ((unsigned int)map_height * 79);

	return checksum;

}
```

Positions yes. Directions yes. Bullets yes. **`camera_x` and `camera_y` do not
appear**, and cannot: if you put them in, the two machines would produce
different numbers on frame 1 of every game and it would report a desync every
time.

## 8.4 The tripwire: `map_width` in the checksum

Look at the last two lines. `map_width` is not game state: it never changes
during a game. What is it doing there?

It is a **deliberate tripwire**. If you start one machine with `/bigmap` and the
other without, the two worlds are different sizes, the walls are in different
places, and the two simulations come apart in a way that is very hard to read
from the outside: the tanks do things that make no sense and you cannot tell
why.

By putting `map_width` in the checksum, that situation turns into a **clean
desync report in the log** on the first check. The game tells you the two
machines are not playing the same game, instead of leaving you staring at two
screens that diverge.

It is cheap: two additions every 30 frames.

## 8.5 The bullet that arrives from another room

This is the case that looks like it needs special code and does not.

Situation: player 1 is in the top left room. Player 2, in the top right one,
fires left. The bullet flies towards player 1, crossing a boundary neither of
them can see.

**What has to be written for this?** Nothing.

- Player 1's machine **is already simulating that bullet**. It computed it
  itself, frame by frame, since it was fired, because it simulates the whole
  game.
- The bullet has a world position, say (400, 150).
- When painting, the same question is asked as for everything else:
  `400 - camera_x`. If that lands between 0 and 319, it gets painted. If not,
  it does not.
- When the bullet reaches x=390 and your camera is at 80, it appears on screen
  at x=310: **it comes in through the right edge**, in view.

There is no "bullet in room B". There is a bullet at a position. The camera
decides whether you see it.

And that bullet's collision against the walls of the room you cannot see works
too, because the mask covers the whole world (part 7.2).

This was actually verified: in the camera test a bullet is fired from x=540
heading left while the tank is down below, and in the rendered frames you can
watch it come in through the right edge of the window and cross the screen.

## 8.6 What is local to each machine

To make it explicit, a summary of which side each thing is on:

| Data | Identical on both machines? |
|---|---|
| `player1.position_x` / `position_y` | **Yes**, mandatory |
| `bullet_position_x` / `bullet_is_flying` | **Yes**, mandatory |
| The collision mask | **Yes**, mandatory |
| `map_width` / `map_height` | **Yes**, and the checksum watches it |
| `camera_x` / `camera_y` | **No**, and rightly so |
| `local_player_is_1` | **No**: it is the opposite on each |
| What is in `buffer_background_image_data` | **No**: each paints its own piece |

---

# PART 9 — MEMORY: 640 KB AND A LOST AFTERNOON

This part is not about cameras. It is about why, in DOS, something that "fits"
can fail to fit, and it is where most of the project's time went. If you are
ever going to touch DOS code, this is worth more than the rest of the manual.

## 9.1 The budget

Real mode DOS has 640 KB of conventional memory, and everything comes out of
it: DOS itself, drivers, the IPX TSRs, your program's code, its stack, and
everything you allocate.

What this game measures at startup, on the test machine:

```
Memory at start: near 571344  far 571344
```

**571,344 bytes.** That is the entire budget.

And what has to go inside it, in supernet mode:

| What | Bytes |
|---|---:|
| The 640x400 map | 256,000 |
| Screen buffer | 64,000 |
| Collision mask | 32,000 |
| Palette | 309 |
| Sound DMA buffer | 8,192 |
| The 4 WAV files | 180,731 |
| Tank sprites | ~6,300 |
| **Total** | **547,532** |

23,812 bytes spare. That is how tight it is.

## 9.2 `malloc` cannot ask for more than 65,535 bytes

First surprise in Turbo C: `malloc()` takes a `size_t`, which is a **16 bit**
integer. The largest number that fits is 65,535.

```c
malloc(64000);    /* fine: 64000 < 65535 */
malloc(256000);   /* impossible: you cannot even ask */
```

For bigger blocks there is `farmalloc()`, which takes an `unsigned long`:

```c
buffer_original_background_bmp = (unsigned char huge *)farmalloc(world_size);
```

That is why the map is allocated with `farmalloc` and everything else with
`malloc`: the map is the only thing over 64 KB.

**A warning, because I got this wrong during this project:** for a while I
assumed `malloc` and `farmalloc` used separate memory pools, and built a whole
line of reasoning on top of it. The log disproved it:

```
Memory at start: near 571344  far 571344
```

`coreleft()` and `farcoreleft()` return **the same number**. In the huge model
they are the same pool. If you find yourself reasoning about how the system
behaves, measure before you build on it.

## 9.3 `far` and `huge` pointers

Second surprise. On the 8086 an address is formed from two 16 bit numbers:

```
   physical address = segment * 16 + offset
```

A `far` pointer holds both. The problem is that `far` pointer arithmetic **only
touches the offset**. And the offset is 16 bits, so when it goes past 65,535 it
**wraps around to zero** instead of carrying into the segment.

In a 64,000 byte buffer that does not matter, you never get there. In a 256,000
byte one, you wrap four times and read garbage.

The fix is the `huge` modifier:

```c
extern unsigned char huge *buffer_original_background_bmp;
```

A `huge` pointer is **normalized** on every operation: the compiler readjusts
segment and offset so the offset always stays between 0 and 15. So there is
never a wrap, and you can walk blocks of any size.

It costs a few extra instructions per operation, so it is used only where it is
needed.

**And now the detail from part 3.4 makes sense:**

```c
	for (row = 0; row < HEIGHT; row++){

		source = buffer_original_background_bmp
		       + ((unsigned long)(camera_y + row) * (unsigned long)map_width)
		       + (unsigned long)camera_x;

		memcpy(destination + destination_offset, source, WIDTH);
	}
```

`source` is rebuilt from the base every time round. Doing that makes Turbo C
normalize it: the offset lands between 0 and 15. So the 320 byte `memcpy` that
follows reaches offset 335 at worst, nowhere near 65,535, and **it is
impossible for it to wrap in the middle of the copy**.

If instead you did `source = source + map_width` at the end of the loop, the
offset would keep growing and at some point the `memcpy` would cross the
segment boundary and copy from somewhere else.

## 9.4 The `died.wav` story

This is the bug of the project, and it is a perfect case study.

**The symptom:** the whole game worked in supernet mode except the death sound.
In the log:

```
Sound: could not load died.wav
```

**The first line of reasoning (wrong):** it does not fit. Make room.

61,440 bytes were recovered by fixing the sound DMA buffer, which was asking
for 69,632 bytes to use 4,096 (it asked to be aligned to a 64 KB boundary when
what is actually needed is *not crossing* one, which you get by asking for
twice the size).

Not enough. And the arithmetic said it **should have been plenty**: by the time
`died.wav` was reached there were **129,982 bytes free** and the file asks for
**42,090**.

**What was really happening:** memory was not short. The free memory was in the
wrong place.

The startup order was this:

```
  1. The MAP is allocated:            farmalloc(256000)
  2. Screen buffers, including the SPRITE SHEET (64000)
  3. Sprites are cut out
  4. The sheet is FREED:              free(64000)      <- leaves a HOLE
  5. WAV files are loaded:            farmalloc(42090) <- cannot find room
```

Drawn out, the heap looked like this:

```
   +----------------------------------------------------------+
   |   MAP 256000  | hole 64000  | screen  |  mask  |  free    |
   +----------------------------------------------------------+
                    ^^^^^^^^^^^^
                    free, but buried in the middle
```

The 129,982 free bytes were: 64,000 in that hole, and the rest up at the top.
But **the hole and the space at the top are not adjacent**. And
`farcoreleft()`, which is what measures it, only counts what is **above the
highest allocation**: it cannot see the hole underneath at all.

> **Total free memory is not the same as contiguous free memory.**
> This is called **fragmentation**, and in DOS it bites constantly.

**The second attempt (which broke the game):** load the sound *before* the map.
The sound loaded perfectly... and then **the map did not fit**. There were
247,371 contiguous bytes left and the map asked for 256,000. Short by 8,629.

Since `farmalloc` returned NULL and the code carried on with a null pointer,
what you saw was the background gone and the tanks drawn on top of garbage. The
log said so plainly, but you had to know how to read it:

```
Sound: loaded, 375680 bytes left        <- all four WAVs loaded
Map 640x400  memory now: near 246800    <- but init_graphics only used 128880
```

`init_graphics` consumed only 128,880 bytes when the map alone is 256,000. The
subtraction does not lie: the map was never allocated.

**The real fix:** instead of moving things around, **remove the hole**.

The sprite sheet was a 64,000 byte buffer holding the whole of `sprites.bmp`,
out of which the 24 sprites were cut at startup and which was then never read
again. It was removed entirely: `sprites.bmp` is now opened and each sprite is
read **straight from the file**, one row at a time.

```c
void bmp_extract_sprite(unsigned int src_x, unsigned int src_y,
                        unsigned int sprite_width, unsigned int sprite_height,
                        unsigned char *sprite_dest)
{
	unsigned int y;
	long file_offset;

	for (y = 0; y < sprite_height; y++){

		file_offset = 1078L
		            + ((long)(HEIGHT - 1 - (src_y + y)) * (long)WIDTH)
		            + (long)src_x;

		fseek(file_sprites_game_open, file_offset, SEEK_SET);
		fread(sprite_dest + (y * sprite_width), 1, sprite_width, file_sprites_game_open);

	}
}
```

(The `HEIGHT - 1 - (src_y + y)` is the BMP bottom-up flip again, done as one
subtraction rather than by reversing a whole buffer.)

The result:

- 64,000 bytes saved, which is more than `died.wav` was short by.
- **Nothing is ever freed**, so there is no hole and no fragmentation.
- The startup order stays exactly as it was, untouched.
- It costs about 340 `fseek` calls at startup and nothing ever again.

And the final layout:

```
  map       256,000  ->  315,344 left
  screen     64,000  ->  251,344 left
  mask       32,000  ->  219,035 left
  DMA         8,192  ->  210,843 left
  fire       25,699  ->  185,144 left
  engip1     55,472  ->  129,672 left
  engip2     57,470  ->   72,202 left
  died       42,090  ->   30,112 left   <- fits
```

## 9.5 The lessons

1. **Measure, do not assume.** Two lines of `coreleft()` and `farcoreleft()` in
   the log were worth more than all my reasoning about how the allocator ought
   to behave.
2. **Total free is not contiguous free.** This is the real lesson.
3. **Biggest allocation first**, on a clean heap. And if you can, do not free
   anything while running: in DOS a mid-game `free` is a hole that stays there.
4. **A silent failure costs more than a loud one.** `farmalloc` returned NULL,
   the code did a `printf` onto a screen that was in graphics mode (i.e.
   invisible) and carried on. That is why the symptom was "everything is
   broken" instead of "the map did not fit".

## 9.6 How to measure it yourself

```c
sprintf(log_message_text, "Memory at start: near %lu  far %lu",
        (unsigned long)coreleft(), (unsigned long)farcoreleft());
tanks_log(log_message_text);
```

At the start of `main()` and again after everything is allocated. Two lines that
turn an afternoon of guessing into a subtraction.

---

# PART 10 — TAKING THIS TO ANOTHER PROJECT

Suppose you have another DOS game with a fixed screen and you want to give it a
big map. These are the steps, in order, and none depends on the previous one
more than it has to.

## Step 1 — Sprite clipping, first of all

Before touching anything about cameras, fix your sprite drawing function:
**signed** coordinates and clipping.

Do it first because it **changes nothing at all** about current behaviour (if
nothing spills off the screen there is nothing to clip) and it is what saves
you from corrupting memory the moment you start subtracting a camera. You can
compile and confirm everything still works before going further.

## Step 2 — Separate world from screen in your head

Go through your code and classify every use of a coordinate:

- Does it decide something? (collisions, movement limits, hits) → **world**.
- Does it paint something? → **screen**, and it carries the camera
  subtraction.

This step is reading, not writing, and it is the one that prevents the weird
bugs.

## Step 3 — World variables, not constants

Wherever you have `WIDTH` and `HEIGHT` acting as "size of the map", change them
to variables:

```c
extern int map_width;
extern int map_height;
```

And leave `WIDTH`/`HEIGHT` meaning **only the screen**, for ever.

Be careful not to change them out of habit in the places that really are about
the screen: the VGA blit, the frame buffer, sprite clipping. Those are 320 and
200 eternally.

## Step 4 — The whole-world collision mask

If your game reads a collision map, pack it to one bit per pixel and load it
complete. If your game is single player with no network you could hold only the
visible part, but I would not: the complication is not worth the bytes.

## Step 5 — Movement limits

If your limits were the screen, they are now the map. Or, better still, drop
them and let the wall painted on the map stop the player. That is what this
game does: **the limit stopped being code and became data.**

If you do that, two warnings:

- **Keep the underflow guards.** The `if (position >= STEP)` tests before a
  subtraction are not screen limits, they are protection against an `unsigned`
  wrapping round to 65,535. With a properly drawn border they never fire, but
  they are there for the day you draw a map wrong.
- **Thick border**, 8 pixels minimum (part 7.7).

## Step 6 — The window and the camera

Now: `bmp_draw_world_window()`, `bmp_camera_follow()`, `bmp_camera_snap()` and
the clamp. About 80 lines in total, and by this point they hold no surprises.

## Step 7 — Measure memory before choosing the map size

Before deciding whether your map is 640x400 or 1280x800, add the two
`coreleft()`/`farcoreleft()` lines from part 9.6, compile, and look at the
number. It is the only thing that tells you what you can afford.

Remember the ceiling: the map at one byte per pixel plus the mask at one bit
per pixel. A 1280x800 world is 1,024,000 bytes of picture. Nowhere near fitting.

---

# PART 11 — REFERENCE

## 11.1 Globals

| Variable | Type | What it is |
|---|---|---|
| `map_width` | `int` | World width in pixels. 320 or 640 |
| `map_height` | `int` | World height in pixels. 200 or 400 |
| `camera_x` | `int` | Left edge of the window, in world. 0..(map_width-320) |
| `camera_y` | `int` | Top edge of the window, in world. 0..(map_height-200) |
| `buffer_original_background_bmp` | `unsigned char huge *` | The whole map. `farmalloc` |
| `buffer_background_image_data` | `unsigned char *` | The frame being built. Always 64,000 |
| `buffer_collision_mask` | `unsigned char *` | The world's walls, 1 bit per pixel |
| `file_sprites_game_open` | `FILE *` | `sprites.bmp`, open and never loaded |

## 11.2 Constants

| Constant | Value | What it is |
|---|---|---|
| `WIDTH` | 320 | **Screen** width. Never changes |
| `HEIGHT` | 200 | **Screen** height. Never changes |
| `SCREEN_SIZE` | 64000 | 320*200, the screen buffer |
| `MAP_MAX_WIDTH` | 640 | Only sizes the loader's one-row scratch buffer |
| `MAP_WALL_COLOR` | 252 | Palette index meaning wall |
| `CAMERA_DEAD_ZONE_X` | 100 | Horizontal dead zone margin. Max 150 |
| `CAMERA_DEAD_ZONE_Y` | 70 | Vertical dead zone margin. Max 90 |

## 11.3 Functions

### Startup

| Function | What it does |
|---|---|
| `bmp_init_buffers(width, height)` | Allocates everything and sets `map_width`/`map_height` |
| `bmp_fill_background_in_main_buffer(file)` | Loads the map picture |
| `bmp_fill_background_collision_in_buffer(file)` | Loads and packs the mask |
| `bmp_open_sprite_sheet(file)` | Opens `sprites.bmp` and leaves it open |
| `bmp_close_sprite_sheet()` | Closes it, once the sprites are cut |

### Camera

| Function | What it does |
|---|---|
| `bmp_camera_follow(x, y, w, h)` | Dead zone: moves the window only when needed |
| `bmp_camera_snap(x, y, w, h)` | Centres the window on the target, at once |

Both take **integers**, not a player pointer, so `bmp.c` does not depend on
your game's structures.

### Drawing

| Function | What it does |
|---|---|
| `bmp_draw_world_window(dest)` | Copies the visible window into the screen buffer |
| `bmp_extract_sprite(sx, sy, w, h, dest)` | Cuts a sprite out of the file |
| `draw_sprite_to_buffer(...)` | Paints a sprite with clipping. **Screen** coordinates, signed |
| `bmp_paint_image_data_to_vga(buffer)` | Blits the screen buffer to the VGA |

### Queries

| Function | Coordinates | What it does |
|---|---|---|
| `bmp_is_wall(x, y)` | **World** | 1 if wall. **The only valid one for deciding** |
| `bmp_get_map_pixel(x, y)` | **World** | Colour of the picture. Debugging only |
| `bmp_get_vga_pixel(x, y)` | **Screen** | What is in VGA memory, tanks already on top |

## 11.4 A complete frame, in order

```
  1. Read the keyboard / receive the other player's keys
  2. Move tanks and bullets            <- WORLD coordinates
  3. Check collisions                  <- bmp_is_wall(), WORLD coordinates
  4. update_camera(0)                  <- decides where the window is
  5. bmp_draw_world_window()           <- background
  6. draw_sprite_to_buffer() x N       <- everything else, world MINUS camera
  7. wait_retrace()
  8. bmp_paint_image_data_to_vga()     <- 64000 bytes to the screen
  9. Compute checksum and advance frame     <- WITHOUT the camera in it
```

---

# PART 12 — COMMON MISTAKES

The ones that actually happened, and what they look like.

### "The tank stops and cannot leave the first screen"

You left a movement limit checking against `WIDTH` instead of `map_width`. The
tank stops at x=302 and there is no way to reach the rest of the map.

### "The game hangs, or strange things happen minutes later"

`unsigned` coordinates in the sprite drawing function. A `-9` becomes `65527`
and you write a long way outside the buffer. With no memory protection in DOS,
the damage surfaces later and somewhere else.

### "The background is garbage and the tanks look horrible"

Some allocation returned NULL and the code carried on. Check the log: if
`init_graphics` consumed fewer bytes than it should have, something did not get
allocated.

### "Fine until I approach an edge, then it goes mad"

The clamp is missing, or missing a case. The window is reading outside the map.

### "The screen flickers between two views"

You are on the rooms model (part 1.3) and the player is right on the boundary.
You need hysteresis, or to switch to the dead zone.

### "Desync on frame 1 of every network game"

You put `camera_x` in the checksum. Each machine has its own and it is correct
for them to differ.

### "On the network the tanks do nonsensical things"

The two machines did not start in the same mode, or loaded different maps. The
checksum should call it out thanks to `map_width` (part 8.4).

### "The tank walks through a thin wall"

The wall is thinner than the tank's step (2) or the bullet's (3). Draw it 8
pixels at least.

### "I cannot get through a doorway"

The gap is the same size as the tank. It needs to be wider, because the tank
only occupies positions 2 apart and almost never lands on the one value that
fits.

### "The map's colours come out wrong"

Every BMP carries its own palette. If you load the palette from one file and
the picture from another with a different palette, you get the wrong colours.
All the BMPs in this game share a palette, byte for byte.

---

# GLOSSARY

**Bitmask.** Storing one yes/no per item using a bit rather than a byte. Eight
times less memory.

**Buffer.** A chunk of memory set aside to hold something. Here, images.

**Clamp.** Trimming a value so it stays inside a range. The camera is clamped so
the window cannot leave the map.

**Clipping.** Drawing only the part of something that falls on screen,
discarding the rest without writing it.

**Dead zone.** The rectangle in the middle of the screen inside which the player
can move without the camera reacting.

**Double buffering.** Building the frame in memory and blitting it in one go,
instead of painting directly onto what is being displayed. Stops flicker.

**Fragmentation.** Free memory being split into pieces separated by allocated
memory. There can be 130 KB free and no room for a 42 KB block.

**Huge (pointer).** A DOS pointer that is normalized on every arithmetic
operation, so it can walk blocks bigger than 64 KB without wrapping.

**Mode 13h.** The VGA graphics mode of 320x200 with 256 colours and one byte per
pixel. The one this game uses.

**Normalize (a pointer).** Readjusting segment and offset so the offset lands
between 0 and 15.

**Palette.** The table of 256 colours. Pixels do not hold a colour, they hold a
number from 0 to 255 which is a position in this table.

**Stride.** How many bytes to step forward in memory to go down one row in the
image. It is the width of the image, not of the screen, and confusing them is
the classic mistake.

**Vertical retrace.** The moment the monitor's beam returns to the top and is
not drawing. Blitting the buffer right then avoids seeing half an old frame and
half a new one.

---

# APPENDIX — WHAT WAS TESTED, AND HOW

This code was never compiled on the DOS machine to test it: it was verified
beforehand, on Linux, by compiling `bmp.c` as it stands and running it against
real data. It is worth knowing that you can do this.

| Test | What it checks | Result |
|---|---|---|
| Exhaustive clipping | An 18x18 sprite at 112,681 positions, (-40,-40) to (360,240), with guard bytes around the buffer | 0 writes outside |
| Clipping counts | Fully inside = 324 px; half off = 162; corner = 81; off = 0 | Correct |
| Collision mask | All 256,000 world pixels compared one by one against `bigcol.bmp` | 0 differences |
| Off the map | `bmp_is_wall()` in all four out-of-range directions | Returns wall |
| Dead zone | That it does not move inside, nor exactly on the edge, and pushes exactly the overshoot | Correct |
| Clamp | That it does not leave the map on any of the four sides | Correct |
| One screen map | That with 320x200 the camera stays pinned at (0,0) | Correct |
| Sprites from file | All 24 sprites compared byte for byte against the old method | 0 differences |
| Full walk | 200 frames of a tank walking the real map, with frames rendered to PNG | Camera still 88% of frames |

The rendered frames were especially useful: they let you **see** the result
without starting DOSBox, and that is how it was confirmed that a bullet fired
outside the window really does come in through the edge.
