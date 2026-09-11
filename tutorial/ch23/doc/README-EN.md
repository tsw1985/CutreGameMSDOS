# Chapter 23 — The bit mask and DOS memory

*[Versión en español](README.md)*

**The last chapter of the big map block.** It covers the piece that makes the
big map **fit**, and the most expensive lesson of the whole project.

```
make
chap23
```

In text mode, so you can read the numbers.

---

## 1. The budget

Real-mode DOS has **640 KB**, and everything comes out of it: the system, the
drivers, the network TSRs, your code, your stack and everything you allocate.

On this project's test machine there were **571,344 bytes** free at startup. That
is the entire budget.

`coreleft()` and `farcoreleft()` tell you. In Turbo C's huge model they return the
same thing: **they are the same pool**.

## 2. The 64 KB ceiling

`malloc()` takes a `size_t`, which in Turbo C is **16 bits**. The largest number
that fits is **65,535**.

```c
	malloc(64000);    /* fine */
	malloc(256000);   /* impossible: you cannot even ask */
```

For bigger blocks there is `farmalloc()`, which takes an `unsigned long`. That is
why the map is requested with `farmalloc` and everything else with `malloc`.

## 3. And the pointer has to be `huge`

On the 8086 an address is two 16-bit numbers:

```
   physical = segment * 16 + offset
```

`far` pointer arithmetic **only touches the offset**. And the offset is 16 bits,
so past 65,535 it **wraps to zero** instead of carrying into the segment.

In a 64,000-byte buffer it does not matter, you never get there. In a
256,000-byte one you wrap four times and read garbage.

A **`huge`** pointer is **normalized** on every operation: the compiler adjusts
segment and offset so the offset always stays between 0 and 15.

```c
extern unsigned char huge *buffer_original_background_bmp;
```

And that is where a detail of `bmp_draw_world_window()` comes from that otherwise
looks absurd: the address is **rebuilt from the base** on every pass of the loop
instead of accumulating. Rebuilding it makes Turbo C normalize it, and then the
320-byte `memcpy` that follows cannot cross the segment boundary.

## 4. One bit per pixel

The collision map has to cover the **whole** world, not just what is visible: on
the network both machines simulate both tanks, so your machine has to know whether
the other player's tank, in a room you cannot see, has hit something.

At one byte per pixel that is **256,000 bytes**. It does not fit.

But look at what that map is ever asked:

```c
	if (bmp_is_wall(x, y) == 1)
```

**There are only two possible answers.** Of the 256 values that fit in a byte you
care about one. You are spending 8 bits on a yes/no.

```
   One byte per pixel (8 pixels = 8 bytes):
     [00] [00] [FF] [FF] [00] [00] [00] [FF]

   One bit per pixel (8 pixels = 1 byte):
     [ 00110001 ]
```

| | Bytes |
|---|---|
| 640x400 at 1 byte/px | 256,000 |
| 640x400 at **1 bit/px** | **32,000** |

And note carefully: that is **half** what the collision map of a **single screen**
used to cost (64,000). The whole world takes less room than one screen did.

## 5. How a bit is read

Here is the code, which is the thing to see:

```c
int bmp_is_wall(int x, int y){

	unsigned long bit_index;
	unsigned int  byte_index;
	unsigned char bit;

	/* off the map counts as wall: a safety net */
	if (x < 0 || y < 0 || x >= map_width || y >= map_height){
		return 1;
	}

	bit_index  = ((unsigned long)y * (unsigned long)map_width) + (unsigned long)x;
	byte_index = (unsigned int)(bit_index >> 3);
	bit        = (unsigned char)(1 << (unsigned int)(bit_index & 7L));

	if ((buffer_collision_mask[byte_index] & bit) != 0){
		return 1;
	}

	return 0;

}
```

Step by step, with pixel **(100, 50)** of a 640-wide world:

| Step | What it does | With the numbers |
|---|---|---|
| `bit_index` | Which pixel number it is, from the start | 50·640 + 100 = **32,100** |
| `>> 3` | Divide by 8: **which byte** it is in | 32,100 / 8 = **4,012** |
| `& 7` | The remainder: **which bit** inside that byte | 32,100 % 8 = **4** |
| `1 << 4` | Build a mask with just that bit | `00010000` |
| `& ` | Test whether it is set | |

### Why `>> 3` and `& 7` rather than `/ 8` and `% 8`

They do exactly the same, but:

- `>> 3` is **a shift**: one instruction
- `/ 8` is **a division**: tens of cycles on an 8086

Turbo C probably turns `/8` into `>>3` by itself, but writing it this way makes
clear **it is a bit operation**, not arithmetic.

Same with `& 7` instead of `% 8`. And it works because 8 is a power of two: the
bottom three bits of the number **are** the remainder of dividing by 8.

### And the multiply is not expensive either

```c
	bit_index = (unsigned long)y * (unsigned long)map_width + x;
```

That looks like a 32-bit multiply, which on an 8086 would be a library call. It is
not:

The 8086 has a **`MUL`** instruction that multiplies **two 16-bit numbers into a
32-bit result**, in a single instruction. `y` fits in 16 bits, `map_width` does
too, and the result needs 32. It is exactly the case that instruction solves.

The compiler uses it. So the total cost of `bmp_is_wall()` over reading a plain
byte is: **one `MUL`, one shift and two ANDs.**

And it is called 3 times per tank per frame plus once per bullet: **8 calls a
frame**. Negligible.

## 6. Off the map counts as wall

```c
	if (x < 0 || y < 0 || x >= map_width || y >= map_height){
		return 1;
	}
```

Those four lines are not a game rule: they are a **safety net**.

The maps are drawn with a solid border 16 to 33 pixels thick, so it should never
come up. But if you ever draw a map with a hole in the border, the tank **stops
dead** instead of the game reading memory that is not ours and hanging twenty
seconds later for an incomprehensible reason.

Same philosophy as chapter 4's clipping: **let bad data produce odd but bounded
behaviour**, not silent corruption.

## 7. The expensive lesson: fragmentation

During development, the game loaded everything **except the last sound effect**:

```
Sound: could not load died.wav
```

And the arithmetic said it **should have fitted**: there were **129,982 bytes
free** and the file asks for **42,090**.

Memory was not short. **The free memory was in the wrong place.**

The startup order was:

```
  1. farmalloc(256000)   the map
  2. malloc(64000)       the sprite sheet
  3. cut out the sprites
  4. free(64000)         release it       <- LEAVES A HOLE
  5. farmalloc(42090)    the WAV          <- cannot find room
```

```
   +----------------------------------------------------+
   |  MAP 256000  |  hole 64000  | screen |  free       |
   +----------------------------------------------------+
                   ^^^^^^^^^^^^
                   free, but buried in the middle
```

Of the 129,982 free, 64,000 were in that hole and the rest up at the top. **And
they are not adjacent.**

> ## TOTAL FREE MEMORY IS NOT CONTIGUOUS FREE MEMORY.

## 8. The attempt that made it worse

The first fix was to load the sound **before** the map. The sound loaded
perfectly… **and then the map did not fit**: 247,371 contiguous bytes left, and it
asked for 256,000. Short by 8,629.

And since `farmalloc` returned NULL and the code carried on with a null pointer,
the result was the background gone and the tanks drawn over garbage.

The log said so plainly, but you had to know how to read it:

```
Sound: loaded, 375680 bytes left        <- all four WAVs loaded
Map 640x400  memory now: near 246800    <- but init_graphics only used 128880
```

`init_graphics` consumed 128,880 bytes when the map alone is 256,000. **The
subtraction does not lie: the map was never allocated.**

## 9. The real fix: remove the hole

Not moving things around. **Removing the hole.**

The sprite sheet was a 64,000-byte buffer the sprites were cut out of at startup
and which was never read again. It was removed entirely: `sprites.bmp` **is opened
and each sprite read straight from the file** (which is what you saw in chapter
4).

- 64,000 bytes saved
- **Nothing is ever freed**, so there is no hole and no fragmentation
- It costs about 340 `fseek`s at startup and nothing ever again

## 10. The four rules

1. **Measure, do not assume.** Two lines of `coreleft()` in the log were worth
   more than all the reasoning about how the allocator ought to behave.
2. **Total free is not contiguous free.** The real lesson.
3. **Biggest allocation first**, on a clean heap. And if you can, do not free
   anything while running.
4. **A silent failure costs more than a loud one.** `farmalloc` returned NULL, the
   code did a `printf` onto a screen in graphics mode (i.e. invisible) and carried
   on. Which is why the symptom was *"everything is broken"* instead of *"the map
   did not fit"*.

## 11. Experiments

1. **Look at the numbers it prints.** Compare them with the 571,344 in the text.
2. **Ask for a 1280x800 map** in `bmp_init_buffers()`. That is 1,024,000 bytes:
   `farmalloc` returns NULL and you will see it.
3. **Add `coreleft()` to your own programs.** It is the habit that saves the most
   time in DOS.

## 12. What to take away

| | |
|---|---|
| `malloc` stops at 65,535 | For more, `farmalloc` |
| A `far` pointer **wraps** at 64 KB | For more, `huge` |
| **1 bit per pixel**: the whole world in 32 KB | Less than one screen used to cost |
| **Total free ≠ contiguous free** | The expensive lesson |
| Biggest first; better still, free nothing | |
| **Measure, do not assume** | |

---

## And that is the big world complete

Block 5 is closed with this: the world, the window, the camera and the memory
that makes it all fit.

One thing is left, and it follows straight from the camera: if each machine
follows its own tank, **how do you find the other one?** That is
[chapter 24](../../ch24/doc/README-EN.md), which puts a number on screen drawn
with sprites.

To go deeper:

- [Camera manual](../../../doc/EN/CAMERA-MANUAL.md) — 60 pages on this block
- [Network manual](../../../doc/EN/NETWORK-MANUAL.md) — the same for chapters 15-19
- [Sound tutorial](../../../doc/EN/SOUND-TUTORIAL.md) — for reusing `sound.c`
- [Network tutorial](../../../doc/EN/NETWORK-TUTORIAL.md) — for reusing `net.c`

---

**Previous:** [Chapter 22](../../ch22/doc/README-EN.md) ·
**Next:** [Chapter 24](../../ch24/doc/README-EN.md) ·
**Index:** [The course](../../README-EN.md)
