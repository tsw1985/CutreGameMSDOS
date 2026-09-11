# Chapter 2 — Loading a BMP and showing it

*[Versión en español](README.md)*

**What you will get:** the game map on screen, loaded from disk. And seeing it
first **with the wrong colours**, on purpose, to understand why.

**Which real code is used:** all of `src/bmp.c`.

```
make
chap02
```

---

## 1. What is inside a BMP file

An 8-bit BMP has three parts, one after the other:

```
   offset 0     +--------------------------+
                |  HEADER    54 bytes      |  width, height, bits per pixel...
   offset 54    +--------------------------+
                |  PALETTE  256 x 4 bytes  |  the colours
   offset 1078  +--------------------------+
                |  PIXELS                  |  one byte per pixel
                +--------------------------+
```

The numbers that matter:

| Offset | What is there |
|---|---|
| 18 | Width, a 4-byte integer |
| 22 | Height, a 4-byte integer |
| 28 | Bits per pixel (always 8 here) |
| **54** | Where the palette starts |
| **1078** | Where the pixels start |

1078 comes from `54 + 256*4`. That is why it is hardcoded: as long as the image
is 8-bit with 256 colours, it is always there.

Each palette entry is **4 bytes**, not 3: blue, green, red and one padding byte
that goes unused. And note the order: **BGR, backwards from the usual.**

## 2. Two things, not one

This is the trap of the chapter, and it is why the program gets it wrong on
purpose first.

Loading an image is **two independent operations**:

1. **The pixels** → `bmp_fill_background_in_main_buffer()`
2. **The palette** → `bmp_extract_pallete_from_file()` then
   `bmp_write_pallete_data_into_dac()`

Do only the first and the picture is there, whole and correct, but painted with
whatever colour table was loaded before. That is the first screen of the
program.

And then, without redrawing **a single pixel**, the right palette goes in and
the image comes good:

```c
bmp_write_pallete_data_into_dac(buffer_palleta_data);
```

**The screen's 64,000 bytes have not changed.** Only the table. That is the
proof of what chapter 1 said: the byte is an index.

## 3. Why divide by 4

Look at `bmp_load_pallete_data()` in `src/bmp.c`:

```c
	fread(&value,1,1,_file);
	a = (value/4);
```

BMP stores each colour component from **0 to 255**. The VGA's DAC only takes
**0 to 63** (6 bits per component). Dividing by 4 is the conversion.

Forget it and every colour saturates: the image comes out white.

## 4. BMPs are upside down

Second trap, and this one is historical: **a BMP stores its rows bottom-up.** The
first row in the file is the last row of the image.

```
   The file:              The image:
   row 0   -------------> row 199  (the bottom one)
   row 1   -------------> row 198
   ...
   row 199 -------------> row 0    (the top one)
```

Load it as is and you see the picture upside down.

In `src/bmp.c` this is solved by reading the rows **backwards**:

```c
	for (row = map_height - 1; row >= 0; row = row - 1){
		fread(map_line, 1, map_width, file);
		destination = buffer_original_background_bmp
		            + ((unsigned long)row * (unsigned long)map_width);
		memcpy(destination, map_line, map_width);
	}
```

The file is read forwards and the image written backwards. Same work as flipping
it afterwards, without needing a whole extra buffer.

## 5. The buffers the game reserves

`bmp_init_buffers(WIDTH, HEIGHT)` asks for all of this:

| Buffer | Size | What for |
|---|---|---|
| `buffer_original_background_bmp` | width x height | The clean map. **Never drawn on** |
| `buffer_background_image_data` | 64,000 | The frame being built |
| `buffer_palleta_data` | 309 | The colours, on their way to the DAC |
| `buffer_collision_mask` | width x height / 8 | The walls (chapter 7) |

Having **two** copies of the map (the clean one and the one being drawn on) is
what makes it possible to erase the tanks from one frame to the next: you copy
the original over the top and you are done. Without it they would leave a trail.

## 6. From buffer to screen

Two steps:

```c
bmp_draw_world_window(buffer_background_image_data);       /* map -> buffer */
bmp_paint_image_data_to_vga(buffer_background_image_data); /* buffer -> VGA */
```

`bmp_draw_world_window()` copies **the visible piece** of the map. With a
one-screen world the visible piece is all of it, and it amounts to a 64,000-byte
`memcpy`. In chapter 21 that will stop being true.

## 7. Experiments

1. **Comment out the palette line** (`bmp_write_pallete_data_into_dac`). You are
   stuck with the ugly image for good. That is the most common beginner's bug.
2. **Load the palette from `sprites.bmp` and the pixels from `cutre.bmp`.** They
   share a palette in this project so nothing changes. Try it with a BMP of your
   own with a different palette and watch the mess.
3. **Load `cutrecol.bmp`** instead of `cutre.bmp`. That is the collision map:
   blue where you can go, yellow where there is wall. It is the same map seen by
   the game instead of by you.
4. **Remove the `bmp_delete_buffers()` at the end.** Nothing breaks, because DOS
   reclaims the memory on exit. But get into the habit of releasing it.

## 8. What to take away

| | |
|---|---|
| A BMP is header + palette + pixels | The pixels start at 1078 |
| **Pixels and palette are two separate loads** | Forgetting the second is the classic bug |
| The DAC wants 0..63, the BMP gives 0..255 | Hence the `/4` |
| **BMPs store rows backwards** | Read backwards, do not flip afterwards |
| There are two copies of the map | A clean one, so you can erase what was drawn |

---

**Previous:** [Chapter 1](../../ch01/doc/README-EN.md) ·
**Next:** [Chapter 3 — Double buffer and retrace](../../ch03/doc/README-EN.md)
