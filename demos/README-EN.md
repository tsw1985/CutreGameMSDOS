# The twenty effects

*[Versión en español](README.md)*

The intro that `game.exe /demo` plays. One folder per effect, and each one
explained step by step: the idea, the code, the traps and what to break to see
how it works.

**Read them in this order.** It is not the order they play in, it is easiest
first: each one adds a technique to the one before, and by the last two you
need everything the earlier ones taught.

| | Effect | The idea | us/frame |
|---|---|---|---|
| 1 | [**cycle** — The palette turning](cycle/doc/README-EN.md) | The screen is never touched: the PALETTE turns | 1.6 |
| 2 | [**scroll** — The endless scroll](scroll/doc/README-EN.md) | Two `memcpy` per row and it wraps by itself | 7.6 |
| 3 | [**blinds** — The venetian blind](blinds/doc/README-EN.md) | Slats opening from the centre, staggered | 7.6 |
| 4 | [**stripes** — Bands at different speeds](stripes/doc/README-EN.md) | Parallax in reverse, speeds taken from a sine | 7.6 |
| 5 | [**melt** — The screen that melts away](melt/doc/README-EN.md) | Doom's wipe: 320 numbers that grow | 27 † |
| 6 | [**wobble** — The image that ripples like a flag](wobble/doc/README-EN.md) | One sine per ROW: a flag in the wind | 8.0 |
| 7 | [**water** — The picture reflected in the water](water/doc/README-EN.md) | Wobble, mirrored, with the amplitude growing | 2.2 † |
| 8 | [**bounce** — The picture that wanders](bounce/doc/README-EN.md) | Lissajous, and SIGNED clipping | 8.1 |
| 9 | [**ripple** — The drop in the pond](ripple/doc/README-EN.md) | Like wobble, but the wave dies with distance | 9.0 |
| 10 | [**mosaic** — The pixelation that breathes](mosaic/doc/README-EN.md) | Blocks, and not repainting what has not changed | 19.6 |
| 11 | [**twister** — The twisted column](twister/doc/README-EN.md) | 3D with no 3D in it: it all falls to one dimension | 35 † |
| 12 | [**zoom** — Zooming in and out](zoom/doc/README-EN.md) | The column table: 320 calculations, not 64000 | 110 |
| 13 | [**lens** — The magnifying glass](lens/doc/README-EN.md) | Precompute a table, but only 6100 points of it | 9.4 † |
| 14 | [**rotozoom** — Rotate and zoom at once](rotozoom/doc/README-EN.md) | 2 adds per pixel, and the visible span per row | 179 |
| 15 | [**carpet** — The rotozoom that never ends](carpet/doc/README-EN.md) | The same one masked: it repeats for ever | 73 † |
| 16 | [**tunnel** — The endless tunnel](tunnel/doc/README-EN.md) | Two 64000 byte tables and the frame is just adds | 77 † |
| 17 | [**sphere** — The picture on a spinning ball](sphere/doc/README-EN.md) | The same with ONE table: latitude is per row | 22 † |
| 18 | [**voxel** — Flying over the picture](voxel/doc/README-EN.md) | The Comanche engine: one ray per column | 135 † |
| 19 | [**cube** — The wireframe cube](cube/doc/README-EN.md) | Real 3D, and among the cheapest: it fills nothing | 4.7 † |
| 20 | [**texcube** — The cube with a photograph on every face](texcube/doc/README-EN.md) | The same one filled, for ten times the price | 50 † |

Costs measured by running the real code over `res/demo/`, in microseconds of host
CPU per frame. They are relative: what matters is that `rotozoom` costs twenty
times a `wobble`, not the absolute number.

† **The ten marked ones were measured on a different machine**, so they cannot be
compared head on with the other ten. Two anchors for scaling them: on that bench
`rotozoom` comes out at **55** rather than 179 and `zoom` at **35** rather than
110. It is not even a constant factor — a machine weighs memory and arithmetic
differently — so the column is good for ordering within each group and not much
else.

**And cost stopped tracking difficulty a while back.** The first ten went
together: the hard one was the expensive one. From `lens` on it no longer does,
for a reason that is the lesson of the second half of the walkthrough:

- **precomputing is free in time and paid for in memory.** `sphere` does
  spherical trigonometry for 22 microseconds because it settled the arcsines on
  the way in.
- **the expensive part of 3D is not the maths, it is the filling.** `cube` sits
  at number 19 and is among the cheapest on the list: it fills nothing. `texcube`
  does exactly the same geometry for ten times the price.
- **and what really costs is writing pixels one at a time.** The three dearest —
  `voxel`, `rotozoom` and `tunnel` — are the three that touch all 64000 without
  being able to use a `memcpy`.

## How the folder is put together

```
demos/
   demos.h          the whole public API: demo_run()
   demos.c          the player: memory, timing, ESC, fades, the effect table
   demolib.h        what every effect shares: sine table, retrace, demo_show()
   <effect>/
      <effect>.c    the effect
      <effect>.h    its one declaration
      doc/          this explanation, in both languages
```

Every effect has the **same shape**, which is what lets `demos.c` keep them in a
plain array of function pointers:

```c
int demo_<name>(unsigned char *image,
                unsigned char *screen,
                unsigned char *palette,
                unsigned long end_tick);
```

Adding one is: write the `.c` in its folder, include its `.h` in `demos.c`, add
one line to `demo_effects[]` and a rule to the `Makefile`. Nothing else
anywhere: `main.c` never finds out.

There are **twenty**. `res/demo/` holds fifteen pictures, so one pass shows
fifteen effects and five never come up. With twenty pictures they all appear and
none repeats; past twenty, the list starts again.

## The rules they all follow

- **No floating point.** Not one `float` in the folder. Fixed point 8.8 and a
  256-entry sine table, because a whole turn of 256 steps wraps with `& 255`
  instead of a division.
- **`demo_show()` once per frame.** Retrace, blit, and feed the sound card. The
  sound is in there so that no effect can forget it — one already did, and the
  music got stuck in a loop for eight seconds.
- **No game headers.** The folder borrows seven functions from `bmp.c` and four
  from `sound.c`, declared by hand in `demolib.h`. It knows nothing about tanks.
- **Nothing exists without `/demo`.** Not a byte of memory, not a file opened.
- **One callback per frame, and nothing more.** `demo_set_idle()` takes a
  function pointer that `demo_sound()` calls once a frame; return 1 from it and
  the intro stops as if ESC had been pressed. The game plugs its network
  heartbeat in there, and the folder still does not know a network exists.

## Running them

```bash
./play.sh local -demo
game.exe /demo
```

See [`../COMANDOS.md`](../COMANDOS.md) for the picture requirements and the DOSBox
settings.
