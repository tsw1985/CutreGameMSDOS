# Chapter 1 — Mode 13h and video memory

*[Versión en español](README.md)*

**What you will get:** a 320x200 screen with all 256 palette colours painted by
hand.

**Which real game code is used:** `src/bmp.c` (just the `vga` variable).

---

## Building and running

From DOS, inside this folder:

```
make
chap01
```

And to clean up:

```
make clean
```

---

## 1. What a "video mode" is

A VGA card can work in many different ways: 80x25 text, 640x480 graphics with 16
colours, 320x200 with 256... each of those setups is called a **mode**, and each
one has a number.

At boot, DOS puts the card in **mode 3**: text, 80 columns by 25 rows. That is
what you see when you type `dir`.

We want **mode 0x13** (hexadecimal):

| | |
|---|---|
| Resolution | 320 x 200 pixels |
| Colours | 256 at a time |
| Bytes per pixel | 1 |
| Memory it takes | 320 x 200 = **64,000 bytes** |

That is *the* DOS games mode, and the reason is the last row: one byte per pixel
and everything in one contiguous block. It is the easiest one there is.

## 2. How you ask for it

There is no C function for this. You ask the BIOS with an **interrupt**: an
old-style system call.

```c
static void set_video_mode(unsigned int mode){

	union REGS regs;

	regs.x.ax = mode;
	int86(0x10, &regs, &regs);

}
```

- `int86()` is a Turbo C function. It fires an interrupt with the registers you
  give it.
- `0x10` is the **video** interrupt.
- Setting `AX = 0x0013` says: function 0 (change mode), mode 0x13.
- `0x0003` goes back to the usual text mode.

The real game does this same thing from assembly, in `src/video.asm`, because
that is how it was written at the start. It is exactly the same.

## 3. The screen is memory. This is the important bit

Here is the idea to take away from this chapter.

**There is no drawing function.** In mode 13h the VGA card places its 64,000
bytes of screen at address **A000:0000**. Write a byte there and a pixel
appears. That is all.

In `src/bmp.c`, line 15:

```c
unsigned char *vga = (unsigned char *) MK_FP(0xA000,0);
```

`MK_FP` means *make far pointer*: it builds a pointer out of a segment (0xA000)
and an offset (0). From then on `vga` is an array of 64,000 bytes that happens
to be the screen.

## 4. Where pixel (x, y) is

Memory is a strip. The screen is a rectangle. The conversion is:

```
position = y * 320 + x
```

```
    x=0                    x=319
  y=0  [0][1][2] ......... [319]
  y=1  [320][321] ........ [639]
  y=2  [640] .............. [959]
   .
  y=199 [63680] .......... [63999]
```

That `* 320` has a name: the **stride**, *how many bytes to step forward to go
down one row*.

Here it matches the screen width, which is why it goes unnoticed. **In chapter
21 it will stop matching**, when the map is wider than the screen, and that is
where everybody trips up. Note it now.

## 5. The byte is not a colour

The second idea of the chapter, and it is the surprising one.

If you write `vga[0] = 4;` you are not saying "paint it red". You are saying:
**"paint it with whatever colour sits at position 4 of the palette"**.

The **palette** is a table of 256 entries inside the card. Each entry holds three
numbers (red, green, blue) from 0 to 63. The pixel byte is only an index into
that table.

That has two big consequences:

1. **Changing the palette changes every colour on screen at once**, without
   touching a single pixel. The fade-to-black effects of the era are exactly
   that.
2. **Two images with different palettes cannot be mixed.** Load the pixels of
   one and the palette of another and you get the wrong colours. That is why
   every BMP in this game shares a palette byte for byte.

The 16x16 grid the program paints is the VGA's default palette: the 256 colours
the card comes up with.

## 6. Read the code

Open `chap01.c`. The loops are deliberately dumb:

```c
	for (y = 0; y < 96; y++){
		for (x = 0; x < 320; x++){
			color = (unsigned char)(((y / 6) * 16) + (x / 20));
			offset = (y * WIDTH) + x;
			vga[offset] = color;
		}
	}
```

Pixel by pixel, no optimisation at all. That way the idea shows. There will be
time to make it fast.

`WIDTH` and `HEIGHT` (320 and 200) come from `header/bmp.h`, the game's real
header. **No magic numbers copied over.**

## 7. Experiments

Things you can change and rebuild:

1. **Paint a single pixel** in the middle: `vga[(100 * 320) + 160] = 15;` Then
   go find it on screen. It is one pixel, and it is tiny.
2. **Change `* 320` to `* 321`** in the bottom loop and see what happens. The
   image comes out slanted: you have just broken the stride, which is the
   mistake point 4 warned about.
3. **Remove the second `getch()`** and rebuild. The program returns to text so
   fast you see nothing. That tells you the drawing is instant.
4. **Write outside the screen:** `vga[70000] = 15;` There is no memory
   protection in DOS: it does not fail, it writes somewhere else. If the game
   does something strange later, now you know why.

## 8. What to take away

| | |
|---|---|
| A video mode is requested from the BIOS with `int 0x10` | |
| **The screen is memory at A000:0000** | The central idea |
| A pixel is at `y * 320 + x` | That 320 is the *stride* |
| **The byte is a palette index, not a colour** | Which is why BMPs must share a palette |

---

**Next:** [Chapter 2 — Loading a BMP](../../ch02/doc/README-EN.md)
