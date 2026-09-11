# Chapter 20 — The world stops being the screen

*[Versión en español](README.md)*

**This chapter is built so that something goes wrong.** On purpose.

```
make
chap20
```

The map becomes 640x400. The screen **does not move**. As soon as you go far you
lose sight of the tank and drive blind.

It is uncomfortable, and that is the point: **until you suffer the problem, a
camera looks like an ornament.**

---

## 1. The only thing that changes

```c
	bmp_init_buffers(640, 400);        /* was: (WIDTH, HEIGHT) */
```

That is it. From then on:

| | Value | What it is |
|---|---|---|
| `WIDTH` / `HEIGHT` | 320 / 200 | **The screen.** Never changes |
| `map_width` / `map_height` | 640 / 400 | **The world** |

Before they matched, so it did not matter which you used. **Now it does**, and
that is where the trouble starts.

## 2. The two coordinate systems

This is the central idea of the block. If you take one thing away, take this.

| | What it is | Range | Who uses it |
|---|---|---|---|
| **WORLD** | Where things really are | 0..639, 0..399 | **The whole game**: positions, bullets, walls, collisions |
| **SCREEN** | Where something is painted this frame | 0..319, 0..199 | **Only the drawing code** |

```
   THE WORLD, 640x400
   +--------------------------------------+
   |                                      |
   |   +--------------+                   |
   |   |    SCREEN    |                   |
   |   |              |         T         |  <- the tank is here,
   |   |              |                   |     alive and moving,
   |   +--------------+                   |     and you cannot see it
   |                                      |
   +--------------------------------------+
```

The same tank has **two coordinates at once**:

- a **world** one, unchanged even if you move the window
- a **screen** one, which changes with the window even if the tank does not move

### A worked example

| | |
|---|---|
| The tank is, in the world, at | **(500, 150)** |
| The window starts at | **(320, 100)** |
| So on screen it is painted at | (500−320, 150−100) = **(180, 50)** |

Now **move the window to (400, 100) without touching the tank**:

| | |
|---|---|
| The tank is still, in the world, at | **(500, 150)** |
| The window is now at | **(400, 100)** |
| On screen it is painted at | (500−400, 150−100) = **(100, 50)** |

The tank **has not moved a single pixel**, and it is drawn 80 pixels further
left. That is scrolling, and that is all it is.

In this chapter the window is pinned at **(0, 0)**, so world and screen
coincide… **as long as the tank is in the first quadrant**. Once it passes x=320
it is painted off screen and vanishes.

## 3. What happens if you mix the two systems

*"It is mistake number one"* is useless if you do not know **what it looks like**.
The concrete symptoms:

**Using screen coordinates for collisions**
The tank bumps into walls that are not there and walks through the ones that are.
And the baffling part: **it works perfectly near the origin**, because there the
two systems coincide. The bug only shows when you go far.

**Forgetting to subtract the camera for ONE object**
That object stays **glued to the screen** while everything else slides past. A
bullet that follows you everywhere instead of staying in the world. Very visual
and very easy to recognise once you have seen it.

**Subtracting the camera twice**
The object moves at **double speed** and in the opposite direction to what you
expect.

**Changing `WIDTH` to `map_width` where it does not belong**
Do it in the VGA blit and you push 256,000 bytes into a 64,000-byte screen and
trash memory. Do it in sprite clipping and sprites stop being clipped at the
right edge.

> `WIDTH` means **screen**, for good. In the VGA blit, in the frame buffer and in
> sprite clipping, **do not touch it**.

## 4. The game works. What does not work is looking at it

Note that movement and collisions **change nothing**:

```c
		moved = tut_try_move(&tank, MOVE_UP);
```

`tut_try_move()` asks `bmp_is_wall()`, which now knows a 640x400 world because
the mask was loaded from `bigcol.bmp`. **Everything is fine**: you move through
all four rooms, you bump into the right walls, you go through the doorways.

The only thing missing is deciding **which piece to look at**. And that is what
makes the match frustrating rather than broken.

## 5. What it costs in memory now

You multiplied the world by 4, so the question is immediate:

| | 320x200 | 640x400 |
|---|---:|---:|
| Map picture | 64,000 | **256,000** |
| Collision mask | 8,000 | **32,000** |
| Screen buffer | 64,000 | 64,000 (unchanged) |

The picture goes from fitting in an ordinary `malloc()` to **not remotely
fitting**: Turbo C's `malloc` maximum is 65,535 bytes.

That is why the map is requested with `farmalloc()` and its pointer is declared
`huge`. **Chapter 23** explains both and why those 256,000 bytes nearly sank the
project.

For now, note that the big map **is not free**, and on a 640 KB machine that
shows.

## 6. The limit stopped being code

There is no clamping against `WIDTH` or `HEIGHT` in the movement any more. What
stops the tank is the **wall painted on the map**.

> **The limit stopped being code and became data.**

It is more elegant and more flexible: change the map and the limits change, with
no rebuild.

With two warnings:

**The underflow guards stay.** The `if (position >= STEP)` tests before a
subtraction are **not screen limits**: they stop an `unsigned` wrapping round to
65,535. With a properly drawn border they never fire, but they are there for the
day you draw a map wrong.

**Thick border**, 8 pixels minimum. The tank moves 2 at a time, so it only
occupies even offsets from its origin: a 1-pixel wall can sit at a coordinate it
never lands on and **get walked through**. The big map has 16 to 33 pixels of
border.

## 7. Spawns have to be reviewed

```c
	player_reset(&tank, 128, 90, MOVE_UP);
```

Two lessons at once here.

The game's spawn for the big map is `BIG_PLAYER1_START`, at **(166, 299)**: the
bottom-left room. With the window pinned at (0,0), as it is in this chapter, that
point **cannot even be seen**: the screen only reaches y=199. You would start
with no tank.

So this chapter starts in room 0, inside the window, so you can **see** the tank
before losing it.

And the normal game's spawn, `PLAYER2_START_Y = 16`, would land **inside the big
map's border wall**, which is 17 pixels thick. The tank would be born trapped:
every direction blocked from the first frame, with no message at all. It would
look like the keyboard was broken.

Both say the same thing: **when you change worlds, review the spawns.**

## 8. Why it does not blow up

The tank is painted at its world coordinate as is, with nothing subtracted. Once
it passes x=320 it is painted off screen.

And it **does not blow up** because `draw_sprite_to_buffer()` clips, since
chapter 4.

Without that clipping, coordinate 400 with `unsigned` parameters would be writing
80 pixels past the end of every row, and the program would be corrupting other
people's memory right now. The symptom would not be an error: it would be the
game hanging five minutes later, somewhere else.

Hold on to that: **chapter 4 looked like excessive caution and here it is what
saves you.**

## 9. Experiments

1. **Go to the bottom-right corner** and look at what it prints on exit.
   Coordinates past 320 and 200: you were alive somewhere invisible.
2. **Count the steps** from the spawn to losing sight of the tank. With
   `PIXEL_TO_MOVE = 2` and the spawn at x=128, that is (320−128−18)/2 = **87
   frames**.
3. **Go back to `bmp_init_buffers(WIDTH, HEIGHT)`** and load `cutre.bmp`.
   Everything works again, because the world is the screen again. **The code has
   not changed**: only the two numbers.
4. **Use `PLAYER2_START_Y`** (the 16) instead of a big-map one. The tank is born
   inside the wall and cannot move. With no message.
5. **Comment out the clipping** in `draw_sprite_to_buffer()` in `src/bmp.c`,
   leave the first quadrant, and be ready to restart DOSBox. *(Remember to undo
   it.)*

## 10. What to take away

| | |
|---|---|
| **WORLD vs SCREEN** | The central idea of the block |
| `WIDTH`/`HEIGHT` are the screen **for good** | `map_width`/`map_height` are the world |
| `screen = world − camera` | One subtraction, that is all |
| Mixing them **works near the origin** and fails far away | Which is why it is so hard to find |
| The game works without a camera; **seeing it does not** | |
| The big map costs 256,000 bytes | Chapter 23 |
| When you change worlds, review the spawns | |

---

**Previous:** [Chapter 19](../../ch19/doc/README-EN.md) ·
**Next:** [Chapter 21 — The window](../../ch21/doc/README-EN.md)
