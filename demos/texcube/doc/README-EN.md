# texcube — The cube with a photograph on every face

*[Versión en español](README.md)*

**Effect 19 of 19 of the walkthrough.** Measured cost: **50 microseconds per
frame**, plus the time to load six BMPs and build five colour tables on the way
in.

The code is in [`../texcube.c`](../texcube.c).

---

## What it does

The same cube, but **filled**: six faces, each one carrying one of the pictures
of the slideshow, turning in three dimensions about all three axes.

It is the effect everybody remembers from the intros and, by a distance, the
longest in this folder. Because of three problems no other one has.

---

## Problem 1: six photographs and one single palette

And it is the worst of the three, however it may look.

The VGA in mode 13h has **one** set of 256 colours. Every BMP brings its own.
Measured over `res/demo/`:

```
   demo01 against demo02 ............. 1 entry alike out of 256
   average of the 15, pairwise ....... 2.3 entries alike out of 256
```

So pasting photograph 2 onto a face with photograph 1's palette loaded does not
give "a picture with slightly odd colours": it gives **random colour noise**.

It is exactly the same trap as the game's radar, where `numbers.bmp` and
`big.bmp` differ in 254 of the 256 entries, and why the radar does not exist
without a theme.

**The way out is the one the demos of the era used: remapping.** The palette of
the picture the slideshow is on stays, and each of the other five has all 256 of
its colours translated to the **closest** index of that palette.

```c
	distance = texcube_square[db] + texcube_square[dg] + texcube_square[dr];
	if (distance < best_distance){
		best_distance = distance;
		best_index    = target;
	}
```

Brute force: 65536 comparisons per photograph, five photographs, **327680
distances** on the way in. The channels run 0 to 63, so a table of 64 squares
swaps the three multiplies for three reads — and on a 386 a 16 bit multiply
costs what fifteen reads do.

**What you see:** the faces come out somewhat washed out when the palettes are
far apart, and they are perfectly recognisable. The quality depends on which
picture is in charge: a colourful one shares out well, a monochrome one tints the
other five its own colour. That is the limit of the format, not of the code.

---

## Problem 2: perspective distorts textures

Filling a quadrilateral by interpolating the texture in a straight line — what is
called **affine mapping** — is wrong as soon as there is perspective: the part of
the face that is further away has to compress faster than a straight line
compresses.

It shows as a crease along the diagonal of the face, the defect the first
PlayStation's games are recognised by.

The right answer is to divide by the depth at **every pixel**, and that is 25000
divides a frame that do not fit. What is done here is what they did:

```
   face not split             face on a 4x4 grid
   +-------------+            +--+--+--+--+
   |            /|            +--+--+--+--+   25 points projected
   |          /  |            +--+--+--+--+   WITH perspective, and inside
   |  crease     |            +--+--+--+--+   each little one the error
   +-------------+            +--+--+--+--+   no longer shows
```

**150 divides a frame instead of 25000**, and the crease is gone.

And the 25 grid points **do not have to be rotated**: rotation is a linear
operation, so interpolating between the already rotated corners gives exactly the
same answer as rotating the interpolated point. Eight vertices are rotated and
the rest comes out of adds.

---

## Problem 3: it has to be filled

The [wireframe cube](../../cube/doc/README-EN.md) draws twelve lines and costs
4.7 microseconds. This one draws about 25000 textured pixels and costs 50: **ten
times as much for the same geometry.** There is the proof that the expensive part
of 3D is the filling.

That is why there is a real triangle rasteriser here, the most serious piece in
the folder.

**The gradients** are what makes the inner loop cheap. In an affine mapping, how
much `u` advances when you move one pixel right is **the same number over the
whole triangle**: it does not depend on the row. It is worked out once from the
determinant of the three vertices and inside there are only adds:

```c
	du_dx = (((long)(pu[1] - pu[0]) * (long)(py[2] - py[0]))
	      -  ((long)(pu[2] - pu[0]) * (long)(py[1] - py[0]))) / area;
```

Four divides per triangle and not one per pixel.

**The faces looking away are not painted**, using the 2D cross product of two
consecutive sides of the already projected face. One sign means we see it from
the front, the other means from behind. With that there is **no need to sort the
faces or keep a depth buffer**: on a convex body, the faces that pass the test
never cover each other. In any ordinary pose exactly 3 of the 6 pass.

---

## The traps

**Rows off the screen must not be skipped with a bare `continue`.** The edges of
the triangle are walked by adding a slope per row; stop adding it while you are
off screen and, when the triangle comes back on, the edges are where the last
visible row left them and it comes out crooked. You have to **always advance and
only sometimes draw**.

**The last grid point's texture coordinate is 127, not 128.** The rasteriser
masks with `& 127`, so a 128 would come back round to 0 and a stripe of the
opposite edge would show along the corner:

```c
	step_u = ((DEMO_TEXTURE_SIZE - 1) << DEMO_SHIFT) / TEXCUBE_GRID;
```

**The six photographs are read row by row**, without going through a 64000 byte
buffer. It is slower (128 `fseek`s per photograph instead of one) and in exchange
it does not ask for a single large block at the very moment 98304 bytes of
textures are already out. And since a BMP stores the last row first, the file row
is `199 - row`, so no `bmp_revert_bmp()` and no two copies in memory at once.

**The small blocks are asked for after the big ones.** Asked for first they would
leave the heap pockmarked and the six 16384 requests would have to find their way
between the holes. It is the game's 256000 byte map rule, at a smaller scale.

**And the sound card has to be fed during the load**, every 16 texture rows. Six
BMPs and five remaps is the slowest thing this folder does.

## Where the six photographs come from

From the slideshow itself, starting at the one being shown:

```c
	current = demo_image_current();
	... demo_image_path(current + index) ...
```

So every time the cube comes round in the intro it carries six different
photographs. `demo_image_path()` wraps by itself, so with fewer than six pictures
they repeat and nothing breaks. They are two functions of `demos.c` and not a
global array on purpose: the effect can never change the list.

## Experiments

1. Set `TEXCUBE_GRID` to 1. The diagonal crease comes back on every face: it is
   the quickest way to see what the grid is for.
2. Raise it to 8. No difference shows and the cost goes up: that is the point
   where subdividing stops paying.
3. Change `if (cross <= 0)` to `if (cross >= 0)`. The three back faces get
   painted instead of the three front ones and the cube reads inside out.
4. Take the culling `continue` out altogether. All six get painted, the last one
   wins, and the cube becomes a flat quadrilateral.
5. In `texcube_load_face()`, force `is_current = 1` for every face. There you see
   what happens with no remapping: five faces of colour noise.

---

**Previous:** [The wireframe cube](../../cube/doc/README-EN.md) ·
**Index:** [The nineteen effects](../../README-EN.md)
