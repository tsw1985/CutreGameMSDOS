# cycle — The palette turning

*[Versión en español](README.md)*

**Effect 1 of 10 of the walkthrough.** Measured cost: **1.6 microseconds per frame**.

The code is in [`../cycle.c`](../cycle.c).

---

## What it does

The picture stands **completely still** and the colours move. On an ordinary
photo it comes out psychedelic; on a picture drawn for the job, with its colours
in ramps, it looks like running water or burning fire.

## The idea

This is the effect to understand first, because it teaches the piece every other
one takes for granted: **in mode 13h a screen byte is not a colour, it is a
colour NUMBER.**

```
   video memory              the palette (the DAC)
   [ 37 ][ 37 ][ 12 ]        37 -> (red 40, green 12, blue 4)
                             12 -> (red  0, green 60, blue 0)
```

Change palette entry 37 and **every** pixel holding 37 changes colour at once,
without touching one byte of the screen.

That is how the falling waterfalls and the blinking signs were done in games
that could not afford to redraw anything.

## Step by step

**1. Paint the picture once.**

```c
	memcpy(screen, image, DEMO_SCREEN);
	demo_show(screen);
```

That is it. The screen is not touched again for the eight seconds it runs.

**2. Turn the palette one place.** Save the first entry, drag every entry down
one, put the saved one at the end:

```
   before:  [0][1][2][3] ... [254][255]
   after:   [0][2][3][4] ... [255][ 1 ]
                                    ^ the one that was 1
```

**3. Send it to the DAC** with `bmp_write_pallete_data_into_dac()`.

**4. Do not turn every frame.** At 70 frames a second a whole revolution would
take under four seconds and make you dizzy. `CYCLE_EVERY 3` turns one frame in
three.

## The code that matters

```c
	first = CYCLE_FIRST * CYCLE_ENTRY;
	last  = (CYCLE_FIRST + CYCLE_COUNT - 1) * CYCLE_ENTRY;

	saved[0] = palette[first + 0];
	saved[1] = palette[first + 1];
	saved[2] = palette[first + 2];

	for (i = first; i < last; i = i + CYCLE_ENTRY){
		palette[i + 0] = palette[i + CYCLE_ENTRY + 0];
		palette[i + 1] = palette[i + CYCLE_ENTRY + 1];
		palette[i + 2] = palette[i + CYCLE_ENTRY + 2];
	}

	palette[last + 0] = saved[0];
```

It is a `memmove` in disguise, written out by hand because it has to move **three
bytes at a time** (blue, green, red) and keep the entry it overwrites.

## The traps

**Colour 0 does not turn.** `CYCLE_FIRST` is 1 on purpose. Index 0 is the black
background in every palette of this project; if it turned, the background would
start flashing colours and it would look terrible.

**This is the one effect that cannot use `demo_show()`.** The others end a frame
with retrace + blit + sound, but this one blits nothing: all it does is write the
palette, **and that write has to land inside the vertical blanking**. So it does:

```c
	demo_wait_retrace();
	/* ... write the DAC ... */
	demo_sound();
```

Mixing the sound first would eat the blanking window and the palette would go in
while the beam was drawing. DOSBox does not care. A real VGA card does.

**Hand the palette back as you found it on ESC.** The effect leaves the palette
turned, and the next picture would inherit half-rotated colours. The player takes
care of it: it fades out with whatever palette is there **now**, not the one it
loaded.

## Cost

**The cheapest of the ten by a mile.** Zero pixels per frame. Just the 1024 port
writes to the DAC, and not even on every frame.

## Experiments

1. Set `CYCLE_EVERY` to 1. Dizzying.
2. Set `CYCLE_FIRST` to 0 and watch what happens to the background.
3. Turn only a small range, say entries 200 to 230, on a picture that has a
   colour ramp there. That is exactly how water was animated at the time.
4. Try it on `demo11.bmp`, a plasma with continuous ramps. It is in the set for
   this.

---

**Next:** [The endless scroll](../../scroll/doc/README-EN.md) ·
**Index:** [The ten effects](../../README-EN.md)
