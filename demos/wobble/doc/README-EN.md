# wobble — The image that ripples like a flag

*[Versión en español](README.md)*

**Effect 5 of 10 of the walkthrough.** Measured cost: **8.0 microseconds per frame**.

The code is in [`../wobble.c`](../wobble.c).

---

## What it does

The picture ripples like a flag in the wind, or like a reflection in a puddle.

## The idea

**Each row is shifted sideways by a sine**, and the sine moves along a little each
frame. That is all there is to it.

```
   row  0   ->|
   row  8      ->|
   row 16        ->|
   row 24      ->|
   row 32   ->|
   row 40 <-|
```

What makes it a wave rather than a mess is that the phase depends on the row:
`demo_sin(phase + y * WOBBLE_DENSITY)`. Neighbouring rows, similar shifts.

## Step by step

**1. The row's shift:**

```c
	shift = (demo_sin(phase + (y * WOBBLE_DENSITY)) * WOBBLE_AMPLITUDE) >> DEMO_SHIFT;
```

`demo_sin()` runs from −256 to 256, so it is already 8.8 fixed point: multiplying
by the amplitude and shifting down 8 gives a number between −24 and +24 with no
division at all.

**2. Turn it positive:**

```c
	if (shift < 0){
		shift = shift + DEMO_WIDTH;
	}
```

A shift of −5 is the same as one of +315 once the row wraps.

**3. The two `memcpy`**, same as in `scroll`.

## The traps

**The `if` is enough and a `while` would be waste.** The amplitude is 24, so the
sine never goes below −24 and one addition brings it into range. Compare with
`stripes`, where a `while` IS needed: the difference is that there the offset
**accumulates** and here it is worked out from scratch every frame.

**`WOBBLE_DENSITY` decides how many waves fit.** At 3 angle steps per row, 200
rows are 600 steps, which is **a bit over two full turns**: two-and-a-bit waves
down the screen.

## Cost

One sine per **row**, not per pixel: 200 table lookups a frame against a
rotozoom's 64000 operations. It is one of the effects that impress most for what
they cost.

## Experiments

1. Raise `WOBBLE_AMPLITUDE` to 80. Still works, and the `if` is still enough,
   because 80 < 320.
2. Set `WOBBLE_DENSITY` to 1: a single very long wave, far gentler.
3. Change `demo_sin(phase + y*D)` to `demo_sin(phase)`. Every row shifts by the
   same amount and the effect becomes a horizontal `scroll` that goes back and
   forth. That shows the undulation **lives in the dependence on y**.

---

**Previous:** [Bands at different speeds](../../stripes/doc/README-EN.md) ·
**Next:** [The picture that wanders](../../bounce/doc/README-EN.md) ·
**Index:** [The ten effects](../../README-EN.md)
