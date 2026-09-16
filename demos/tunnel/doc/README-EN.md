# tunnel — The endless tunnel

*[Versión en español](README.md)*

**Effect 15 of 19 of the walkthrough.** Measured cost: **77 microseconds per
frame**, plus **2 milliseconds** of building the tables on the way in.

The code is in [`../tunnel.c`](../tunnel.c).

---

## What it does

Flying down the inside of a tube lined with the picture, never coming out.

It is *the* effect of the 90s intros alongside the rotozoom, and the one that
best teaches the idea half this folder rests on: **when a sum is expensive and
never changes, you do not optimise it, you precompute it.**

## The sum that cannot be done

For every pixel of the screen you need two things about the tube:

```
   angle    where it falls around the tube  ->  atan2(dy, dx)
   depth    how far away it is              ->  k / sqrt(dx2 + dy2)
```

An **arctangent** and a **square root** per pixel. That is 64000 pixels at 70
frames a second: four and a half million square roots a second on a 386. It is
not that it is slow, it is that it does not exist.

## The idea: neither of them ever changes

The angle and the depth of a pixel depend on **where the pixel is**, and the
pixel does not move.

The only thing that changes with time is how far down the tube we are and how
much we have turned, and that is **two numbers for the whole screen**, not two
numbers per pixel.

```
   ON THE WAY IN (once)         EVERY FRAME (64000 times)
   --------------------         -------------------------
   depth[i] = k / radius        depth[i] + advance
   angle[i] = atan2(dy,dx)      angle[i] + spin
   64000 square roots           two adds and two ANDs
   64000 arctangents
```

```c
	screen[offset] = texture[
		(((depth[offset] + step_depth) & TUNNEL_MASK) << DEMO_TEXTURE_SHIFT)
	  +  ((angle[offset] + step_angle) & TUNNEL_MASK)
	];
```

Two table reads, two adds, two ANDs and one texture read. **Not a root, not a
divide, not a multiply.** The whole movement of the tunnel is the two variables
`advance` and `spin`.

It costs **128000 bytes of memory**, and that is the deal: memory for time. It
is the same deal `demo_row[]` makes in `demolib.h`, only on a much larger scale.

## And the tables are built without `math.h`

Not one `float` gets in here, so both functions have to be made from scratch.

**The square root, bit by bit.** The schoolbook method in base 2: try putting a
bit in the root starting from the highest and keep the one that does not
overshoot. That is 8 turns for a 16 bit number, against the 189 that "add one
until you overshoot" would take.

**The arctangent, 65 numbers.** Only **an eighth of a turn** is needed: 16 of the
128 steps. Everything else comes out of symmetry, because a circle is the same
eighth repeated eight times and flipped.

```c
	if (ay <= ax){
		base = tunnel_atan[(ay * 64) / ax];                    // first eighth
	}else{
		base = TUNNEL_QUARTER - tunnel_atan[(ax * 64) / ay];   // the same one, flipped
	}
```

That turns a transcendental function into 65 numbers and four sign comparisons.
Both divides are **by the long side, always**, which is what guarantees the ratio
lands in 0..64 and that nothing is divided by zero.

## The traps

**A whole turn of the tube is 128 steps, not 256.** All the rest of the folder
uses 256 step angles. Not here, and it is not a whim: 128 is the width of the
texture, so **the angle already is the texture column** and does not need
scaling. One scaling step per pixel would be 64000 multiplies a frame.

**The exact centre has radius 0.** The depth would be infinite and the divide
would blow up. It gets radius 1, which leaves it the furthest pixel of all.

**And the sound card has to be fed while the tables are built.** That is 64000
square roots and 64000 arctangents, the slowest thing this folder does after
loading the photo cube. Without the `demo_sound()` every 16 rows the music would
loop during the build. It is the same reason `demos.c` calls `demo_sound()` on
both sides of loading a BMP.

## A radial seam shows

The texture does not fit against itself sideways, so where the angle wraps round
(from 127 to 0) a line shows. It is the same price
[carpet](../../carpet/doc/README-EN.md) pays and every tunnel of the era paid it.

## Cost

The dearest of the fifteen, and honestly so: it writes all 64000 pixels every
frame without saving a single one. In exchange the inner loop is one of the
simplest in the folder — all the difficulty is in the build.

## Experiments

1. Set `TUNNEL_SPIN` to 0. It stops turning and only the advance is left: that is
   where you see they are two independent movements added to two tables.
2. Raise `TUNNEL_DEPTH` to 8192. The tube stretches and far more rings pack into
   the far end.
3. Change `radius < 1` to `radius < 20`. A flat disc appears in the middle: that
   is the back of the tunnel ceasing to be infinite.
4. Replace the integer root with `ax + ay / 2` (the approximation the game's
   radar uses). The tunnel stops being round and becomes a diamond. It is a good
   way to see how much the real root matters.

---

**Previous:** [The rotozoom that never ends](../../carpet/doc/README-EN.md) ·
**Next:** [The picture wrapped round a spinning ball](../../sphere/doc/README-EN.md) ·
**Index:** [The nineteen effects](../../README-EN.md)
