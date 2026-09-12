# zoom — Zooming in and out

*[Versión en español](README.md)*

**Effect 9 of 10 of the walkthrough.** Measured cost: **110 microseconds per frame**, and 144 before it was optimised.

The code is in [`../zoom.c`](../zoom.c).

---

## What it does

The picture breathes in and out, centred, with black around it when it is far
away.

## The idea

The technique changes here. Every effect so far moved **whole rows** with
`memcpy`. A zoom cannot: the row has to be **stretched or squeezed**, which means
deciding for each screen pixel which image pixel it comes from.

And the way to think about it is the opposite of what you would expect. You do not
take the image and scale it: **you walk the screen** and ask each pixel *"where do
I come from?"*. That way there are no holes: every screen pixel is filled exactly
once.

```
   scale 2.0 (far)               scale 0.5 (close)
   screen:  0 1 2 3 4 5          screen:  0 1 2 3 4 5
   image:   0 2 4 6 8 10         image:   0 0 1 1 2 2
             ^ skipping                    ^ repeating
```

## Step by step

**1. The frame's scale**, from a sine, between 0.6 and 2.2.

**2. Where the top-left corner lands.** Start at the centre of the image and back
off half a scaled screen, so the zoom is about the centre and not the corner:

```c
	start_u = ((long)(DEMO_WIDTH / 2) << DEMO_SHIFT) - (step * (DEMO_WIDTH / 2));
```

**3. Walk the screen** adding `step` to the source coordinate.

## The optimisation: the column table

And here is the thing worth learning.

**In a zoom with no rotation the source column depends ONLY on x.** Row 0 and row
199 read exactly the same 320 columns. The first version did that calculation
64000 times a frame **when there are 320 distinct answers**.

```c
	u = start_u;
	for (x = 0; x < DEMO_WIDTH; x++){
		sx = (int)(u >> DEMO_SHIFT);
		if ((unsigned int)sx < DEMO_WIDTH){
			column[x] = (unsigned int)sx;
			if (x < first){ first = x; }
			last = x;
		}
		u += step;
	}
```

320 calculations at the top of the frame, and the inner loop becomes:

```c
	for (x = first; x <= last; x++){
		screen[destination + x] = image[source_row + column[x]];
	}
```

Not one long addition, not one shift, not one bounds check. **From 144
microseconds per frame to 110.**

And `first` and `last` come out for free — where the visible part starts and ends —
so the black edges are filled with `memset` instead of pixel by pixel.

## The traps

**The comparison is `unsigned`** and does the work of two:

```c
	if ((unsigned int)sx < DEMO_WIDTH)
```

A negative `sx` seen as `unsigned` is an enormous number and fails the "less than
320" in one go. It is the standard trick for clipping with one comparison instead
of two.

**`step` can never be 0**, hence the `if (scale < 32)`. With a step of 0 every
column would read the same pixel: it would not hang, but you would get a screen of
one flat colour.

**A row entirely outside the image is painted with one `memset`**, with no
per-column checks. When the picture is far away, most rows are these.

## Cost

110 µs/frame, against 144 before the table. Still the second most expensive of the
ten: unavoidable, because it really does touch all 64000 pixels.

## Experiments

1. Take the table out and go back to computing `u >> 8` inside the loop. Measure.
2. Set `ZOOM_SCALE_AMP` to 0 and `ZOOM_SCALE_MID` to `DEMO_ONE`. The picture comes
   out identical to the original. If it does not, there is a half-pixel error
   somewhere.
3. Set `ZOOM_SCALE_MID` to `DEMO_ONE * 4`. You will see the picture tiny in the
   middle, and see how fast it runs when almost everything is `memset`.

---

**Previous:** [The pixelation that breathes](../../mosaic/doc/README-EN.md) ·
**Next:** [Rotate and zoom at once](../../rotozoom/doc/README-EN.md) ·
**Index:** [The ten effects](../../README-EN.md)
