# water — The picture reflected in the water

*[Versión en español](README.md)*

**Effect 6 of 19 of the walkthrough.** Measured cost: **2.2 microseconds per
frame**, the cheapest of the twenty.

The code is in [`../water.c`](../water.c).

---

## What it does

The top half is the picture. The bottom half is the same picture upside down,
rippling, as if the scene were at the edge of a pond.

## It is wobble, with three differences

[wobble](../../wobble/doc/README-EN.md) shifts every row by a sine. So does this,
which is why it comes right after it in the walkthrough: what changes is not the
technique, **it is what is done with it**.

```
   1. only the bottom half ripples
   2. that half reads the picture BACKWARDS, from row 99 upwards
   3. the amplitude GROWS downwards instead of staying constant
```

The third one does the work:

```
   constant amplitude              growing amplitude
   ~~~~~~~~~~~~~~~~~  shore        ~~~~~~~~~~~~~~~~~  shore
   ~~~~~~~~~~~~~~~~~                ~~~~~~~~~~~~~~~
   ~~~~~~~~~~~~~~~~~                 ~~~~~~~~~~~~~
   ~~~~~~~~~~~~~~~~~                  ~~~~~~~~~~~
   looks like a flag               looks like water
   hung upside down
```

And it has a physical reason: what is reflected near the horizon is far away, and
what is far away barely moves in the image even though the water moves the same.

## Step by step

**1. The top half is copied ONCE, outside the loop.** It never changes, so that
is 32000 bytes a frame that are never touched. There is half the reason this is
the cheapest effect in the folder.

**2. The amplitude, which is a straight line and not a sine:**

```c
	amplitude = WATER_AMPLITUDE_FAR
	          + (((WATER_AMPLITUDE_NEAR - WATER_AMPLITUDE_FAR) * depth)
	             / (DEMO_HEIGHT - WATER_HORIZON));
```

What has to grow is **how much** it moves, not **how**: the shape of the wave is
already set by the sine on the next line.

**3. The source row, counted upwards from the shore.** That is what turns the
reflection over:

```c
	source_y = WATER_HORIZON - 1 - depth;
```

**4. And the shift, with two `memcpy` and two `memset`** — one for the part you
see and one for the black gap it leaves.

## The traps

**`shift` is an `int` and can go negative**, and there is the trap that follows
this whole project around: in an `unsigned` it would be 65527, and a `memcpy` of
65527 bytes takes out whatever sits behind the buffer. That is why there are two
explicit branches and not one clever expression covering both.

**`source_y` is clamped to 0.** With the horizon exactly halfway it never goes
out, but move it to a third of the screen and the reflection would ask for
negative rows.

## Cost

**The cheapest of the twenty**, and not by a little: half the screen is never
touched and the other half is 100 `memcpy` of under 320 bytes. Not one pixel is
written on its own.

## Experiments

1. Make `WATER_AMPLITUDE_FAR` equal to `WATER_AMPLITUDE_NEAR`. The upside down
   flag is back and the water is gone.
2. Drop the `- 1 - depth` and leave `source_y = depth`. The reflection stops
   being mirrored and it looks like a split screen.
3. Raise `WATER_HORIZON` to 150. A puddle instead of a lake.
4. Raise `WATER_AMPLITUDE_NEAR` to 40. There you see what the two clipping
   branches are for.

---

**Previous:** [The image that ripples like a flag](../../wobble/doc/README-EN.md) ·
**Next:** [The picture that wanders](../../bounce/doc/README-EN.md) ·
**Index:** [The nineteen effects](../../README-EN.md)
