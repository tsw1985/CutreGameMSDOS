# Demo workshop: ten effects from palette cycling to rotozoom

[Español](README.md) · [Start with the foundations](BASES-EN.md)

This expanded workshop lives in `docgpt`; the original C implementation remains in the repository's `demos` directory. Each lesson derives an algorithm, works through numbers, reviews limits and supplies an exercise with an answer.

| Order | Lesson | Main idea |
|---:|---|---|
| 0 | [Foundations](BASES-EN.md) | Buffers, phase, fixed point and timing |
| 1 | [Cycle](cycle/doc/README-EN.md) | Palette permutation; pixel indices remain still |
| 2 | [Scroll](scroll/doc/README-EN.md) | Circular rows split into two copies |
| 3 | [Blinds](blinds/doc/README-EN.md) | Centred openings and intermediate arithmetic |
| 4 | [Stripes](stripes/doc/README-EN.md) | Accumulated band positions and fixed velocities |
| 5 | [Wobble](wobble/doc/README-EN.md) | Spatial density and temporal phase |
| 6 | [Bounce](bounce/doc/README-EN.md) | Signed translation and clipping |
| 7 | [Ripple](ripple/doc/README-EN.md) | Distance-based phase and attenuation |
| 8 | [Mosaic](mosaic/doc/README-EN.md) | Block sampling and reuse of unchanged results |
| 9 | [Zoom](zoom/doc/README-EN.md) | Inverse sampling and a column table |
| 10 | [Rotozoom](rotozoom/doc/README-EN.md) | Linear increments and visible intervals |

Operation counts describe the algorithms, not benchmarks performed in this review. The foundations explain how to record reproducible timing measurements.

## Player and resources

[demos.h](../../demos/demos.h) exposes `demo_run()` and `demo_set_idle()`. [demos.c](../../demos/demos.c) owns resources, selection, timing and transitions; [demolib.h](../../demos/demolib.h) provides shared tables and services.

The player allocates two 64000-byte image buffers and a 768-byte palette, starts audio, loads each picture, presents it under black, fades in, runs an effect, fades out and advances. It finally closes audio, frees buffers and returns to text mode. Music comes from `res/demo.wav`.

There are seven external graphics functions and five audio functions. Graphics declarations are in `demolib.h`; audio declarations are in `demos.c`. A host port must provide all twelve with compatible signatures.

## Playback differs from teaching order

Playback order is **rotozoom, wobble, blinds, zoom, stripes, cycle, scroll, ripple, mosaic, bounce**. Fifteen valid images therefore reuse the first five effects.

The default effect interval is **7 seconds**, plus image loading and two 33-retrace fades. Fifteen images nominally take around two minutes at sustained 70 Hz. An image that cannot be opened is skipped without advancing the effect index.

## Shared effect contract

```c
int demo_name(unsigned char *image,
              unsigned char *screen,
              unsigned char *palette,
              unsigned long end_tick);
```

Preserve `image`, render into `screen`, return 1 after the interval or 0 on cancellation. Cycle deliberately edits the palette.

Most effects finish each iteration through `demo_show()`. Cycle presents initially, then directly services retrace, palette and `demo_sound()`. Fades also service audio and the host callback.

## Run and extend

From Linux, with a previously built game:

```bash
./play.sh local -demo
```

From DOS, inside `bin`:

```bat
game.exe /demo
```

Individual effects are not separate executables. Experiment in a working copy. Adding one requires its source, declaration, include and table entry, plus object rules and response-file linkage in the [root Makefile](../../Makefile).

The game uses the idle callback for network service and remote ESC. Dynamic intro resources are released before world loading. Omitting `-demo` avoids those allocations and execution; linked code and static data still exist.

## Workshop deliverable

Complete all ten exercises and create a variation. Supply its sampling formula, two hand-calculated frames, numeric limits, clipping rules and a description of what is precalculated.

[Begin with the foundations](BASES-EN.md)
