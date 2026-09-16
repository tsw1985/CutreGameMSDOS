# The nineteen effects

*[Versión en español](README.md)*

The intro that `game.exe /demo` plays. One folder per effect, and each one
explained step by step: the idea, the code, the traps and what to break to see
how it works.

**Read them in this order.** It is not the order they play in, it is easiest
first: each one adds a technique to the one before, and by the last two you
need everything the earlier ones taught.

| | Effect | The idea | us/frame |
|---|---|---|---|
| 1 | [**scroll** — The endless scroll](scroll/doc/README-EN.md) | Two `memcpy` per row and it wraps by itself | 7.6 |
| 2 | [**blinds** — The venetian blind](blinds/doc/README-EN.md) | Slats opening from the centre, staggered | 7.6 |
| 3 | [**stripes** — Bands at different speeds](stripes/doc/README-EN.md) | Parallax in reverse, speeds taken from a sine | 7.6 |
| 4 | [**melt** — The screen that melts away](melt/doc/README-EN.md) | Doom's wipe: 320 numbers that grow | 36 † |
| 5 | [**wobble** — The image that ripples like a flag](wobble/doc/README-EN.md) | One sine per ROW: a flag in the wind | 8.0 |
| 6 | [**water** — The picture reflected in the water](water/doc/README-EN.md) | Wobble, mirrored, with the amplitude growing | 3.1 † |
| 7 | [**bounce** — The picture that wanders](bounce/doc/README-EN.md) | Lissajous, and SIGNED clipping | 8.1 |
| 8 | [**ripple** — The drop in the pond](ripple/doc/README-EN.md) | Like wobble, but the wave dies with distance | 9.0 |
| 9 | [**mosaic** — The pixelation that breathes](mosaic/doc/README-EN.md) | Blocks, and not repainting what has not changed | 19.6 |
| 10 | [**twister** — The twisted column](twister/doc/README-EN.md) | 3D with no 3D in it: it all falls to one dimension | 41 † |
| 11 | [**zoom** — Zooming in and out](zoom/doc/README-EN.md) | The column table: 320 calculations, not 64000 | 110 |
| 12 | [**lens** — The magnifying glass](lens/doc/README-EN.md) | Precompute a table, but only 6100 points of it | 12 † |
| 13 | [**rotozoom** — Rotate and zoom at once](rotozoom/doc/README-EN.md) | 2 adds per pixel, and the visible span per row | 179 |
| 14 | [**carpet** — The rotozoom that never ends](carpet/doc/README-EN.md) | The same one masked: it repeats for ever | 79 † |
| 15 | [**tunnel** — The endless tunnel](tunnel/doc/README-EN.md) | Two 64000 byte tables and the frame is just adds | 97 † |
| 16 | [**sphere** — The picture on a spinning ball](sphere/doc/README-EN.md) | The same with ONE table: latitude is per row | 27 † |
| 17 | [**voxel** — Flying over the picture](voxel/doc/README-EN.md) | The Comanche engine: one ray per column | 117 † |
| 18 | [**cube** — The wireframe cube](cube/doc/README-EN.md) | Real 3D, and among the cheapest: it fills nothing | 6.5 † |
| 19 | [**texcube** — The cube with a photograph on every face](texcube/doc/README-EN.md) | The same one filled, for thirteen times the price | 88 † |

Costs measured by running the real code over `res/demo/`, in microseconds of host
CPU per frame. They are relative: what matters is that `rotozoom` costs twenty
times a `wobble`, not the absolute number.

† **The ten marked ones were measured on a different machine**, so they cannot be
compared head on with the other nine. Two anchors for scaling them: on that bench
`rotozoom` comes out at **49** rather than 179 and `zoom` at **24** rather than
110.

**And cost stopped tracking difficulty a while back.** The first ones went
together: the hard one was the expensive one. From `lens` on it no longer does,
for a reason that is the lesson of the second half of the walkthrough:

- **precomputing is free in time and paid for in memory.** `sphere` does
  spherical trigonometry for 27 microseconds because it settled the arcsines on
  the way in.
- **the expensive part of 3D is not the maths, it is the filling.** `cube` sits
  at number 18 and is among the cheapest on the list: it fills nothing.
  `texcube` does exactly the same geometry for thirteen times the price.
- **and what really costs is writing pixels one at a time.** The dearest ones —
  `rotozoom`, `voxel`, `tunnel` and `texcube` — are the ones that touch the
  whole screen without being able to use a `memcpy`.

## There was a twentieth, and it was dropped

`cycle` turned the palette without touching a single pixel: the cheapest effect
of all and the most of its era. But it only works on pictures drawn for it, with
their colours in a ramp. On a **photograph** it does not give an effect, it
gives a mush of colour with nothing recognisable in it, and this is a slideshow
of photographs. Gone.

## And an epilogue, which is not an effect

After the last picture comes [**outro** — the goodbye
card](outro/doc/README-EN.md): a card that breathes gently and waits for you to
press a key.

It is not in the table and not part of the walkthrough, because **it does not
end by itself**: the twenty effects are handed an `end_tick` and stop; this one
waits. In the table it would hang the slideshow on whichever picture it landed
on.

It is there so the intro does not end with a jolt. The last picture used to fade
out and the game started in the same instant; in front of an audience that is a
jump. The card turns the cut into a stop.

The picture is `res/demo/fin.bmp` and changes like any other — 320x200, 256
colours, 65078 bytes — with one condition: **the text has to fit in the middle
91%**, because the card zooms in to 1.09. The exact numbers are in [its own
document](outro/doc/README-EN.md). If the file is not there it is skipped and
nothing happens.

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

There are **nineteen**. `res/demo/` holds fifteen pictures, so one pass shows
fifteen effects and four never come up. With nineteen pictures they all appear
and none repeats; past nineteen, the list starts again.

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
