# bounce — The picture that wanders

*[Versión en español](README.md)*

**Effect 6 of 10 of the walkthrough.** Measured cost: **8.1 microseconds per frame**.

The code is in [`../bounce.c`](../bounce.c).

---

## What it does

The whole picture wanders around the screen over a black background, tracing a
Lissajous figure: one sine for the horizontal, another for the vertical, at
different speeds.

## The idea

This is the effect that teaches **signed clipping**, the trap that has bitten
most often in this whole project.

The picture is shifted by `(dx, dy)`. The part that falls off must not be copied,
and the part that stays must be copied from the right place. It sounds trivial and
it is not.

```
   dx positive:              dx negative:
   [   |PICTURE------>]      [<------PICTURE|   ]
    ^ black                                  ^ black
    starts at dx             starts at 0
    copies 320-dx            copies 320+dx  (dx is negative)
    from column 0            from column -dx
```

## Step by step

**1. The two shifts, from two sines with coprime speeds:**

```c
	offset_x = (demo_sin(angle_x) * BOUNCE_RANGE_X) >> DEMO_SHIFT;
	offset_y = (demo_cos(angle_y) * BOUNCE_RANGE_Y) >> DEMO_SHIFT;
```

3 and 5 share no divisors, so the path takes a very long time to close and does
not look like a loop.

**2. The whole background black**, in one go: `memset(screen, 0, DEMO_SCREEN)`.

**3. Throw away the rows that do not land:**

```c
	source_y = y - offset_y;
	if (source_y < 0 || source_y >= DEMO_HEIGHT){
		continue;
	}
```

**4. The horizontal clip**, which is the code to look at:

```c
	if (offset_x >= 0){
		destination_x = offset_x;
		source_x      = 0;
		copy_width    = DEMO_WIDTH - offset_x;
	}else{
		destination_x = 0;
		source_x      = -offset_x;
		copy_width    = DEMO_WIDTH + offset_x;
	}

	if (copy_width <= 0){
		continue;
	}
```

## The traps

**All of this has to be `int`, not `unsigned`.** Same trap as the game's sprite
clipping and the radar's subtraction: a `-9` put in an `unsigned` is 65527, and a
`memcpy` of 65527 bytes writes half a screen buffer into memory that is not ours.
On DOS that is not an error, it is silent corruption that shows up twenty seconds
later.

**The `if (copy_width <= 0)` is not spare.** At the current amplitude the picture
never leaves entirely, but raise `BOUNCE_RANGE_X` to 400 and without that line you
have a `memcpy` of negative length.

## Cost

One big `memset` and up to 200 `memcpy`. Practically the same as `scroll`.

## Experiments

1. Take the `int` off the offset variables and make them `unsigned int`. Brace
   yourself for a screen full of rubbish, and you will see why the whole game
   insists on this so much.
2. Set `BOUNCE_SPEED_X` and `BOUNCE_SPEED_Y` both to 4. The path closes into a
   diagonal and the repetition shows.
3. Raise both ranges to 200. That is where you see what `copy_width <= 0` is for.

---

**Previous:** [The image that ripples like a flag](../../wobble/doc/README-EN.md) ·
**Next:** [The drop in the pond](../../ripple/doc/README-EN.md) ·
**Index:** [The ten effects](../../README-EN.md)
