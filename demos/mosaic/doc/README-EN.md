# mosaic — The pixelation that breathes

*[Versión en español](README.md)*

**Effect 8 of 10 of the walkthrough.** Measured cost: **19.6 microseconds per frame**, and 78 before it was optimised.

The code is in [`../mosaic.c`](../mosaic.c).

---

## What it does

The picture breaks into huge squares where nothing can be made out, and works its
way down to real pixels. Then it goes back up.

## The idea

Each block takes the colour of the pixel at **its top-left corner**. No averages,
no means, nothing expensive: pick one colour and fill.

```
   image            block of 4
   a b c d          a a a a
   e f g h    ->    a a a a
   i j k l          a a a a
   m n o p          a a a a
```

And the fill is one `memset` per block row, not one pixel at a time.

## Step by step

**1. This frame's block size**, from a sine mapped to 1..40:

```c
	block = 1 + (((demo_sin(phase) + DEMO_ONE) * (MOSAIC_MAX_BLOCK - 1)) / (DEMO_ONE * 2));
```

**2. Walk the screen block by block**, clipping the last one in each row and
column, which almost never fits whole.

**3. Fill each block** with one `memset` per row of it.

## The optimisation, which is half of this document

The first version cost **78 microseconds per frame**, eleven times the cheap
effects. It now costs **19.6**. The change is six lines:

```c
	if (block == last_block){
		demo_show(screen);
		phase = (phase + MOSAIC_SPEED) & DEMO_ANGLE_MASK;
		continue;
	}
```

**If the block is the same size as last frame, the drawing would be identical and
it is already in `screen`.** There is nothing to do.

And the arithmetic is brutal: the size only changes about 40 times in the eight
seconds the effect runs, and at 70 frames a second that is 560 frames. **93% of
them were repainting, pixel by pixel, a result they already had in front of
them.**

And one more case:

```c
	if (block == 1){
		memcpy(screen, image, DEMO_SCREEN);
		...
	}
```

With one-pixel blocks there is no mosaic to speak of: it is the picture as it is,
and one `memcpy` does the work of 64000 single-byte `memset`.

## The traps

**`block` can never be 0.** The `1 +` in the calculation is there for that: a
block of size 0 is `for (x = 0; x < 320; x += 0)`, an infinite loop with the
screen frozen.

**`last_block` starts at −1**, not 0. It has to be a value that matches no
possible size, so the first frame always gets painted.

**40 divides both 320 and 200.** At the maximum size the screen comes out in exact
squares. With 41 there would be odd leftovers at the edges.

## Cost

19.6 µs/frame on average, against 78 before. The frames where the block changes
still cost the same; what disappeared are the other 520.

## Experiments

1. Remove the `if (block == last_block)` and measure. The whole lesson in one
   `diff`.
2. Raise `MOSAIC_MAX_BLOCK` to 100. You will see a frame made of four squares.
3. Make the block non-square: one size for the width, a different one for the
   height.

---

**Previous:** [The drop in the pond](../../ripple/doc/README-EN.md) ·
**Next:** [Zooming in and out](../../zoom/doc/README-EN.md) ·
**Index:** [The ten effects](../../README-EN.md)
