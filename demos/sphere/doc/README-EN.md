# sphere — The picture wrapped round a spinning ball

*[Versión en español](README.md)*

**Effect 17 of 20 of the walkthrough.** Measured cost: **22 microseconds per frame**.

The code is in [`../sphere.c`](../sphere.c).

---

## What it does

A globe with your photograph for a map, spinning over black. It is the other
great table effect of the era alongside the tunnel, and it comes from the same
reasoning.

## The idea, which is the tunnel's

For every pixel of the screen you need two things about the ball:

```
   latitude    how high up the globe it falls  ->  asin(dy / R)
   longitude   where round it goes             ->  asin(dx / width_of_that_row)
```

Two arcsines per pixel, as impossible as the tunnel's square roots. And once
again: neither depends on time. They are worked out on the way in and the frame
comes down to an add and an AND.

```c
	screen[i] = texture[ (latitude[i] << 7) + ((longitude[i] + spin) & 127) ];
```

## But this costs half what the tunnel does, and that is the interesting part

The tunnel needs two 64000 byte tables. This one needs a single one, and the
reason is worth stopping on:

> **A pixel's latitude depends only on its row.**

Every pixel of row 40 is at the same height of the globe, whichever way you look
at it. So the latitude is **two hundred numbers**, one per row, not 64000.

```
   row 40  ----------------------  all at the same latitude
           \                    /
            \__________________/
```

The longitude does go per pixel, because it depends on how wide the globe is at
that height: near the pole, two pixels a short way apart span many degrees.

64600 bytes instead of 128000, and the inner loop also saves a table read and a
shift per pixel, because the texture row is worked out once per row. That is why
this costs 22 microseconds and the tunnel 77.

## The outline is settled once and never looked at again

The ball is a circle, so each row of the screen cuts it in **one continuous
piece**. Where each row starts and ends is stored, and the drawing loop never
asks whether it is inside: the outside is done with `memset`. It is the visible
span argument of the [rotozoom](../../rotozoom/doc/README-EN.md).

## The traps

**The pixel is not square**, just as in [cube](../../cube/doc/README-EN.md). A
circle of equal radius both ways comes out egg shaped. Here it is solved the
other way round from there: instead of shrinking what is drawn, **the `y` is
stretched before entering the maths** and everything else reasons as if it were a
real circle.

```c
	stretched = (dy * SPHERE_ASPECT_NUM) / SPHERE_ASPECT_DEN;
```

**The pole row has zero width.** `asin(dx / half)` would divide by zero. It is
handled separately: at the pole every longitude is the same, so the angle is 0
and that is that.

**The tail of the arcsine table looks like a mistake and is not:**

```
	..., 29, 29, 30, 32
```

Near 1 the arcsine goes vertical and a tiny change in the sine is many degrees.
On the ball that lands exactly on the rim, where the texture compresses to
nothing and nobody notices.

## Cost

Cheap for what it looks like. The ball covers about 24000 of the 64000 pixels and
the rest is `memset`; the inner loop is an add, an AND and two reads. It comes in
under [zoom](../../zoom/doc/README-EN.md), which has no third dimension at all.

## Experiments

1. Remove the `y` stretch. You get an egg, and the non-square pixel explains
   itself at a glance.
2. Change `(angle + SPHERE_QUARTER) * 2` to `(angle + SPHERE_QUARTER)`. The
   texture only uses half its height and the ball comes out with the picture
   squashed against the equator.
3. Set `SPHERE_SPIN` to 0. It stands still and you can see the sphere's
   distortion without the movement getting in the way.
4. Remove the `half == 0` case. Division by zero on the first pole row.

---

**Previous:** [The endless tunnel](../../tunnel/doc/README-EN.md) ·
**Next:** [Flying over the picture](../../voxel/doc/README-EN.md) ·
**Index:** [The twenty effects](../../README-EN.md)
