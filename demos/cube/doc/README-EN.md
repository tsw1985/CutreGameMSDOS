# cube — The wireframe cube

*[Versión en español](README.md)*

**Effect 18 of 19 of the walkthrough.** Measured cost: **4.7 microseconds per frame**.

The code is in [`../cube.c`](../cube.c).

---

## What it does

A wireframe cube turning on two axes, over the picture.

It is the first effect in the folder that is **really 3D**: eight vertices,
twelve edges and a perspective projection, all in integers. The idea comes from
[root42's doscube](https://github.com/root42/doscube).

## And look at the cost

**4.7 microseconds.** It is the second cheapest of the fifteen, behind `cycle`,
and it sits at number 14 of the walkthrough.

That is where the coincidence the previous thirteen carried breaks, the one where
the hard thing was the expensive thing. A wireframe cube fills nothing: **eight
points** are worked out per frame and twelve lines are drawn. The expensive part
of 3D is not the maths, **it is the filling** — and the next effect proves it, by
doing exactly the same geometry for ten times the price.

That is why the 3D effects of the era all started here.

## The chain of sums, from a vertex to a pixel

```
   1. the cube, eight vertices at +/-1                  (constant)
   2. turn about Y, then about X                        (4 multiplies)
   3. push it away, adding to z                         (1 add)
   4. divide by z: that is the perspective              (2 divides)
   5. scale and centre on the screen                    (2 adds)
```

The divides are the only place this effect divides, and there are **16 per
frame**. A rotozoom never divides once, but it touches 64000 pixels: they are
two opposite ways of spending the budget.

## Step by step

**1. The rotation**, two turns one after the other. The Y one mixes `x` and `z`
and leaves `y` alone; the X one acts on what came out of it:

```c
	rotated_x = (((long)corner_x[index] * cos_y) +
	             ((long)corner_z[index] * sin_y)) >> DEMO_SHIFT;
```

All in `long` because a product of two 8.8 numbers runs off a 16 bit `int`
**before** it is shifted back down.

**2. The whole of the perspective is dividing by `z`:**

```c
	rotated_z += CUBE_DISTANCE;
	screen_x[index] = (int)((rotated_x * CUBE_FOCAL) / rotated_z) + (DEMO_WIDTH / 2);
```

What is far has a big `z` and shrinks; what is near has a small `z` and grows.
That is all there is to it.

**3. The twelve edges, in a table of index pairs**, so the drawing loop is four
lines and not twelve calls written out by hand.

## The traps

**The pixel is not square.** Mode 13h is 320x200 stretched over a 4:3 screen, so
every pixel is 20% taller than it is wide. Project `y` with the same number as
`x` and the "cube" comes out taller than it is wide, like a shoebox on end.

```c
#define CUBE_ASPECT_NUM 	5
#define CUBE_ASPECT_DEN 	6
```

5/6 is 0.833, which is exactly 1/1.2. It is the only thing in the whole effect
that **does not come from the geometry: it comes from the monitor.**

**The colour of the edges cannot be picked by hand.** In a program with mode
13h's default palette, 15 is white and that is that. Not here: every picture
brings its own palette, so index 15 of `demo03.bmp` is whatever that photograph
happened to put there, which may well be brown.

So it gets searched for. The 768 bytes arrive as **BGR and 0 to 63** — the way
`bmp_load_pallete_data()` leaves them, dividing by 4 — and the index whose three
channels add up highest is picked: that photograph's white, whatever its number
is. It is the same trap the game's radar has with `numbers.bmp`.

**`z` never reaches zero**, which is why there is no clipping against the camera
plane. The cube is at 4 and its nearest vertex at 4 − √3 = 2.27. Without that
margin you would have to clip, which is a good deal more work than adding a
constant.

**The line coordinates are `int`.** A vertex leaves the screen constantly, and a
−9 in an `unsigned` is 65527.

## Cost

A 64000 byte `memcpy` to put the picture back as a background, and that is **by
far the dearest thing in the effect**: it costs more than the eight vertices and
the twelve lines put together.

Repainting only where the cube went last frame can be done, but that is twelve
lines to remember and walk again, and it does not beat a 64000 byte `memcpy`.

## Experiments

1. Take the two aspect lines out. The cube stretches upwards and stops looking
   like a cube: it is the quickest way to see that 320x200 is not square.
2. Set `CUBE_FOCAL` to 600. The perspective opens so wide the near faces distort
   like a wide angle lens.
3. Set `CUBE_SPIN_X` and `CUBE_SPIN_Y` both to 2. The cube always turns about the
   same diagonal and the repetition shows within three seconds.
4. Change `color = cube_brightest(palette)` to `color = 15`. On some pictures it
   will show and on others the cube will vanish: that is the palette problem.

---

**Previous:** [Flying over the picture](../../voxel/doc/README-EN.md) ·
**Next:** [The cube with a photograph on every face](../../texcube/doc/README-EN.md) ·
**Index:** [The nineteen effects](../../README-EN.md)
