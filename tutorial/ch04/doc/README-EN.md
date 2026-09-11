# Chapter 4 — Sprites: cutting and drawing with transparency

*[Versión en español](README.md)*

**What you will get:** tanks and a bullet on top of the map, and a tank walking
off the edges without breaking anything.

**Which real code is used:** `src/bmp.c`, `header/players.h`.

```
make
chap04
```

---

## 1. One image for every drawing

Every sprite in the game lives in a single file, `res/sprites.bmp`, in a grid:

```
   +--------------------------------------------------+
   | tank1 up | tank1 down | ... | explosion          |
   +--------------------------------------------------+
   | tank2 up | tank2 down | ... | bullet             |
   +--------------------------------------------------+
```

For two reasons:

1. **One file is opened once**, not thirty times.
2. **They all share a palette by construction.** If each sprite were its own BMP
   with its own palette, you could not mix them on screen.

## 2. Cutting one out

```c
bmp_extract_sprite(2, 5, TANK_WIDTH, TANK_HEIGHT, tank_up);
```

Takes an 18x18 rectangle starting at (2,5) of the sheet and copies it into
`tank_up`, **packed**: in the destination the rows follow each other 18 apart,
not 320.

```
   In the SHEET (320 wide):          In the DESTINATION (18 wide):

   ....XXXXXXXX................       XXXXXXXX
   ....XXXXXXXX................       XXXXXXXX
   ....XXXXXXXX................       XXXXXXXX
       ^ the sprite
```

`TANK_WIDTH` and `TANK_HEIGHT` come from `header/players.h`, the real header. No
numbers copied by hand.

**Not every sprite is the same size**: the tank is 18x18, the bullet 4x3, the
explosion 13x13. That is why width and height are parameters.

## 3. The surprising detail: the sheet is not in memory

Note it is called `bmp_open_sprite_sheet()`, not "load".

The game **opens the file and reads each sprite straight off disk**, one row at
a time. The full sheet (64,000 bytes) never exists in memory.

It was not always so. It changed for a very specific memory-management reason
laid out in the [camera manual, part 9.4](../../../doc/EN/CAMERA-MANUAL.md). The
short version: those 64,000 bytes lived alongside the 256,000-byte map, and
releasing them left a hole in the middle of the heap that stopped the last sound
from loading.

## 4. Transparency is colour 0

A sprite is a rectangle, but a tank is not. What happens at the corners?

```c
	pixel = sprite[src_offset];
	if(pixel != 0) {
		dest_buffer[dest_offset] = pixel;
	}
```

**Colour 0 does not get painted.** It is skipped, and whatever was underneath
stays.

That is why palette index 0 is reserved in this game and never used to draw
anything real: it is the "colour" that means "nothing here".

It is what lets the map show through the corners of the tank instead of a block
of background.

## 5. Clipping, and why it matters so much

The second half of the program walks the tank from one edge to the other,
**spilling off both sides**, with negative coordinates and coordinates past 320.

Without clipping that is catastrophic. And not for the reason you would think:

```c
	/* The OLD version, with unsigned parameters */
	unsigned int dest_x
```

If the tank is at x = -9 and `dest_x` is a 16-bit `unsigned int`, **-9 is not
-9: it is 65527**. And then:

```
   dest_offset = row * 320 + 65527
```

That is about 65,000 bytes outside the screen buffer. And DOS has **no memory
protection**: the write happens and flattens whatever is there. It could be
another buffer, your own code, or the interrupt vector table.

The symptom is not an error: the game hangs five minutes later, somewhere
unrelated.

The fix is two things:

1. **Signed parameters** (`int dest_x`), so -9 is -9.
2. **Work out which part of the sprite lands on screen** before the loops, and
   walk only that.

Details in the [camera manual, part 4](../../../doc/EN/CAMERA-MANUAL.md).

## 6. Why `int` and not `long`

A question that comes up by itself: if coordinates can be negative, why not use
`long` and stop worrying?

Because the 8086 **has no 32-bit arithmetic**. Every `long` operation becomes
several instructions, and multiplies and divides become library calls.

A Turbo C `int` reaches **32,767**. The biggest world in the game is 640 wide.
That is 32,000 to spare.

## 7. Experiments

1. **Change `if(pixel != 0)` to `if(pixel != 15)`.** Now white is transparent and
   the tank comes out with a black box around it.
2. **Cut at the wrong coordinates**, say `bmp_extract_sprite(10, 10, ...)`. You
   get a sliced tank with bits of its neighbour. That is how you see the sheet is
   a grid and the coordinates have to be exact.
3. **Cut 40x40 instead of 18x18.** You take the sprite and its neighbours with it.
4. **Comment out `bmp_close_sprite_sheet()`.** Nothing breaks, but you leave a
   file open. DOS hands out few handles; twenty of those and you run out.

## 8. What to take away

| | |
|---|---|
| Every sprite in one sheet | One file and one palette |
| Cutting = copying with repacking | In the destination the stride is the sprite's |
| **Colour 0 is not painted** | That is transparency |
| **Signed coordinates + clipping** | Without it, silent memory corruption |
| `int`, not `long` | The 8086 has no 32-bit arithmetic |

---

**Previous:** [Chapter 3](../../ch03/doc/README-EN.md) ·
**Next:** [Chapter 5 — The keyboard](../../ch05/doc/README-EN.md)
