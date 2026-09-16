# carpet — The rotozoom that never ends

*[Versión en español](README.md)*

**Effect 14 of 19 of the walkthrough.** Measured cost: **73 microseconds per frame**.

The code is in [`../carpet.c`](../carpet.c).

---

## What it does

An endless floor of repeated photographs that turns, zooms and drifts. No edges,
no black background, filling all 64000 pixels always.

It is the "carpet" effect of the intros, and the idea comes from
[root42's rotozoom](https://github.com/root42/rotozoom).

## The idea

It is the [rotozoom](../../rotozoom/doc/README-EN.md) and the difference fits on
one line. Where that one asks *"have I left the picture?"* and paints black,
this one does `& 127` and comes back in the other side.

```
   rotozoom                       carpet
   +-----------------+            +-----------------+
   |     .-----.     |            |-.  |  .-----.  |-|
   |    /       \    |            |  \ | /       \ | |
   |   | PICTURE |   |            |--PICTURE-PICTURE|
   |    \       /    |            |  / | \       / | |
   |     '-----'     |            |-'  |  '-----'  |-|
   +-----------------+            +-----------------+
    one copy, black                repeats for ever
    all around                     (and the seam shows)
```

## Why it needs a texture and the picture will not do

To wrap round with an AND, the width has to be a **power of two**. The picture is
320x200 and neither number is one, so wrapping with it would cost two divides per
pixel: **128000 divides per frame**, which on a 386 means it does not start.

So on the way in the picture is shrunk to 128x128 by
[`demo_shrink_to_texture()`](../../demos.c), and from there the inner loop has
neither a divide nor a comparison.

16384 bytes and one shrink, in exchange for taking the divide out of the place
you go through 64000 times a frame.

## Step by step

**1. The four increments**, identical to the rotozoom's: the sum is linear, so
moving one pixel right always adds the same to `u` and `v`.

**2. The centre drifts**, which is what turns "a picture turning" into "a floor
you fly over". Two sines at different speeds, a Lissajous figure just like
[bounce](../../bounce/doc/README-EN.md).

**3. And the inner loop, which has no `if` at all:**

```c
	source = (unsigned int)((v >> 1) & 0x3F80L)
	       + (unsigned int)((u >> DEMO_SHIFT) & DEMO_TEXTURE_MASK);

	screen[destination + x] = texture[source];
```

No clipping, no visible span, no `memset` for the borders. Every pixel of the
screen lands inside the texture because the AND makes sure it does. The rotozoom
spends half a function working out where the visible part of each row starts and
ends; here that half function is not needed at all.

## The traps

**The bias, and it is the only real one.** The AND that wraps round only works on
positive numbers, and `u` and `v` go negative constantly: the top left corner of
the screen falls to the left of the centre of the texture nearly always.

In C89, **what a `>>` does to a negative number is up to the compiler**. Turbo C
shifts with sign and it would come out right, `gcc` too, and even so this is
exactly the kind of thing that works on the Linux bench and does something else
on the iron. So nobody gets asked:

```c
#define CARPET_BIAS 	((long)DEMO_TEXTURE_SIZE * (long)DEMO_ONE * 64L)
```

And it has to be an **exact multiple** of the texture size in fixed point
(128 * 256 = 32768), or it would shift the carpet half a tile. 2097152 is
32768 x 64 exactly.

**One shift fewer.** The texture row is worked out in two operations instead of
three:

```
	((v >> 8) & 127) << 7   ==   (v >> 1) & 0x3F80
```

Going down 8 and up 7 is going down 1, and the seven dirty low bits that leaves
are cleared by the same mask moved seven places along. Checked over all 85715
values of the real range: zero differences. One shift fewer per pixel, and there
are 64000 pixels. It took 24% off the cost of the effect.

And **it only works because `v` is always positive**, which is what the bias
buys.

## The price: the seam shows

The picture does not fit against itself, so where it repeats the join shows.
That is not a fault: it is what this effect does and what the original did. What
is on screen is a mosaic, and a mosaic has joins.

## Cost

The dearest of the folder after the [tunnel](../../tunnel/doc/README-EN.md), for
a healthy reason: together with that one it is the only effect that **writes all
64000 pixels every frame**. The rotozoom looks heavier and saves most of the
screen with `memset`.

If it comes up short on your machine, root42's original rendered at 320x100 and
doubled the rows. You lose half the vertical detail and gain half the time.

## Experiments

1. Take both `& DEMO_TEXTURE_MASK` away. You will watch it read outside the
   texture: the colours of whatever memory sits behind it.
2. Set `CARPET_BIAS` to 0. Under `gcc` you will notice nothing; this is exactly
   the sort of thing that breaks on the iron and not on the bench.
3. Change it to `2097152 + 1`. The carpet slides one pixel per turn and "exact
   multiple" starts to mean something.
4. Set `CARPET_SCALE_AMP` to `DEMO_ONE * 2`. So many tiles show that the seam
   becomes the effect.

---

**Previous:** [Rotate and zoom at once](../../rotozoom/doc/README-EN.md) ·
**Next:** [The endless tunnel](../../tunnel/doc/README-EN.md) ·
**Index:** [The nineteen effects](../../README-EN.md)
