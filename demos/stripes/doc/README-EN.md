# stripes — Bands at different speeds

*[Versión en español](README.md)*

**Effect 4 of 10 of the walkthrough.** Measured cost: **7.6 microseconds per frame**.

The code is in [`../stripes.c`](../stripes.c).

---

## What it does

The screen is cut into horizontal bands and each one scrolls at its own speed:
some fast to the right, some slowly to the left. The picture is torn apart and
puts itself back together when the speeds line up again.

## The idea

It is the **parallax** of platform games used backwards. There, the far background
moves slower than the ground to give depth; here it is used to break the picture
up.

And the key is where each band's speed comes from: **a sine of its band number**,
not a random number.

```
   band   0  1  2  3  4  5  6  7  ...
   speed +6 +5 +3  0 -3 -5 -6 -5      <- a sine
```

With random speeds it looks like a broken television. With a sine, neighbouring
bands move alike and the whole thing **undulates**.

## Step by step

**1. One accumulated offset per band**, in an array of 25 ints.

**2. Each band's speed, fixed for the whole effect:**

```c
	speed = (demo_sin(stripe * STRIPES_SPREAD) * STRIPES_SPEED) >> DEMO_SHIFT;
```

**3. Accumulate and wrap.** Here with `while`, not `if`:

```c
	while (offset[stripe] < 0){
		offset[stripe] = offset[stripe] + DEMO_WIDTH;
	}
	while (offset[stripe] >= DEMO_WIDTH){
		offset[stripe] = offset[stripe] - DEMO_WIDTH;
	}
```

**4. The usual two `memcpy`**, eight times per band.

## The traps

**Here the `while` really is needed.** In `scroll` an `if` was enough because the
offset grew by one. Here it grows by six, in both directions, and after hundreds
of frames I would not bet on it. An `if` that falls short leaves an out-of-range
offset and the `memcpy` runs off the buffer.

**`STRIPES_SPREAD` is prime (11).** If it divided 256, the speed pattern would
repeat every few bands and the symmetry would show.

## Cost

The same as `scroll`: two `memcpy` per line. The sine is worked out once per band,
25 a frame.

## Experiments

1. Replace the sine with `rand()`. Compare: undulation becomes malfunction.
2. Set `STRIPES_HEIGHT` to 1. Two hundred one-line bands. A completely different
   effect, and just as period-correct.
3. Make the speed change over time: add `phase` inside the `demo_sin()`. The bands
   speed up and slow down.

---

**Previous:** [The venetian blind](../../blinds/doc/README-EN.md) ·
**Next:** [The image that ripples like a flag](../../wobble/doc/README-EN.md) ·
**Index:** [The ten effects](../../README-EN.md)
