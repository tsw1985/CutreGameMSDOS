# Before the first effect: pixels, time and fixed point

[Effect index](README-EN.md) · [Español](BASES.md)

This workshop supplies the vocabulary used by all ten effects. You need arrays and loops; no prior trigonometry is assumed.

## 1. Three separate resources

Each effect receives `image`, `screen`, `palette` and `end_tick`. `image` holds 64000 original colour indices and remains unchanged. `screen` is the writable result. `palette` contains 768 DAC-ready colour components. `end_tick` is the stopping time.

```text
original image → transformation → screen → VGA
palette ────────────────────────────────→ DAC
```

Transforming the previous result repeatedly would accumulate rounding and lose detail. Geometric effects rebuild from `image`; cycle deliberately changes the palette instead.

Input BMPs must match the loader: 320 × 200, 8-bit indexed, uncompressed, 40-byte DIB, full palette, pixels at offset 1078. Their normal size is 65078 bytes. Index 0 must appear black because clipping uses it for the background.

## 2. Rows are memory spans

`demo_row[y]` stores `y*320` as `unsigned int`; the last row starts at 63680. Signed 16-bit offsets cannot represent the whole screen.

`memcpy` copies a span and `memset` fills one with a byte. Find valid row, column and length before constructing pointers. A negative length converted to unsigned becomes a large memory operation, not an empty span.

## 3. Sine as a repeating height

Imagine the height of a point travelling around a wheel. It moves through the centre, top, centre, bottom and centre. Sine describes that smooth repeating motion.

One revolution here has 256 phases rather than 360 degrees:

| Phase | Revolution | `demo_sin` |
|---:|---|---:|
| 0 | Start | 0 |
| 64 | Quarter | 256 |
| 128 | Half | 0 |
| 192 | Three quarters | −256 |
| 256 | Same as 0 | 0 |

Cosine is sine shifted by a quarter revolution, or 64 phases. The table contains integer approximations; neighbouring phases can share a value.

Amplitude sets the extent of movement. For ±24 pixels, calculate `sine*24/256`. Phase speed sets the increment per rendered frame. Adding 4 repeats after 64 frames; adding 3 repeats after 256. For a nonzero integer speed, the phase period is `256/gcd(256,speed)`, where gcd is the greatest common divisor. Zero speed holds the phase still.

## 4. Fixed point changes the unit

Store 1.5 as `1.5*256=384`. The lowest eight bits express fractions of 1/256:

| Real value | Stored integer |
|---:|---:|
| 0.5 | 128 |
| 1 | 256 |
| 1.5 | 384 |
| 2 | 512 |

Multiplying two scaled values produces a scale of 65536. Dividing by 256 restores the original scale: `384*512/256=768`, representing 3. The intermediate product needs `long`. Widen operands **before** multiplication; assignment to a wide result afterwards is too late.

The source often uses `>> 8` to reduce the scale. Right shifting a negative signed integer is implementation-defined in C; these routines target the arithmetic-shift behaviour of their Borland environment. A port must verify it. Not every quantity is a literal 16-bit 8.8 value: zoom and rotozoom accumulate coordinates in `long`, still with eight fractional bits.

## 5. Effect duration and animation rate differ

The BIOS clock runs at approximately 18.2 ticks per second. The current default is 7 seconds, converted to `7*182/10=127` ticks. Fade-in and fade-out each add 33 retraces.

Graphic phase advances per frame. A slow machine renders fewer frames during the same tick interval and therefore completes fewer revolutions. Retrace waiting limits presentation rate; it does not guarantee 70 frames per second.

## 6. End-of-frame services

The usual route is `demo_show(screen)`: wait for retrace, copy 64000 bytes and call `demo_sound()`. That function services audio and the host callback. Cycle presents once, then handles retrace, DAC changes and `demo_sound()` directly.

Fades also service audio and the callback. Local ESC and a callback requesting cancellation share the cleanup path. Playing music does not make a long blocking calculation harmless: the current DMA half-buffer needs service approximately every 46.44 ms.

## 7. An effect skeleton

This fragment needs `demolib.h` and the player's linked functions; it is not a standalone executable:

```c
int demo_example(unsigned char *image,
                 unsigned char *screen,
                 unsigned char *palette,
                 unsigned long end_tick)
{
    (void)palette;
    while (demo_now() < end_tick) {
        if (demo_escape_pressed() == 1) {
            return 0;
        }
        memcpy(screen, image, DEMO_SCREEN);
        demo_show(screen);
    }
    return 1;
}
```

Adding an effect requires a declaration, a table entry and its object in the root Makefile and link response file. Manually copied external declarations must match their definitions: a C linker does not guarantee detection of incompatible prototypes.

## 8. Experiments and measurements

Predict a single frame first, using a labelled grid. Change one parameter in a working copy and record the direction and magnitude of the result. Then observe the full sequence.

For timing comparisons, record hardware/emulator, version, core, cycles, image, duration, initial phase and audio state. Separate image generation, retrace waiting and VGA transfer. Historical host-CPU microseconds do not predict Turbo C performance on a 386.

The tutorials count operations to explain algorithmic cost; those counts are not newly executed benchmarks.

**Exercise:** sine=128 and amplitude=20. What displacement results? What period comes from adding 4 phases? **Answer:** 10 pixels and 64 frames.

Continue with [cycle](cycle/doc/README-EN.md).
