# scroll — The endless scroll

*[Versión en español](README.md)*

**Effect 2 of 10 of the walkthrough.** Measured cost: **7.6 microseconds per frame**.

The code is in [`../scroll.c`](../scroll.c).

---

## What it does

The picture scrolls diagonally without stopping, and whatever leaves one side
comes back in the other. There is no beginning and no end: it behaves as if it
were glued to a cylinder in both directions.

## The idea

This is where you learn the technique **half the effects in this collection**
use: move a whole row with `memcpy` instead of pixel by pixel.

A shifted row is the same row cut in two pieces that swap places:

```
   image:   [ A A A A | B B B B B B B ]
   screen:  [ B B B B B B B | A A A A ]
                            ^ the cut is at 320 - shift
```

Two `memcpy` and the row is done. On an 8086 a `memcpy` is a `REP MOVSW`: two
bytes per instruction, and it never goes through a C loop at all.

## Step by step

**1. Two counters**, one horizontal and one vertical, that advance and wrap at
the edge.

**2. For each screen row, work out which image row it comes from:**

```c
	source_y = y + offset_y;
	if (source_y >= DEMO_HEIGHT){
		source_y = source_y - DEMO_HEIGHT;
	}
```

An `if` and not a `%`: the subtraction is one instruction, the modulo is a
division.

**3. The two `memcpy`:**

```c
	memcpy(screen + destination_row + offset_x,
	       image + source_row,
	       DEMO_WIDTH - offset_x);

	memcpy(screen + destination_row,
	       image + source_row + (DEMO_WIDTH - offset_x),
	       offset_x);
```

## The traps

**The shift has to stay between 0 and 319.** At 320 the first `memcpy` copies 0
bytes and the second copies 320 from `image + row + 0`, which happens to work,
but at 321 it runs off the buffer. That is why the counter wraps **before** it is
used, not after.

**A `memcpy` of 0 bytes is legal** and does nothing, which is exactly what is
needed when the shift is 0.

**The two speeds are not the same.** The vertical one advances once every
`SCROLL_Y_EVERY` frames. Equal speeds would trace a perfect diagonal that repeats
almost immediately and the loop would show.

## Cost

One of the cheapest: 400 `memcpy` per frame and nothing else. Not one per-pixel
operation.

## Experiments

1. Set `SCROLL_Y_EVERY` to 1 and watch the repetition become obvious.
2. Remove the second `memcpy`. You will see the black gap left by what scrolls
   out.
3. Change the wrap `if` into a `while`. Nothing changes, and that is the point:
   at these speeds one wrap is always enough.

---

**Previous:** [The palette turning](../../cycle/doc/README-EN.md) ·
**Next:** [The venetian blind](../../blinds/doc/README-EN.md) ·
**Index:** [The ten effects](../../README-EN.md)
