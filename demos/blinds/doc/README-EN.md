# blinds — The venetian blind

*[Versión en español](README.md)*

**Effect 3 of 10 of the walkthrough.** Measured cost: **7.6 microseconds per frame**.

The code is in [`../blinds.c`](../blinds.c).

---

## What it does

The screen is cut into twenty horizontal slats and each one opens from its centre
until it uncovers the picture, then closes again. It is the transition every
slide-show program of the era had.

## The idea

Each slat is a **window that grows from the middle**. What is inside the window is
the picture; what is outside is black:

```
   shut:    [################|################]
   half:    [########|PICTURE PICTURE|########]
   open:    [PICTURE PICTURE PICTURE PICTURE ]
```

And what makes it read as a real blind instead of a stage curtain is that **each
slat lags a little behind the one above it**.

## Step by step

**1. How far open THIS slat is.** From a sine, with an offset that depends on the
slat number:

```c
	half = (((demo_sin(phase + (slat * BLINDS_STAGGER)) + DEMO_ONE)
	         * (DEMO_WIDTH / 2)) / (DEMO_ONE * 2));
```

The `+ DEMO_ONE` moves the sine from −256..256 to 0..512, and the division brings
it to 0..160, which is half the screen.

**2. Clamp it.** Rounding can leave `half` a hair out of range, and that would
give a `memset` of negative length — which in `size_t` is an enormous number.

**3. Paint each line of the slat** with two `memset` (the black edges) and one
`memcpy` (the open middle).

## The code that matters

```c
	memset(screen + row, 0, (DEMO_WIDTH / 2) - half);
	memset(screen + row + (DEMO_WIDTH / 2) + half, 0, (DEMO_WIDTH / 2) - half);

	memcpy(screen + row + (DEMO_WIDTH / 2) - half,
	       image  + row + (DEMO_WIDTH / 2) - half,
	       open);
```

Note the `memcpy` copies **from the same position** in the image and on the
screen. The slat shifts nothing, it only covers.

## The traps

**`BLINDS_SLAT` has to divide 200.** It is 10, so there are exactly 20 slats. With
11 there would be 2 leftover lines at the bottom that never get painted and keep
whatever was there before.

**Clamping `half` to 0..160 is not paranoia.** It is the only defence against a
`memset` of negative length, and that failure does not give an error: it writes
half a megabyte of zeros over whatever it finds.

## Cost

Cheap: 200 lines × (2 `memset` + 1 `memcpy`). The sine is worked out once per
**slat**, not per line: 20 sines a frame.

## Experiments

1. Set `BLINDS_STAGGER` to 0. Every slat opens at once and it stops looking like
   a blind: it looks like a curtain.
2. Raise `BLINDS_STAGGER` to 20. The wave goes round more than once down the
   screen.
3. Change `BLINDS_SLAT` to 4. Forty thin slats, far more nervous.

---

**Previous:** [The endless scroll](../../scroll/doc/README-EN.md) ·
**Next:** [Bands at different speeds](../../stripes/doc/README-EN.md) ·
**Index:** [The ten effects](../../README-EN.md)
