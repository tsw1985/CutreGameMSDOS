#ifndef DEMOS_H
#define DEMOS_H

//===========================================================
// THE DEMO PLAYER
//
// A little slideshow of 90s screen effects. You hand it a list of 256 color
// BMP files and it shows them one after another, each one through a
// different effect, with the music playing and ESC to get out.
//
// It is meant to be an intro, and it is meant to be STEALABLE. This whole
// folder only ever calls src\bmp.c (loading a BMP, the palette, the DAC,
// blitting to the screen) and src\sound.c (the music). It knows nothing
// about tanks, players, the map or the network, and there is no header of
// the game in any file under demos\. Copy the folder into another project
// with those two modules and it works.
//
//
// MEMORY: NOTHING EXISTS UNLESS YOU ASK FOR IT
//
// demo_run() reserves what it needs when it is called and hands ALL of it
// back before it returns, sound included. Called with no /demo on the
// command line it never runs, and not one byte is spent.
//
// That is not tidiness, it is the one rule this project learned the hard
// way. The game needs 256000 CONTIGUOUS bytes for its map, and a block
// reserved and freed in the middle of the heap leaves a hole that a 256000
// byte request cannot use. So the demo has to run BEFORE the game reserves
// anything, and it has to give everything back before the game starts:
//
//     arguments  ->  demo_run()  ->  the game starts on a clean heap
//
// The same goes for the sound. Leaving the card up with its DMA buffer
// alive while the map is asked for is exactly the mistake that once left
// the map without contiguous memory. demo_run() closes the sound on its way
// out and the game opens it again, which means the music restarts when the
// match begins.
//===========================================================

// How long each picture stays on screen, in seconds, when the caller does
// not say. Eight is about right: long enough to watch the effect breathe,
// short enough that you are not bored before the next one.
#define DEMO_DEFAULT_SECONDS 	7

// The music. Streamed from disk by src\\sound.c, so a long song costs the
// same as a short one. It has to be an 8 bit mono 44100 Hz WAV: unlike the
// effects, the streaming code cannot convert the rate on the fly.
#define DEMO_SONG 				"..\\res\\demo.wav"

// How loud the intro plays, out of SOUND_VOLUME_MAX, which is 32.
//
// play_song() would leave it at 16, half, and half is the right answer for
// the MATCH: there the song is competing with engines and shots and has to
// leave them room. During the intro there is nothing else making a noise, so
// the number is free to be about the recording instead.
//
// And it has to be, because this one is loud. Measured against the song the
// game plays:
//
//     prody8.wav   peak 118/127   RMS 18.9
//     demo.wav     peak 128/127   RMS 39.0
//
// Twice the level, and already clipped in the file itself: a peak of 128 is
// past what a signed 8 bit sample holds, and 0.04% of it sits on the rail.
// Eight is what makes it as loud as the game's own song used to be.
//
// If it still sounds gritty this low, that is the WAV and not the mixer.
// Nothing here can undo clipping that was baked in before the file arrived.
#define DEMO_SONG_VOLUME 		16

// How long the fade between two pictures takes. Every picture brings its
// OWN palette and the VGA only has one, so two of them can never be on
// screen together: the way from one to the next HAS to go through black.
// That is not a workaround, it is what the intros of the time did.
#define DEMO_FADE_STEPS 		32


//-----------------------------------------------------------
// THE IDLE CALLBACK
//
// Called once per frame, from demo_sound(), which is the one place every
// effect passes through. It is how the caller gets a word in edgeways during
// an intro that runs for over two minutes without returning.
//
// The game uses it to keep the network alive: over a network the OTHER
// machine is sitting there waiting for this intro to end, and ten seconds
// without a packet is all it takes for it to decide the connection is gone.
//
// It is a plain function pointer and not a call into net.c on purpose. This
// folder does not know that a network exists, and that is what keeps it
// liftable into another project: there, nobody sets the callback and nothing
// happens.
//
// Return 1 to CUT THE INTRO SHORT. It comes out of demo_run() exactly as if
// ESC had been pressed, which is what lets the player on the other machine
// skip an intro that is not even running on their screen.
//-----------------------------------------------------------
typedef int (*demo_idle_fn)(void);

void demo_set_idle(demo_idle_fn callback);


//===========================================================
// Plays the whole thing.
//
//   image_paths     array of file names, DOS style ("..\\res\\demo\\a.bmp")
//   image_count     how many there are
//   seconds_each    how long each one lasts, 0 for DEMO_DEFAULT_SECONDS
//
// Every file has to be a 320x200 BMP with 256 colors, which is what the
// rest of the game reads too. A picture that fails to open is skipped and
// the show carries on.
//
// Each picture gets a DIFFERENT effect, taken in turn from the table in
// demos.c. With more pictures than effects the list starts again, which is
// on purpose: fifteen pictures through ten effects is fine.
//
// Returns 1 if it played to the end, 0 if ESC was pressed. Either way
// everything is back the way it was found: the memory, the sound card, and
// the screen, which is left in 80x25 text so that whoever called can still
// print something.
//===========================================================
int demo_run(char **image_paths, int image_count, unsigned int seconds_each);

#endif
