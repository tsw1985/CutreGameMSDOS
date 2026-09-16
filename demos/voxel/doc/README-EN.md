# voxel — Flying over the picture

*[Versión en español](README.md)*

**Effect 17 of 19 of the walkthrough.** Measured cost: **135 microseconds per
frame**, the dearest of the twenty.

The code is in [`../voxel.c`](../voxel.c).

---

## What it does

The picture is read as a height map — light is hill, dark is valley — and then
you fly over it. It is the **Comanche (1992)** engine, which astonished everyone
because it looked like 3D and there was not one polygon in it.

## From a photograph to a landscape

Two maps are needed: one of **heights** and one of **colours**. The picture gives
both, for free:

```
   colour = the pixel, as it is
   height = how light that colour is
```

The second one **cannot be read off the pixel**, and there is the trap that
follows half this folder around: in 256 colours a pixel is not a colour, it is an
**index**. 200 is not lighter than 3, it is simply the one sitting in slot 200.
You have to go to the palette, add its three channels and keep that.

That is 256 additions on the way in, and they leave a table that turns an index
into a height at a glance.

## And then the relief has to be stretched

This was not there and had to be added after looking at the bench. With raw
heights, a picture of similar tones gave **a perfect plain**:

```
   demo13: 87% of its pixels land on the SAME height
           -> the landscape is a plateau, and correctly so
   demo01: its colours run from 7 to 210
           -> mountains and ravines
```

So the lowest and the highest **of the ones the picture actually uses** are found
— by walking the texture, not the palette, because half a palette may be unused —
and that slice is spread over the 63 steps. A flat picture gets gentle hills and
a contrasty one gets cliffs, but none of them gets a billiard table.

What it does not fix: if 87% of the picture is one tone, 87% of the landscape is
a plateau however much you stretch. That is not a defect of the code, it is that
the picture is flat.

## The algorithm, which is one line

For every **column** of the screen a ray is fired at the horizon and walked. At
each step the terrain height is read, projected to a screen row, and:

> if it comes out **higher** than the highest already painted in this column,
> the new piece is painted. If not, it is hidden and nothing is painted.

```
   walking from NEAR to FAR:

   horizon ------------------------------
                        ___
                   ____/   \___            <- this shows: it comes out higher
              ____/            \__
         ____/                              <- this is hidden: not painted
   ____/
```

That is enough for the near to hide the far **with no depth buffer, no sorting
and without repainting a single pixel twice**. One number per column — *how far
up I have got* — does the job a real 3D engine needs a 64000 byte z-buffer for.

## And the usual trick: not one divide

Projecting means dividing by the distance, and that is 41000 steps a frame. The
distances, on the other hand, are always the same 128: it walks one at a time. So
the 128 reciprocals are precomputed on the way in:

```c
	screen_y = VOXEL_HORIZON
	         + (((VOXEL_CAMERA - terrain) * inverse[z]) >> DEMO_SHIFT);
```

A multiply and a shift where there was a divide.

## The traps

**The camera has to fly above the highest hill** or it would end up inside the
terrain. 80 against 63 leaves little margin on purpose: the lower you fly, the
more the distances between high and low open up and the more relief you see. With
the camera at 200 the same landscape looks like a painted map.

**Nothing can be drawn above the horizon**, and that gives an early exit that is
exact rather than an approximation: since `VOXEL_CAMERA - terrain` is always
positive, `screen_y` always lands below `VOXEL_HORIZON`. Once a column reaches
there, any remaining steps are wasted time.

**The ANDs wrap the map round** because the texture is 128, a power of two. The
landscape repeats for ever both ways and never runs out. It is what
[carpet](../../carpet/doc/README-EN.md) does, sideways.

## Cost

**The dearest of the twenty**, for two reasons that add up: 41000 ray steps and
up to 64000 writes **at a stride of 320 bytes**, because a screen column is not
consecutive bytes. It is the same awkward write as
[melt](../../melt/doc/README-EN.md), but here with a calculation at every step.

If it drags on your machine, `VOXEL_DEPTH` at 96 takes a quarter of the work away
and all you lose is the furthest band, which is already squeezed into eight rows.

## Experiments

1. Set `VOXEL_CAMERA` to 200. The relief flattens until it looks like a painted
   map: there you see that the flying height is the contrast control.
2. Remove the relief stretch (the `lowest`/`loftiest` block). Then try a light
   picture and you will get the billiard table.
3. Set `VOXEL_SPREAD` to 256. The field of view opens so wide that the edges
   stretch like a fisheye.
4. Change the walk to far-to-near and drop the `if (screen_y >= highest)`. It
   still comes out right and costs twice as much: that is painting what is about
   to be covered.

---

**Previous:** [The picture wrapped round a spinning ball](../../sphere/doc/README-EN.md) ·
**Next:** [The wireframe cube](../../cube/doc/README-EN.md) ·
**Index:** [The nineteen effects](../../README-EN.md)
