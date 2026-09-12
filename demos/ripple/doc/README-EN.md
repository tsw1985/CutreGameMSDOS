# ripple — The drop in the pond

*[Versión en español](README.md)*

**Effect 7 of 10 of the walkthrough.** Measured cost: **9.0 microseconds per frame**.

The code is in [`../ripple.c`](../ripple.c).

---

## What it does

Waves spreading out from the centre of the screen, like a drop falling into a
pond.

## The idea

It resembles `wobble` and it is not the same thing, and the difference is the one
between a flag and a puddle:

| | The sine depends on… | Result |
|---|---|---|
| **wobble** | the ROW | Waves travel top to bottom, all equally strong |
| **ripple** | the DISTANCE TO THE CENTRE | Waves come out of the middle **and die away** |

That dying away with distance is what makes it look like real water. Without it
the edge of the screen ripples as hard as the centre and the eye does not buy it.

```
   row   0  amplitude  0   .
   row  50  amplitude 10   ~~~
   row 100  amplitude 20   ~~~~~~~   <- the centre
   row 150  amplitude 10   ~~~
   row 199  amplitude  0   .
```

## Step by step

**1. The row's distance to the centre**, as an absolute value.

**2. The amplitude, falling in a straight line towards the edges:**

```c
	amplitude = (RIPPLE_AMPLITUDE * ((DEMO_HEIGHT / 2) - distance)) / (DEMO_HEIGHT / 2);
```

A quadratic falloff would be more physical and would cost one more multiply per
row for something the eye cannot tell apart.

**3. The sine, with the distance inside it:**

```c
	shift = (demo_sin(phase + (distance * RIPPLE_DENSITY)) * amplitude) >> DEMO_SHIFT;
```

**4. The usual two `memcpy`.**

## The traps

**`RIPPLE_SPEED` is NEGATIVE.** It is −5, and that is not a mistake. With a
positive value the waves come from outside towards the centre, as if the drop had
landed on the edge of the screen. Negative makes them come out of the middle,
which is what a real drop does. Flipping that sign is the difference between
looking like water and looking like a strange effect.

**The amplitude can come out 0**, and then the shift is 0 and the `memcpy` copy
the row unchanged. Correct, with no special case.

## Cost

Same as `wobble`: one sine per row. The two extra multiplies for the amplitude are
200 a frame, nothing.

## Experiments

1. Change `RIPPLE_SPEED` to +5 and compare. Same code, different effect.
2. Remove the falloff: set `amplitude = RIPPLE_AMPLITUDE` flat. It becomes a
   `wobble` with the sine measured from the centre, and stops looking like water.
3. Make the falloff quadratic by multiplying the amplitude by itself and dividing.
   See whether you can tell. I cannot, and that is why it is not there.

---

**Previous:** [The picture that wanders](../../bounce/doc/README-EN.md) ·
**Next:** [The pixelation that breathes](../../mosaic/doc/README-EN.md) ·
**Index:** [The ten effects](../../README-EN.md)
