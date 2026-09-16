# melt — The screen that melts away

*[Versión en español](README.md)*

**Effect 4 of 19 of the walkthrough.** Measured cost: **27 microseconds per frame**.

The code is in [`../melt.c`](../melt.c).

---

## What it does

Every column of the picture slides off on its own, at its own speed, starting
when its turn comes, until nothing is left. It is the wipe **Doom changed screens
with in 1993**, and it is still the most recognisable transition ever made.

It is also the effect with the least maths of the twenty: no fixed point, no
table, no projection. Just 320 numbers that grow.

## Why it goes by columns and not by rows

The four effects before it move **whole rows**, because a row is 320 consecutive
bytes and that is a `memcpy`. This one cannot:

```
   every column has fallen a different amount

   x=0   x=1   x=2   x=3
    |     |           |
    v     v     v     v      <- one ROW of the screen is made of
   ###   ###   ###   ###        320 pieces from 320 different rows
   ###   ###         ###        of the picture
```

Walking it by columns means writing with a stride of 320 bytes, the worst thing
you can ask of a cache. On a 386 that does not matter — there is no cache to
spoil — and it is 64000 writes with not a single calculation, so it stays among
the cheap ones.

## Randomness without `rand()`, and not ending up with a comb

Doom drew each column's delay at random. There is no `rand()` here and none is
needed: two sines of different periods, added, give a sequence that does not
repeat across 320 columns and that comes out **the same on both machines** of a
network game.

But the spread cannot be pure randomness, and that cost a pass on the bench. With
two short-period sines every column fell at a rate unrelated to its neighbour and
what came out was a **comb**: 320 independent sticks, not a curtain.

```
   COMB (wrong)                 CURTAIN (right)
   |‾|_|‾‾|_|‾|__|‾|            ‾‾\__/‾‾‾\_/‾‾‾‾\__
   similar frequencies          one SLOW wave in charge
   at the same weight           + a fast one that only nicks the edge
```

Doom did not use sines: it drew the first column and let each next one drift at
most one step from the one before. The result is the same and here it comes from
**weighting the frequencies**:

```c
	mixed = (demo_sin(x * MELT_SLOW) * MELT_SLOW_WEIGHT)
	      +  demo_sin(x * MELT_FAST);
```

`MELT_SLOW` is 2 out of 256 steps, one wave every 128 columns: that one makes the
curtain. `MELT_FAST` is 37, one wave every seven columns, and at a third of the
weight it only nicks the edge.

## Step by step

**1. The spread, once only.** A column has to fall at the same speed for ever: if
it were worked out per frame the curtain would shiver instead of falling.

**2. The speed comes from another wave, shifted by 64 steps** (a quarter turn).
From the same one, the columns that start late would also be the slow ones and
the curtain would always open in the same place.

**3. Spend the delay first, fall after:**

```c
	if (delay[x] > 0){
		delay[x]--;
	}else if (offset[x] < DEMO_HEIGHT){
		offset[x] += speed[x];
	}
```

**4. Start again when nothing is falling.** Seven seconds is a bit over two
passes, and leaving the screen black for the last three would be a wasted slide.

## The traps

**The speed range is narrow on purpose.** With `MELT_SPEED_MIN` 1 and `MAX` 4 the
fast columns go four times quicker than the slow ones and the curtain falls apart
within two seconds. 2 and 4 hold the shape to the end.

**The `+ 1024` instead of an absolute value.** `mixed` runs from −1024 to 1024 and
is needed in 0..2048. Adding the range shifts the whole thing; an absolute value
would fold it in half and the columns on the left would fall exactly like the
ones on the right.

## Experiments

1. Set `MELT_SLOW_WEIGHT` to 0. There is the comb, and you see why that one line
   is the whole effect.
2. Set `MELT_FAST` to 2 as well. The curtain comes out perfectly smooth and looks
   like a blind, not like something melting.
3. Remove the 64 step shift from the speed. The curtain always opens in the same
   place.
4. Raise `MELT_DELAY_MAX` to 200. The first columns reach the bottom before the
   last ones have started.

---

**Previous:** [Bands at different speeds](../../stripes/doc/README-EN.md) ·
**Next:** [The image that ripples like a flag](../../wobble/doc/README-EN.md) ·
**Index:** [The nineteen effects](../../README-EN.md)
