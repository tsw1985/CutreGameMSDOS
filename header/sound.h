#ifndef SOUND
#define SOUND

//===========================================================
// Sound Blaster playback, with several sounds at the same time.
//
// The card has ONE DAC: the hardware cannot play two sounds at once. What
// this module does is MIX them by software, adding up the samples of every
// active voice into the single buffer the DMA is playing. That is the only
// way to hear an engine and a shot together.
//
// Everything is 8 bit mono PCM, and every sound comes out at
// SOUND_SAMPLE_RATE. A WAV recorded at another rate is resampled when it is
// loaded, since the DSP only has one output rate.
//
// If there is no card, sound_init() returns 0 and every other call here
// does nothing. The game runs exactly the same, without sound.
//===========================================================

// The sounds that get loaded from res\, one entry per WAV file
#define SOUND_SAMPLE_FIRE 		0
#define SOUND_SAMPLE_ENGINE_1 	1
#define SOUND_SAMPLE_ENGINE_2 	2
#define SOUND_SAMPLE_DIED 		3
#define SOUND_TOTAL_SAMPLES 	4

// The voices that can sound at the same time. A voice is a slot: it holds
// which sample it is playing and how far into it it has got. Giving each
// thing its own fixed voice means a shot can never cut off an engine, and
// firing twice only cuts off that player's own previous shot.
#define SOUND_VOICE_ENGINE_1 	0
#define SOUND_VOICE_ENGINE_2 	1
#define SOUND_VOICE_FIRE_1 		2
#define SOUND_VOICE_FIRE_2 		3
#define SOUND_VOICE_EXPLOSION 	4
#define SOUND_TOTAL_VOICES 		5

// Volume of each voice, out of 64. The engines are turned down so a shot
// can be heard over them, and because two engines running flat out plus a
// shot would clip badly when added together.
#define SOUND_VOLUME_FIRE 		64
#define SOUND_VOLUME_ENGINE 	34
#define SOUND_VOLUME_DIED 		64

// Starts everything up: finds the card, loads the WAVs, and starts the DMA
// running on silence. Returns 1 if there is sound, 0 if there is not.
int sound_init();

// Stops the card and gives the memory back. MUST be called before leaving
// the program, or the DMA carries on reading memory that is no longer ours.
void sound_shutdown();

// Refills whichever half of the buffer the card has just finished playing,
// mixing every active voice into it. Has to be called once per frame from
// the main loop: if it stops being called the sound stutters, because the
// card just replays the half it already has.
void sound_update();

// Plays a sample once on a voice, from the start. If that voice was already
// sounding it is cut off.
void sound_play(int voice, int sample_id, int volume);

// Plays a sample on a loop, for as long as the engine is running. Calling
// it again with the same sample on the same voice does NOTHING, so it can
// be called every frame without the sound restarting over and over.
void sound_loop(int voice, int sample_id, int volume);

// Silences one voice, or all of them
void sound_stop(int voice);
void sound_stop_all();

#endif
