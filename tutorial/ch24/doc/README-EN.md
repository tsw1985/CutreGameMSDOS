# Chapter 24 — The proximity radar: numbers made of sprites

*[Versión en español](README.md)*

Chapter 22 gave you a camera that follows **your** tank. That solved one problem
and created another: in a world four screens wide the other tank is almost
always outside what you can see, and finding him means wandering at random until
you trip over him.

This chapter fixes it with a number at the bottom of the screen, from **000%** to
**100%**, that tells you how close the enemy is.

```
make
chap24
```

| Key | |
|---|---|
| arrows | drive the **blue** tank (the one the camera follows) |
| W A S D | drive the **red** tank, so you can watch the number change |
| **T** | turns the black outline off and on |
| ESC | quit |

---

## 1. Why sprites and not `printf`

In mode 13h there **is no text**. The screen is a buffer of 64,000 bytes where
each byte is one pixel, and the BIOS has no function to put a letter there that
is not painting it pixel by pixel.

So a number on screen is exactly the same thing as a tank: **a sprite cut out of
a BMP**. The only new part is deciding *which* of the eleven to draw where.

## 2. The number sheet

`res\Numbers\<THEME>\numbers.bmp` is a 320x200 sheet **just like
`sprites.bmp`**: 54 bytes of header, the palette at 54, the pixels at 1078. It is
opened and cut with the same two functions from chapter 4.

The only thing you need to know is the grid:

```
   +--------+--------+--------+--------+     +--------+
   |   0    |   1    |   2    |   3    | ... |   %    |
   | 18x18  | 18x18  | 18x18  | 18x18  |     | 18x18  |
   +--------+--------+--------+--------+     +--------+
   x=0      x=18     x=36     x=54           x=180
```

**Eleven 18x18 cells in a single row**, cell N starting at `x = N * 18`. The ten
digits in order, and the `%` at the end.

And one detail that decides a constant further down: inside its 18 wide cell,
the ink of each figure covers **rows 2 to 15** and between 8 and 14 columns:

| | Ink (real width) |
|---|---|
| the digits | from 8 px (the `1`) to 13 px |
| the `%` | 13 or 14 px |
| the cell | **18 px** |

Which means **every cell has air on both sides**. Draw the figures 18 pixels
apart and the number reads as three separate digits instead of one number. That
is why this is in `header\players.h`:

```c
#define NUMBER_WIDTH 			18
#define NUMBER_HEIGHT 			18
#define NUMBER_TOTAL_SPRITES 	11
#define NUMBER_PERCENT_CELL 	10

#define NUMBER_ADVANCE 			13
```

`NUMBER_ADVANCE` is **not** `NUMBER_WIDTH`, and that is the whole point: the
cells overlap by five pixels and nothing happens, because color 0 is transparent
and only the ink is ever written.

```
   at 18 (the cell width):     0   5   0   %      <- three loose digits
   at 13 (NUMBER_ADVANCE):    050%               <- a number
```

## 3. The eleven pointers, and where they live

```c
extern char *number_0;
extern char *number_1;
/* ... */
extern char *number_percent;
```

Declared in `header\players.h`, **defined in `src\main.c`** (and in this chapter,
in `chap24.c`). An `extern` says *"this exists somewhere"*, and the linker finds
it wherever it is.

### Why they are NOT inside `struct player`

That was the first idea and it is the wrong one. The radar is **one sign for the
whole screen**, not something each tank owns. Inside the struct there would be
**two identical sets** of eleven sprites, one per player:

| | Bytes |
|---|---|
| One set of 11 sprites of 18x18 | 3,564 |
| Inside `struct player`, with two players | **7,128** |

Twice the memory to draw exactly the same thing. On a 640 KB machine you do not
do that.

## 4. Allocating them: the **all or nothing** pattern

```c
	char **target[NUMBER_TOTAL_SPRITES];
	unsigned int cell;

	target[0]  = &number_0;
	target[1]  = &number_1;
	/* ... */
	target[NUMBER_PERCENT_CELL] = &number_percent;

	for (cell = 0; cell < NUMBER_TOTAL_SPRITES; cell++){

		*target[cell] = (char *)malloc(NUMBER_WIDTH * NUMBER_HEIGHT);

		if (*target[cell] == NULL){
			free_sprite_numbers();     /* hand back what there already was */
			return 0;
		}

	}
```

That `char **target[11]` is an **array of pointers to pointer**: each slot holds
*the address of one of the global variables*. That lets the loop write into
`number_0`, `number_1`... without eleven repeated lines of `malloc` each with its
own `if`.

And the important part is the `free_sprite_numbers()` inside the `if`: **either
all eleven are there or none are**. A half filled set would have a `NULL` in the
middle of it, the code would draw it and hang. This way, if memory runs out there
simply is no radar and the game runs exactly as it did before the radar existed.

The cutting is another loop, with the usual cutter:

```c
	bmp_open_sprite_sheet(file);

	for (cell = 0; cell < NUMBER_TOTAL_SPRITES; cell++){
		bmp_extract_sprite(cell * NUMBER_WIDTH, 0,
		                   NUMBER_WIDTH, NUMBER_HEIGHT,
		                   *target[cell]);
	}

	bmp_close_sprite_sheet();
```

## 5. When, during startup

In the game, `init_sprite_numbers()` is called **right after `init_graphics()`**,
for three reasons:

1. **The theme is not decided until then.** `theme_folder` is chosen inside
   `init_graphics()`, and without a theme there is no way to know which
   `numbers.bmp` to open.

2. **There is only one `FILE *`.** `bmp_open_sprite_sheet()` works on a single
   global variable in `src\bmp.c`, and it is not released until
   `init_graphics()` has cut out the last tank. Opening `numbers.bmp` before that
   would take the tank sheet's handle away.

3. **The big one first.** It is 3,564 bytes asked for *after* the 256,000 of the
   map. That is the rule from chapter 23, and here it holds by itself.

And the `free` goes at the end, next to the `player_free()` calls.

## 6. The palette: why the radar needs a theme

This is the part you do not see coming.

The DAC is loaded from the palette **of the map**. And `numbers.bmp` is painted
in the palette of **its** theme. They are the same one, so the figures come out
in the colors the artist chose.

Mix them and they do not. Comparing entry by entry:

| | Palette entries that differ |
|---|---|
| SKYNET's `numbers.bmp` against `map_sky.bmp` | **0** out of 256 |
| SKYNET's `numbers.bmp` against `big.bmp` (the original map, no theme) | **254** out of 256 |

With an unthemed map the color indexes of the figures would land on colors that
have nothing to do with them, and the number would come out in random colors.
That is why in the game the radar **only exists with `-sky`, `-war` or `-neon`**,
and plain `/bigmap` draws nothing. It is not a limitation: there would be nothing
to read.

## 7. **The idea of this chapter: the subtraction that is not done**

Since chapter 20, **everything** that gets drawn carries the same arithmetic:

```
   screen = world - camera
```

```c
	draw_sprite_to_buffer(sprite, TANK_WIDTH, TANK_HEIGHT,
	                      (int)tank.position_x - camera_x,     /* <-- */
	                      (int)tank.position_y - camera_y,     /* <-- */
	                      buffer_background_image_data);
```

The tanks carry it. The bullets carry it. The explosion carries it.

**The radar does not.**

```c
	draw_sprite_to_buffer(figure[cell], NUMBER_WIDTH, NUMBER_HEIGHT,
	                      RADAR_X + (cell * NUMBER_ADVANCE),   /* no camera */
	                      RADAR_Y,                             /* no camera */
	                      buffer_background_image_data);
```

And it is not an oversight: **the radar is not *in* the world, it is on the
screen.** Its place is the same four cells whatever happens.

This is what a **HUD** (heads-up display) is, and the rule that separates it from
everything else is exactly this:

> If a thing has a position in the world, subtract the camera.
> If it is stuck to the screen, do not.

It is the same idea as chapter 22 seen from the other side. There we said: *"if
the camera decides where something goes, that something is decoration"*. The
radar is pure decoration.

### Where it sits on the screen

```c
#define RADAR_DIGITS			3
#define RADAR_CELLS				(RADAR_DIGITS + 1)
#define RADAR_WIDTH				(((RADAR_CELLS - 1) * NUMBER_ADVANCE) + NUMBER_WIDTH)

#define RADAR_MARGIN_BOTTOM		2
#define RADAR_X					((WIDTH - RADAR_WIDTH) / 2)
#define RADAR_Y					(HEIGHT - NUMBER_HEIGHT - RADAR_MARGIN_BOTTOM)
```

`RADAR_WIDTH` is **not** `4 * 18`. The first three cells only advance 13, but the
last one takes its full width:

```
   |<-13->|<-13->|<-13->|<----18---->|
   [  0   ][  0   ][  9  ][     %    ]
   |<--------- 57 pixels ----------->|
```

Hence `RADAR_X = (320 - 57) / 2 = 131` and `RADAR_Y = 200 - 18 - 2 = 180`. They
are all **constants**: the compiler works them out once and nothing is divided at
run time.

And it is drawn **last of all**, after tanks and bullets, so nothing can be
painted over it.

## 8. From one number to four sprites

```c
	value = percent;

	for (cell = RADAR_DIGITS - 1; cell >= 0; cell--){
		figure[cell] = digit[value % 10];
		value = value / 10;
	}

	figure[RADAR_DIGITS] = number_percent;
```

Right to left, which is how you take a number apart:

- `% 10` gives the units digit
- `/ 10` throws that digit away and leaves the rest

With **50**:

| Pass | `value` | `value % 10` | Goes to | `value / 10` |
|---|---|---|---|---|
| 1 | 50 | **0** | cell 2 | 5 |
| 2 | 5 | **5** | cell 1 | 0 |
| 3 | 0 | **0** | cell 0 | 0 |

Result: `[0][5][0][%]` → **050%**

### Why always three digits, with leading zeros

The leading zero **comes out by itself**: the loop runs three times whatever
happens, and once `value` is 0, `0 % 10` is still 0. Nothing has to be padded by
hand.

And that is what you want. If the number changed width when it crossed from 9 to
10, a centered number would **jump sideways**:

```
   variable width:      9%      ->     10%     ->    100%
                      (centered)    (centered)     (centered)
                         ^ every jump moves the sign

   fixed width:        009%     ->     010%     ->   100%
                         ^ the sign never moves
```

A sign that jumps is a sign you look at instead of playing.

## 9. The proximity algorithm

### First: no floating point

The real distance between two points is:

```
   distance = sqrt(dx*dx + dy*dy)
```

That square root is a `float`, and in this project there is **not one single
float**. Adding one makes Turbo C link its whole floating point library into a
program that is counting its bytes. Not for a sign.

The classic integer approximation is:

```
   distance = bigger + (smaller / 2)
```

```c
	if (dx < dy){
		swap = dx;
		dx = dy;
		dy = swap;
	}

	distance = (long)dx + ((long)dy / 2L);
```

It lands within **11%** of the real distance and costs a compare, an add and a
shift.

### The other candidates, and why not

| Formula | Name | What it gets wrong **here** |
|---|---|---|
| `\|dx\| + \|dy\|` | Manhattan | Punishes diagonals: a tank on the diagonal reads much further away than one straight ahead at the same real distance |
| `max(\|dx\|,\|dy\|)` | Chebyshev | The opposite: rewards diagonals |
| `bigger + smaller/2` | the one chosen | Behaves like the real distance in every direction |

And the diagonal matters a lot, because the two tanks start **in opposite
corners**: the other one is almost always diagonally away.

Measured by running the real code:

| Situation | Reading |
|---|---|
| 100 px horizontally | **88%** |
| 100 px vertically | **88%** |
| 100 px diagonally | **87%** |
| 100 px horizontally, the other way round | **88%** |

One point of difference between straight and diagonal. That is what we were
after.

### The scale

```c
	if (map_width > map_height){
		worst_distance = (long)(map_width  - TANK_WIDTH)
		               + ((long)(map_height - TANK_HEIGHT) / 2L);
	}else{
		worst_distance = (long)(map_height - TANK_HEIGHT)
		               + ((long)(map_width  - TANK_WIDTH) / 2L);
	}

	percent = 100L - ((distance * 100L) / worst_distance);
```

The same formula over the whole world, taken from `map_width` and `map_height`.
**There is no 640 written by hand anywhere**: the day you load a map of another
size, the radar recalibrates itself. And the `- TANK_WIDTH` is there because a
tank is a box and not a point: its corner can never reach the real edge.

On the 640x400 map that gives a scale like this:

| Situation | Reading |
|---|---|
| Corner to corner | 7% |
| The game's two spawns | **50%** |
| Touching side by side (18 px) | 98% |
| On top of each other | 100% |

### Trap 1: the cast to `int` **before** subtracting

```c
	dx = (int)a->position_x - (int)b->position_x;
```

`position_x` is `unsigned int`. If tank A is **to the left** of tank B, the
subtraction in unsigned does not go negative: it **wraps round the bottom** and
comes out as 65,000-something.

```
   without the cast:   100 - 300  ->  65336   ->  huge distance  ->  0%
   with the cast:      100 - 300  ->    -200  ->  abs = 200      ->  right
```

The radar would read 0% every time the tanks happened to be the wrong way round.
It is the same trap as the sprite clipping in chapter 4, and as
`player_add_offset()` in chapter 7.

### Trap 2: the arithmetic has to be in `long`

```c
	percent = 100L - ((distance * 100L) / worst_distance);
```

`distance * 100` reaches about **78,000** on the 640x400 map. A 16 bit `unsigned
int` stops at **65,535**.

In 16 bits that multiplication **wraps round**, the percentage comes out
nonsense, and the radar reads a cheerful **100% at the far end of the world**:
the exact opposite of what it is supposed to say. That is why the three variables
are `long` and the literal is written `100L`.

## 10. The outline

The radar has **no background of its own**: it lands on whatever piece of the map
the camera happens to be showing. Over the dark floor of SKYNET the figures read
beautifully; over the pale stone wall of MILITAR they nearly vanish.

And that cannot be fixed by choosing a better spot, because the spot is chosen by
the player moving around.

The answer is a **black outline**, and drawing one needs a new function in
`src\bmp.c`:

```c
	if(sprite[src_offset] != 0) {
		dest_buffer[dest_offset] = color;     /* <-- one flat color */
	}
```

`draw_sprite_silhouette_to_buffer()` is `draw_sprite_to_buffer()` word for word,
clipping included, with **one byte changed**: instead of copying the sprite's own
color, it writes the color you hand it.

> The **shape** of the sprite says **where** to write.
> The `color` parameter says **what** to write.

It is drawn four times, one pixel out on each side:

```c
static int radar_outline_x[4] = { -1,  1,  0,  0 };
static int radar_outline_y[4] = {  0,  0, -1,  1 };
```

Not the diagonals: they would cost half again for a thickness the eye does not
see at this size.

### And color 0 is not a problem

The outline is drawn in **color 0**, the transparent one. It sounds like a bug
and it is not:

- **transparent** is what is read **from the sprite**
- **0** is what is written **to the screen**, and there it is an ordinary black

It was chosen because it is **pure black in the palette of all three themes**,
checked entry by entry. So no per-theme outline color is needed.

### The two passes cannot be merged

```c
	for (cell ...) { the 4 outlines }      /* pass 1: ALL the outlines */
	for (cell ...) { the figure }          /* pass 2: ALL the figures  */
```

The figures are 13 pixels apart and their ink is up to 14 wide, so every cell
**overlaps the one before it**. If each figure were outlined and filled before
moving on to the next, the black outline of one would eat the right hand edge of
the one already painted:

```
   right:  outline outline outline outline
           figure  figure  figure  figure

   wrong:  outline figure  outline figure  ...
                           ^ this outline eats the previous figure
```

Press **T** in the chapter program and drive down to the bottom wall: you will
see it in one second.

## 11. The radar and the network: it does not exist

The radar comes out of two positions **both machines already simulate
identically**, so both of them work out the same number without one byte crossing
the wire.

And because it is decoration, it **does not go into the checksum** from chapter
19. Same rule as `camera_x`:

> If the drawing code decides it, it is not part of the game state.

If it did go in, nothing bad would happen *today*... until the day somebody
changed the formula on one machine and not the other, and the game reported a
desync over a sign.

## 12. Experiments

1. **Press T** and stand on the bottom wall. That is the outline's reason to
   exist.
2. **Set `NUMBER_ADVANCE` to 18** and rebuild. You will see three loose digits
   instead of a number.
3. **Remove the `(int)` from the subtraction** in `compute_proximity_percent()`
   and take the red tank to the left of the blue one. The radar falls to 0%.
4. **Change `100L` to `100`** and go to the far end of the map. There is the 16
   bit overflow.
5. **Change the formula to `dx + dy`** (Manhattan) and compare a diagonal with a
   straight line at the same distance.
6. **Take the camera subtraction away from the tanks** and give it to the radar.
   You will see both things wrong at once, which is the best way to understand
   the rule.

## 13. What to take away

| | |
|---|---|
| In mode 13h there **is no text** | A number is sprites, like everything else |
| A sheet of N equal cells | `cell N starts at N * width` |
| The **advance** is not the cell width | The ink does not fill the cell |
| **HUD = do not subtract the camera** | If it is stuck to the screen, it is not in the world |
| `% 10` and `/ 10` | Taking the digits out of a number, right to left |
| Fixed width with leading zeros | A sign that does not jump |
| `bigger + smaller/2` | A decent distance with no sqrt and no `float` |
| Cast to `int` **before** subtracting `unsigned` | Or the subtraction wraps round |
| `long` the moment you multiply by 100 | 16 bits run out at 65,535 |
| The silhouette in a flat color | The shape says where, the color says what |
| Decoration stays out of the checksum | Same rule as the camera |

---

## End of the course

Now for real. You have seen the whole road: from a black screen to two tanks
fighting over a network across a world four screens wide, with a sign that tells
you whether you are getting warmer.

To go deeper:

- [Camera manual](../../../doc/EN/CAMERA-MANUAL.md) — 60 pages on chapters 20-24
- [Network manual](../../../doc/EN/NETWORK-MANUAL.md) — the same for chapters 15-19
- [Sound tutorial](../../../doc/EN/SOUND-TUTORIAL.md) — for reusing `sound.c`
- [Network tutorial](../../../doc/EN/NETWORK-TUTORIAL.md) — for reusing `net.c`

---

**Previous:** [Chapter 23](../../ch23/doc/README-EN.md) ·
**Index:** [The course](../../README-EN.md)
