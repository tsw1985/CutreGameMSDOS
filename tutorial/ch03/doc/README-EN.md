# Chapter 3 — Double buffer and vertical retrace

*[Versión en español](README.md)*

**What you will get:** the same movement three times, better each time, and an
understanding of why.

**Which real code is used:** `src/bmp.c`.

```
make
chap03
```

---

## 1. The problem: the screen is read while you write it

The monitor does not display a fixed image: it **redraws** it about 70 times a
second, reading the VGA's 64,000 bytes top to bottom, non-stop.

And you are writing to those same bytes at the same time.

To move a bar you have to do two things:

1. Erase the bar from where it was
2. Paint it where it is now

Between the two there is an instant where **the bar is nowhere**. If the monitor
passes through at that moment, it draws a screen with no bar. At 70 times a
second, that is **flicker**.

That is pass 1 of the program.

## 2. The fix: build the frame where nobody is looking

**Double buffering**: instead of drawing on the screen, you draw into a plain
chunk of memory. Nobody is looking at it, so you can take as long as you like
and leave it half-finished as often as you need.

When the frame is **finished**, you copy it to the VGA in one go.

```c
	bmp_draw_world_window(buffer_background_image_data);       /* background */
	draw_bar(buffer_background_image_data, x);                 /* the bar */
	bmp_paint_image_data_to_vga(buffer_background_image_data);  /* all at once */
```

That `buffer_background_image_data` is exactly that: the frame under
construction. `bmp_init_buffers()` already reserved it back in chapter 2.

Goodbye flicker. That is pass 2.

## 3. But there is still tearing

The 64,000-byte copy is not instant. It takes time. And the monitor keeps
reading meanwhile.

If the monitor is on row 100 and you are copying row 50, then:

- rows 0 to 50, already read, are from the **old** frame
- rows 100 onwards, about to be read, are from the **new** one

You see half of each, with a horizontal seam. It is called **tearing**, and in
pass 2 you can see it on the bar: it looks split.

## 4. The vertical retrace

When the monitor finishes the last row it **does not start again immediately**.
The beam has to physically travel back to the top, and that takes a while: about
1.4 milliseconds in mode 13h.

During that time it is **reading nothing**. That is the perfect moment to copy.

```c
static void wait_retrace(void){

	while (inp(0x3DA) & 0x08);      /* wait for the current one to end */
	while (!(inp(0x3DA) & 0x08));   /* wait for the next one to start */

}
```

Port `0x3DA` is a VGA status register. Its `0x08` bit is 1 while a retrace is
happening.

**Both loops are necessary**, and the first one is the surprising one:

- If you arrive here and the retrace is already under way, very little of it is
  left. Starting to copy now means arriving late.
- So the first loop waits for it to **end**, and the second for the next one to
  start, whole, from the beginning.

That is pass 3, and it is what the game does in `main.c`.

## 5. And it sets your frame rate for free

Side effect: since the retrace happens about 70 times a second, waiting for it
makes your loop run at **70 frames per second**, dead steady, without using any
clock.

The game relies on that: tank speed is calibrated against the retrace. Remove it
and the game runs as fast as the processor can go, and on a 486 that is
unplayable.

## 6. Experiments

1. **Remove `wait_retrace()` from pass 3.** It becomes pass 2 again.
2. **In pass 1, remove the background repaint**
   (`bmp_paint_image_data_to_vga(buffer_original_background_bmp)`). The bar
   leaves a trail. That shows what the clean copy of the map is for.
3. **Change the step from `x = x + 2` to `x = x + 8`.** Faster, and pass 2's
   tearing becomes much more obvious.
4. **Keep only the second loop** of `wait_retrace()`. It almost always works,
   and every so often a tear slips through. Intermittent bugs look like that.

## 7. What to take away

| | |
|---|---|
| Drawing straight to the VGA flickers | You erase and paint in front of the audience |
| **Double buffer**: build the frame aside, blit it whole | Kills the flicker |
| **Retrace**: blit while the monitor is not reading | Kills the tearing |
| Waiting for the retrace also pins you to 70 fps | Without any clock |

---

**Previous:** [Chapter 2](../../ch02/doc/README-EN.md) ·
**Next:** [Chapter 4 — Sprites](../../ch04/doc/README-EN.md)
