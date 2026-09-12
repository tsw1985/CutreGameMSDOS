#ifndef DEMOLIB_H
#define DEMOLIB_H

//===========================================================
// The bits every effect needs. Internal to demos\: main.c has no business
// including this, it only wants demos.h.
//
// It exists so that ten effects do not each carry their own copy of a sine
// table, a retrace wait and an "has ESC been pressed" check.
//===========================================================

#include <stdio.h>
#include <string.h>		// memcpy y memset: los usan casi todos los efectos

// The screen. Mode 13h and nothing else, for ever.
#define DEMO_WIDTH 		320
#define DEMO_HEIGHT 	200
#define DEMO_SCREEN 	64000


//-----------------------------------------------------------
// WHAT THIS FOLDER BORROWS FROM THE OUTSIDE
//
// Seven functions of src\bmp.c, and that is the entire dependency list.
//
// They are declared HERE, by hand, instead of by including header\bmp.h,
// and that is on purpose: bmp.h includes players.h, so one #include would
// drag the tanks, the bullets and the collision boxes into a folder that
// has no business knowing they exist. Written out like this, demos\ depends
// on seven FUNCTIONS and not on the game's header tree, and lifting the
// folder into another project means providing these and nothing else.
//
// If a signature here ever stops matching src\bmp.h the linker says so, so
// the copy cannot rot in silence.
//-----------------------------------------------------------
void bmp_paint_image_data_to_vga(char *buffer_image_data);
void bmp_load_pallete_data(char *buffer_data_dest, FILE *file);
void bmp_fill_buffer_with_image_data_from_file(char *buffer_data_dest, FILE *file);
void bmp_revert_bmp(char *bmp_data);
void bmp_write_pallete_data_into_dac(char *pallete_data);
void bmp_write_pallete_data_into_dac_scaled(char *pallete_data, int level);
void bmp_write_black_pallete_into_dac(void);


//-----------------------------------------------------------
// FIXED POINT, because there is not one float in this project and an intro
// is a bad reason to link Turbo C's floating point library.
//
// Everything is 8.8: the low 8 bits are the fraction, so 256 means 1.0 and
// a value of 384 means 1.5. Multiply two of them and you have to shift the
// result back down by 8, or you end up with 16.16 by accident.
//-----------------------------------------------------------
#define DEMO_ONE 		256		// 1.0
#define DEMO_SHIFT 		8		// how far to shift a product back down


//-----------------------------------------------------------
// A whole turn is 256 steps, not 360 degrees. That is deliberate: an angle
// then wraps round with & 255 instead of a modulo, which on an 8086 is the
// difference between one AND and a division.
//
// demo_sine[a] runs from -256 to 256, so it is already 8.8: sine * length
// >> 8 gives you the projection with no division anywhere.
//-----------------------------------------------------------
#define DEMO_ANGLE_STEPS 	256
#define DEMO_ANGLE_MASK 	255

extern int demo_sine[DEMO_ANGLE_STEPS];

// Cosine is the sine a quarter turn along. 256 / 4 = 64.
#define demo_cos(a) 	demo_sine[(((a) + 64)) & DEMO_ANGLE_MASK]
#define demo_sin(a) 	demo_sine[(a) & DEMO_ANGLE_MASK]


//-----------------------------------------------------------
// Row y of a 320 wide screen starts at byte demo_row[y].
//
// It is there to kill a multiply. Working out y * 320 for every pixel of a
// rotozoom is 64000 multiplies a frame; looking the row up in a table is an
// add. On a 386 it is worth it, on an 8086 it is the difference between
// running and crawling.
//-----------------------------------------------------------
extern unsigned int demo_row[DEMO_HEIGHT];

// Fills both tables. demos.c calls it once, before any effect runs.
void demo_tables_init(void);


//-----------------------------------------------------------
// Waiting and getting out
//-----------------------------------------------------------

// One vertical retrace. Its own copy and not the game's, because the game's
// lives inside src\main.c, which has a main() and cannot be linked against.
// Two lines are a cheap price for a folder that travels on its own.
void demo_wait_retrace(void);


//-----------------------------------------------------------
// END OF FRAME. Every effect calls one of these two exactly once a turn of
// its loop, and that is not a convenience, it is the whole point.
//
// An effect that runs for eight seconds is 560 frames, and during all of
// them the Sound Blaster is reading the DMA buffer on its own. If nobody
// refills the half it has just finished, the card plays that same half
// again, and again: the music gets stuck in a loop of a fraction of a
// second and stays there until the effect ends.
//
// The first version of this folder called sound_update() in the fades and
// NOT inside the effects, and that is exactly what it sounded like. Putting
// it inside the one call every effect already makes per frame is what stops
// it from happening again to the eleventh effect somebody writes.
//
// sound_update() is cheap: it returns immediately unless the card has
// actually finished a half, which at 512 samples happens about 3 times a
// second against 70 frames.
//-----------------------------------------------------------

// Waits for the retrace, puts the frame on screen and feeds the card.
//
// The order matters: the 64000 byte copy to video memory goes FIRST, while
// the beam is still in the vertical blanking, and the sound is fed with
// what is left of the frame. The other way round, mixing would eat the
// blanking window and the picture would tear.
void demo_show(unsigned char *screen);

// Feeds the card AND runs the idle callback: the end of a frame for an
// effect that does not repaint. Only cycle needs it directly: it paints once
// at the start and after that all it changes is the palette, and the palette
// write has to stay inside the blanking.
//
// demo_show() calls this one, so there is exactly ONE place in the folder
// where a frame is accounted for. Same reasoning as the sound: what lives in
// the single funnel cannot be forgotten by the eleventh effect.
void demo_sound(void);

// 1 if ESC is down. Read through the BIOS and not through an INT 9 handler
// of our own, on purpose: an interrupt vector is exactly the kind of thing
// that makes a module impossible to lift into another program. It also
// means the demo has to run BEFORE the game installs its own handler.
int demo_escape_pressed(void);

// The BIOS tick counter, 18.2 a second. What every effect uses to know when
// its time is up.
unsigned long demo_now(void);

// Empties whatever is sitting in the BIOS keyboard buffer, so the key that
// ended one effect does not immediately end the next one too.
void demo_flush_keys(void);


//-----------------------------------------------------------
// THE SHAPE OF AN EFFECT
//
// Every effect looks exactly like this, which is what lets demos.c keep
// them in a plain array of function pointers and pick one per picture. Add
// an effect, add a line to that table, and there is nothing else to touch.
//
//   image      the picture, 320x200, one byte per pixel. READ ONLY: the
//              same buffer is handed to the next effect.
//   screen     where to paint, 320x200. Yours to destroy.
//   palette    the picture's own 256 colors, already in the DAC. Only the
//              effects that play with the palette touch it.
//   end_tick   the value of demo_now() at which to stop.
//
// Returns 1 if it ran its time out, 0 if ESC was pressed.
//-----------------------------------------------------------
typedef int (*demo_effect_fn)(unsigned char *image,
                              unsigned char *screen,
                              unsigned char *palette,
                              unsigned long end_tick);

#endif
