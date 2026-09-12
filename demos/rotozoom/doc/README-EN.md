# rotozoom — Rotate and zoom at once

*[Versión en español](README.md)*

**Effect 10 of 10 of the walkthrough.** Measured cost: **179 microseconds per frame**, and 257 before it was optimised.

The code is in [`../rotozoom.c`](../rotozoom.c).

---

## What it does

The picture spins over black while it zooms in and out. **THE** effect of 90s
intros, the one you had to have.

## The idea

It is `zoom` with rotation, and the rotation changes everything.

The calculation for a pixel `(x,y)` measured from the centre is:

```
   u =  x*cos(a)*scale + y*sin(a)*scale
   v = -x*sin(a)*scale + y*cos(a)*scale
```

Four multiplications per pixel. **256000 a frame.** On an 8086 that is not
happening.

### The trick of the era

The calculation is **linear**. That means that moving one pixel to the right, `u`
and `v` always grow by the same amount, wherever you are:

```
   x:    0     1     2     3     4
   u:  100   112   124   136   148      <- always +12
   v:   50    57    64    71    78      <- always +7
```

So you work those two increments out **once per frame** and the inner loop is two
additions:

```c
	u += du_dx;
	v += dv_dx;
```

**From 256000 multiplications to 2 additions per pixel.** That is the rotozoom.

## Step by step

**1. The four increments**, worked out once:

```c
	du_dx = ((long)demo_cos(angle) * scale) >> DEMO_SHIFT;
	dv_dx = ((long)demo_sin(angle) * scale) >> DEMO_SHIFT;
	du_dy = -dv_dx;
	dv_dy =  du_dx;
```

`du_dy` and `dv_dy` are for going down a row, and they come from the other two:
going down a row is the same as moving along a column, turned 90 degrees.

**2. The top-left corner.** Start at the centre of the image and back off half a
screen **in both rotated directions**. Without this the picture would spin about
its corner.

**3. Walk**, adding `du_dx`/`dv_dx` per column and `du_dy`/`dv_dy` per row.

## The optimisation: the visible span

The rotated image is a **quadrilateral**, and a horizontal line cuts a
quadrilateral in **one single piece**. Not two, not several: one.

```
   row 40:   ############/PICTURE\##############
                         ^       ^
                      first     last
```

So instead of asking 320 times per row "am I inside?", you work out where it
starts and ends, fill the outside with `memset`, and **inside the span nothing is
checked**, because by construction it all falls inside.

From **257 µs/frame to 179**. And in a rotozoom with the picture zoomed out, the
outside is most of the screen.

### How the span is worked out, without dividing negatives

`rotozoom_span()` solves `0 <= start + x*step < limit`. And all its divisions are
**positive by positive**, on purpose:

> In C89 the rounding of a division with negatives is **up to the compiler**. That
> is exactly the sort of thing that works in gcc and does something else in
> Turbo C.

When the step is negative the sign is flipped and the same problem is solved the
other way round:

```c
	if (step > 0){
		if (start >= 0){
			first = 0;
		}else{
			first = ((-start) + step - 1) / step;   /* round up */
		}
		...
	}else{
		down = -step;      /* now the division is positive */
		...
	}
```

## The traps

**`scale` can never be 0**: that would be dividing the world by nothing. Hence the
`if (scale < 32)`.

**Two 8.8 products in a row give 16.16.** That is why every multiplication has its
`>> DEMO_SHIFT` behind it. Forget one and the effect comes out at 256 times the
scale and you see nothing.

**The spans have to be intersected**, not one or the other. A pixel has to be
inside in `u` **and** in `v`; the good span is the intersection of the two.

## Cost

**The most expensive of the ten by a distance**: 179 µs/frame, over twenty times a
`wobble`. It is unavoidable, and that is why it is first in the effect list: when
the machine is struggling, this is the one you notice.

## Experiments

1. Set `ROTOZOOM_SPIN` to 0. It becomes a `zoom`, and you will see the real `zoom`
   does the same thing much faster.
2. Remove the `>> DEMO_SHIFT` from `du_dx`. Black screen and nothing else.
3. Replace `rotozoom_span()` with the per-pixel `if` of the first version and
   measure. The gap grows the further the picture is zoomed out.
4. Try making `du_dy = dv_dx` instead of `-dv_dx`. You get a rotated mirror: handy
   for understanding why the sign is where it is.

---

**Previous:** [Zooming in and out](../../zoom/doc/README-EN.md) ·
**Index:** [The ten effects](../../README-EN.md)
