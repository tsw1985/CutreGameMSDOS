# Chapter 21 — The window: taking a piece out of a big map

*[Versión en español](README.md)*

**What you will get:** moving the window by hand across the 640x400 world, with a
stationary tank, and understanding exactly what a window is before anything moves
it for you.

**Which real code is used:** `src/bmp.c` (`bmp_draw_world_window`),
`src/players.c`.

```
make
chap21
```

Arrows: move **the window**. SHIFT to go fast. ESC to quit.

---

## 1. There is no camera here

This matters for reading the chapter properly.

**The arrows do not move any tank.** They move `camera_x` and `camera_y`
directly. The tank is pinned in the world at **(228, 140)** and does not move at
any point in the program:

```c
	tank.position_x = TANK_WORLD_X;    /* and nobody touches it again */
	tank.position_y = TANK_WORLD_Y;
```

**And you are still going to see it slide across the screen.**

That apparent contradiction is the whole lesson. One thing is **where** something
is and another is **where it is painted**.

## 2. A reminder from chapter 1: the stride

In chapter 1 we said a pixel is at:

```
   position = y * 320 + x
```

and that **the 320 has a name: the stride**, *how many bytes to step forward to
go down one row*. And I warned that here it would stop matching the screen width.

The moment has come.

An image is not stored as a rectangle. It is stored as **a strip of bytes**, row
after row:

```
  memory:  [ row 0 (640 bytes) ][ row 1 (640 bytes) ][ row 2 ] ...
```

For the 640x400 map the stride is **640**. For the screen it is still **320**. Two
different images with two different row steps, and that is where the difficulty
lies.

## 3. Why a `memcpy` no longer works

Up to chapter 20, the background was copied like this:

```c
	memcpy(destination, map, 64000);
```

It worked because the map's 64,000 bytes were, **in the same order**, the
screen's 64,000 bytes. Map row 0 → screen row 0. All contiguous.

Now look at what you want to copy: a window **320 wide** inside a map **640
wide**.

```
  THE WORLD, 640 wide:

  row 100:  ....................[XXXXXXXXXXXXXXXX]....................
  row 101:  ....................[XXXXXXXXXXXXXXXX]....................
  row 102:  ....................[XXXXXXXXXXXXXXXX]....................
                                 ^                ^
                                 camera_x         camera_x + 320


  AND IN MEMORY, those three rows sit this far apart:

  [...320... XXXX ...320...][...320... XXXX ...320...][...320... XXXX ...]
             ^ I want this            ^ I want this            ^ I want this
             |<----------- 640 bytes ----------->|
```

**The pieces you want are not next to each other.** Between the end of one and
the start of the next there are **320 bytes you do not want**.

A single `memcpy` does not know how to skip them. It copies contiguous bytes,
full stop.

## 4. The answer: one row at a time

If you cannot copy it in one go, you copy it in **200 goes**: one per screen row.

This is `bmp_draw_world_window()` from `src/bmp.c`, in full:

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

Line by line:

| | |
|---|---|
| `row` | Goes 0 to 199: the 200 rows of **the screen** |
| `camera_y + row` | Turns it into its **world** row. Camera at y=100 makes screen row 0 world row 100 |
| `* map_width` | Skips that many **whole world rows**, 640 bytes each |
| `+ camera_x` | Steps along the row to where the window starts |
| `memcpy(..., WIDTH)` | Copies 320 bytes: **one complete screen row** |
| `destination_offset += WIDTH` | Advances 320 in the destination, where rows **are** contiguous |

## 5. It is not more work

This is surprising: **exactly the same bytes get copied as before.**

```
   200 rows x 320 bytes = 64,000 bytes
```

The same 64,000 as the old `memcpy`. It is not more work: it is **the same work
split into 200 calls** instead of one.

The only extra cost is the overhead of calling `memcpy` 200 times rather than
once, and on a 486 that does not show.

## 6. A worked example

Camera at **(320, 100)**. You want screen row **0**.

```
   world row       = camera_y + row = 100 + 0   = 100
   row skip        = 100 * 640                  = 64,000
   step along row  = camera_x                   = 320
   -------------------------------------------------------
   position in the map                          = 64,320
```

You copy 320 bytes from byte 64,320 of the map, into byte 0 of the screen.

Now row **1**:

```
   world row  = 100 + 1 = 101
   row skip   = 101 * 640 = 64,640
   + 320
   ------------------------------
   position   = 64,960
```

Note: **64,320 → 64,960 is a 640-byte jump**, even though you only copied 320.
That 320-byte gap is the part of the world to the right of your window.

## 7. The odd detail: `source` is rebuilt in full each pass

Look at the loop again. The address is computed **from scratch** each time,
instead of `source = source + map_width` at the end, which would be the natural
thing.

It looks wasteful. **It is deliberate**, and it has to do with how pointers work
in DOS.

The short version: a `far` pointer can only walk 64 KB before wrapping, and the
map takes 256,000 bytes. Rebuilding the address from the base forces the compiler
to **normalize** the pointer, and a normalized pointer can never run off its
segment when you add 320 to it.

If you accumulated it instead, at some point the `memcpy` would cross a segment
boundary and copy from somewhere else.

**Chapter 23 explains it in full**, including what a `huge` pointer is and why the
map is declared that way. For now, take it that the full `source =` is not
clumsiness.

## 8. The clamp, by hand

In this chapter you write it yourself:

```c
	if (camera_x < 0){ camera_x = 0; }
	if (camera_y < 0){ camera_y = 0; }
	if (camera_x > map_width  - WIDTH ){ camera_x = map_width  - WIDTH;  }
	if (camera_y > map_height - HEIGHT){ camera_y = map_height - HEIGHT; }
```

| | Window range |
|---|---|
| `camera_x` | 0 … 640-320 = **320** |
| `camera_y` | 0 … 400-200 = **200** |

`map_width - WIDTH` is the furthest right the window can sit without its right
edge leaving the map.

**Without this**, `bmp_draw_world_window()` would read from before the start of
the map or past its end. And in DOS that is not an error: it draws garbage, or
hangs.

Remove it and go to a corner: you will see it.

## 9. What the program proves

On exit it prints three lines:

```
  The tank, in the WORLD   : (228, 140)   <- never changed
  The window               : (128, 104)
  The tank, on the SCREEN  : (100, 36)
```

The tank **has not moved**. The only thing that changed was the window. And yet
you watched it cross the whole screen.

If the screen one goes outside 0..319 / 0..199, the tank was outside the window
and you could not see it — **and it was still in exactly the same place in the
world**.

## 10. Experiments

1. **Go to a corner and note the three numbers.** Check the subtraction by hand.
2. **Remove the clamp** and go off the top left. Garbage on screen: you are
   reading from before the map.
3. **Change `* map_width` to `* WIDTH`** in `bmp_draw_world_window()` (in
   `src/bmp.c`, then undo it). The wrong stride: the image comes out slanted and
   repeated. **That is what a stride bug looks like**, and it is worth having
   seen it once.
4. **Change `memcpy(..., WIDTH)` to `memcpy(..., 160)`.** Half the screen keeps
   the previous frame.
5. **Fix `camera_x = 320`** and do not move it. You are seeing the right half of
   the world, permanently. That is a fixed "room", the model that was rejected.

## 11. What to take away

| | |
|---|---|
| **The stride is THE IMAGE's width**, not the screen's | Here they stop matching |
| The window's rows are **not contiguous** in memory | Which is why a `memcpy` will not do |
| **200 `memcpy`s of 320 bytes** | The same 64,000 bytes, spread out |
| `source` is rebuilt in full on purpose | `huge` pointers, chapter 23 |
| The clamp keeps the window inside the map | Without it you read outside |
| **Moving the window does not move the world** | The tank never moved |

---

**Previous:** [Chapter 20](../../ch20/doc/README-EN.md) ·
**Next:** [Chapter 22 — The camera](../../ch22/doc/README-EN.md)
