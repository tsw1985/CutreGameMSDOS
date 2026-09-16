# twister — The twisted column

*[Versión en español](README.md)*

**Effect 11 of 20 of the walkthrough.** Measured cost: **35 microseconds per frame**.

The code is in [`../twister.c`](../twister.c).

---

## What it does

A vertical column with four faces spins on itself and is also **twisted**: the
top does not face where the bottom faces. The picture is pasted onto the four
faces, so you see the whole of it go past as the column turns.

It is one of the most recognisable effects of the era — the *Second Reality* one
— and one of the cheapest there is.

## The idea

It is a 3D effect with **no 3D in it**, and that is the whole trick.

A real cube has to be rotated, projected and filled. A vertical column does not,
because however much it turns **a point never changes row**. One row of the
screen can only show what is at that height of the column.

```
   seen from above, at the height of row y:

        corner 1
           /\            the four corners are a quarter turn
   cnr 0  /  \  cnr 2    apart: 64 of the 256 angle steps
          \  /
           \/
        corner 3

   on screen only x matters:  cx + sin(angle) * radius
```

No matrix, no `z`, no division by depth. Four sines and a span fill per row.

## Step by step

**1. This row's angle**, and here is the whole twist:

```c
	angle = phase + (y * TWISTER_TWIST);
```

Take the `y *` away and the four corners land on the same `x` on every row, and
what is left is a box turning. That multiply by the row **is** the effect.

**2. The four corners**, plus a fifth that is the first one again so the face
loop does not have to ask whether it has wrapped round:

```c
	for (corner = 0; corner < 5; corner++){
		corner_x[corner] = (DEMO_WIDTH / 2)
		                 + ((demo_sin(angle + (corner * 64)) * TWISTER_RADIUS) >> DEMO_SHIFT);
	}
```

**3. The face looking the other way is not painted**, and the test is two
characters:

```c
	if (left >= right){
		continue;
	}
```

As the column turns the corners move on screen and the faces flip over. A face
whose left corner has overtaken its right one is a face we are seeing from
behind.

And because the four are walked **in order around the column**, the ones that
pass the test never overlap: no depth sorting and no `z` buffer. It is the same
argument [texcube](../../texcube/doc/README-EN.md) makes with its cross product,
one dimension down.

**4. The picture, stretched over the face.** Each face takes a quarter of the
picture's width (320 / 4 = 80), and that quarter has to fit into the `span`
pixels the face occupies right now:

```c
	texture_step = ((long)TWISTER_FACE_WIDTH << DEMO_SHIFT) / (long)span;
	texture_u    = (long)(face * TWISTER_FACE_WIDTH) << DEMO_SHIFT;
```

One divide per face per row — 800 at most per frame — and the pixel loop is left
with a single add.

## The traps

**The texture step never runs past the edge, and it is the truncation that
guarantees it.** After `span - 1` adds, `texture_u` is at most

```
	face*80 + (80 * (span-1) / span)   <   (face+1)*80
```

strictly less, because integer division rounds down. That is why the last face
never reads column 320, which on the last row would be byte 64000 — one past the
buffer. Swap that divide for one that rounds and this effect starts reading out
of bounds.

**The clipping is `int`.** At the current radius the column does not leave the
screen, but raise `TWISTER_RADIUS` and a negative `x` in an `unsigned` is 65000
odd. And when clipping on the left you have to **advance the texture coordinate
too**, or the face stretches:

```c
	if (left < 0){
		texture_u += texture_step * (long)(-left);
		left = 0;
	}
```

## Cost

One 64000 byte `memset` plus about 35000 textured pixels. Half the screen stays
black nearly always, and that is what makes it cheap: a
[carpet](../../carpet/doc/README-EN.md) touches all 64000 every frame.

## Experiments

1. Set `TWISTER_TWIST` to 0. The twist goes and a plain turning box is left:
   that is what the effect was about.
2. Raise it to 3. The column turns almost three times from top to bottom and
   becomes a spring.
3. Remove the `if (left >= right) continue;`. All four faces get painted, the
   back ones over the front ones, and the column goes transparent and confusing.
   That is what the culling saves you.
4. Raise `TWISTER_RADIUS` to 200 and take the `left` clipping out. A crash or
   rubbish on screen, depending on the day.

---

**Previous:** [The pixelation that breathes](../../mosaic/doc/README-EN.md) ·
**Next:** [Zooming in and out](../../zoom/doc/README-EN.md) ·
**Index:** [The twenty effects](../../README-EN.md)
