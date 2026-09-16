# lens — The magnifying glass wandering over the picture

*[Versión en español](README.md)*

**Effect 12 of 19 of the walkthrough.** Measured cost: **9.4 microseconds per frame**.

The code is in [`../lens.c`](../lens.c).

---

## What it does

A circle of glass travels over the image and magnifies whatever it catches
underneath. A classic of the 90s and of the screensavers of the time, and one of
the few effects that leaves the **whole** picture visible the entire time: the
rest of the screen is never touched.

## The idea: the lens does not move, the picture does

What a lens distorts depends only on **where the pixel is inside the lens**, not
on where the lens happens to be. The centre pixel always reads from the centre,
the rim pixel always reads from the rim.

So the displacement of every point of the glass is worked out once, on the way
in, and stored as an offset in bytes:

```c
	lens_delta[i] = (source_dy * 320) + source_dx;
```

And moving the lens is adding the centre's position to all of it. Each frame is
6100 adds and 6100 reads, with no square root and no divide.

It is the [tunnel](../../tunnel/doc/README-EN.md) idea — precompute what does not
change — applied to a surface of 6100 pixels instead of the screen's 64000. That
is why the table is 12K and not 128.

## The shape of the glass

```
	source = r * (1 - k * (1 - (r/R)^2))
```

At the rim (`r = R`) the bracket is 1 and it **displaces nothing**, so the lens
blends into the picture with no seam. At the centre (`r = 0`) it magnifies by
`1/(1-k)`, which at `k = 0.4` is 1.67.

```
   r/R:     0.0   0.25   0.5   0.75   1.0
   source:  0.0   0.16   0.35  0.62   1.0     <- lands exactly on the rim
                                    ^ no jump here
```

That last column is what matters. A lens that jumps at the rim reads as a cutout
stuck on top, not as glass.

## Step by step

**1. The table, walking the square that contains the circle.** For the points
inside it stores where they read from; for the rows it stores **where the useful
part starts and ends**, which leaves the drawing loop without a single *am I
inside?*. It is the visible span trick of the
[rotozoom](../../rotozoom/doc/README-EN.md).

**2. The integer root, here the brute force way:**

```c
	radius = 0;
	while ((radius + 1) * (radius + 1) <= squared){
		radius++;
	}
```

That is 6100 points **once in the whole life of the effect**, so bringing over
the tunnel's bit by bit method is not worth it. It is the one place in the folder
where the slow version is the right one.

**3. And the wander**, two sines at coprime speeds, like
[bounce](../../bounce/doc/README-EN.md).

## The traps

**The source is counted from the CENTRE of the lens, not from the pixel being
painted.** This cost a segmentation fault on the bench: `lens_delta` stores
*where the point (lx,ly) of the glass reads from*, and that where-from is
measured against the centre. Adding it to the current pixel doubles everything
and runs off the bottom of the buffer — the sanitiser caught it reading 87 bytes
past the end of the image.

```c
	screen[destination] = image[center_offset + (unsigned int)delta[index]];
	/*                          ^^^^^^^^^^^^^  and not destination */
```

**The centre stays one radius away from the edges ALWAYS:**

```c
	range_x = (DEMO_WIDTH  / 2) - LENS_RADIUS - 1;
```

and with that the source can never leave the buffer, which is what lets the
drawing loop skip bounds checks. The alternative would be an *am I inside?* per
pixel, 6100 times a frame. Here the clipping is settled **once, in the walk**,
not in the drawing.

## Cost

One 64000 byte `memcpy` for the picture and 6100 loose pixels. The `memcpy` is
the expensive part: the lens itself costs a tenth of it.

## Experiments

1. Set `LENS_STRENGTH` to 0. The lens disappears although the code still runs:
   `factor` is 256 and every point reads from itself.
2. Raise it to 200. It magnifies 4.6 times and the pixels go blocky.
3. Change the formula to `source_radius = (radius * radius) / LENS_RADIUS`. It
   still does not jump at the rim and magnifies infinitely at the centre: a water
   drop rather than a lens.
4. Raise `LENS_RADIUS` to 90 without touching `range_x`. The centre gets too
   close to the edge and there you see what that `- LENS_RADIUS - 1` is for.

---

**Previous:** [Zooming in and out](../../zoom/doc/README-EN.md) ·
**Next:** [Rotate and zoom at once](../../rotozoom/doc/README-EN.md) ·
**Index:** [The nineteen effects](../../README-EN.md)
