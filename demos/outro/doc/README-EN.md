# outro — The goodbye card

*[Versión en español](README.md)*

**It is not one of the twenty effects.** It is the epilogue: it plays once, at
the end, and it is not part of the walkthrough.

The code is in [`../outro.c`](../outro.c). The picture is
[`res/demo/fin.bmp`](../../../res/demo/fin.bmp).

---

## What it is for, and it is not decoration

Without it the intro fades out and the game starts **in the same instant**. In
front of an audience that is a jolt: no time to say *"right, and this is the
game"*, no time for anyone to get ready.

The epilogue turns that cut into a stop. The intro ends, the card comes up, and
the game starts when you want it to.

```
   before:  ...last picture  ->  [fade]  ->  the game, just like that
   now:     ...last picture  ->  [fade]  ->  card  ->  key  ->  the game
```

## Why it is not in `demo_effects[]`

Because it does not end by itself. The twenty effects are handed an `end_tick`
and stop on their own; this one waits for somebody to press a key, and in the
table it would hang the slideshow on whichever picture it landed on.

So it has **a different signature** — no `end_tick` — and `demo_run()` calls it
by hand, at the end and once:

```c
	if (finished == 1){
		if (demo_load_image(DEMO_ENDING_IMAGE, image, palette) == 1){
			...
			demo_outro(image, screen, palette);
			...
		}
	}
```

**Only if the show reached the end.** If you left with ESC it is because you
want to play *now*, and putting up a card that asks for another key would be
exactly the opposite of what you asked for.

And it reuses **the same three blocks of memory** that were already out: not one
byte more than the usual 128768.

## Why the zoom is so short

The rest of the folder distorts the picture thoroughly, which is the point. Not
here: this is a card, and a card has to be **readable**. A rotozoom over text
makes it illegible within two frames.

So it takes the [zoom](../../zoom/doc/README-EN.md), the one distortion that
**twists nothing** — everything grows equally in both directions, letters stay
letters — and cuts its travel down to almost nothing:

```
   normal zoom:   0.6 to 2.2 magnifications
   epilogue:      1.00 to 1.09
```

Just enough that the screen does not look frozen.

## And inwards only, never outwards

The scale never goes above 256, which is 1:1. Zooming out, the picture would
cover less than 320 pixels and **a black frame would pulse** around the card,
which is exactly the sort of thing that pulls your eye off what it says.

Always zooming in, the screen is always full, there are no borders to fill and
the inner loop is left without a single `memset`. The normal zoom spends half a
function on that.

## The picture's safe area

At 1.09 magnifications only the **middle 91%** of the picture is visible.
Anything that has to be read has to fit in there:

```
   the full 320 x 200
   +--------------------------------+
   |   what shows at 1.09           |
   |   +------------------------+   |   x from 14 to 306
   |   |   text safe area       |   |   y from  8 to 192
   |   |  x 32..288  y 28..172  |   |
   |   +------------------------+   |
   +--------------------------------+
```

`fin.bmp` is drawn against those numbers: its frame sits at x 24..295 and
y 20..179, inside what shows, and the text a good deal further in. **If you
change the picture, respect the inner rectangle** or the zoom will eat the
letters.

And if you raise `OUTRO_SCALE_AMP` the arithmetic changes. The clamp in the
column table stops it reading outside the buffer, but it does not stop the text
walking off the screen.

## Changing the card

Any BMP of **320x200, 256 colours and exactly 65078 bytes**, called `fin.bmp`
and dropped in `res/demo/`. Index 0 has to be black, as everywhere in this
folder.

If the file is not there nothing happens: `demo_run()` skips it and the game
starts as it always did.

## Any key, and the other machine's too

`demo_any_key_pressed()` answers to any key, not just ESC. And it checks
`demo_aborted` for the same reason `demo_escape_pressed()` does: over a network
the machine **waiting** for the one playing the intro has only ESC to say *"get
on with it"*, and that ESC reaches here through `demo_sound()`'s callback.

That is also why the wait can last as long as it needs to without dropping the
match: `demo_show()` calls `demo_sound()` every frame, and the network heartbeat
lives in there.

---

**Index:** [The twenty effects](../../README-EN.md)
