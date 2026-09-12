# The ten effects

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
| 5 | [**wobble** — The image that ripples like a flag](wobble/doc/README-EN.md) | One sine per ROW: a flag in the wind | 8.0 |
| 6 | [**bounce** — The picture that wanders](bounce/doc/README-EN.md) | Lissajous, and SIGNED clipping | 8.1 |
| 7 | [**ripple** — The drop in the pond](ripple/doc/README-EN.md) | Like wobble, but the wave dies with distance | 9.0 |
| 8 | [**mosaic** — The pixelation that breathes](mosaic/doc/README-EN.md) | Blocks, and not repainting what has not changed | 19.6 |
| 9 | [**zoom** — Zooming in and out](zoom/doc/README-EN.md) | The column table: 320 calculations, not 64000 | 110 |
| 10 | [**rotozoom** — Rotate and zoom at once](rotozoom/doc/README-EN.md) | 2 adds per pixel, and the visible span per row | 179 |

Costs measured by running the real code over `res/demo/`, in microseconds of host
CPU per frame. They are relative: what matters is that `rotozoom` costs twenty
times a `wobble`, not the absolute number.

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

Adding one is: write the `.c` in its folder, include its `.h` in `demos.c`, and add
one line to `demo_effects[]`. Nothing else anywhere.

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
