#ifndef SOUND
#define SOUND

//===========================================================
// SOUND BLASTER LIBRARY
//
// Plays several 8 bit mono PCM sounds at the same time. It knows nothing
// about any particular game: it loads the WAV files you name, gives each one
// a number, and plays them when you say so.
//
// The card has ONE DAC, so the hardware cannot really play two sounds at
// once. What this does is MIX them by software, adding up the samples of
// every sound that is currently going into the single buffer the DMA is
// playing. That is the only way to hear an engine and a shot together.
//
// If there is no card, sound_start() returns 0 and every other call here
// does nothing at all. A program using this runs exactly the same, in
// silence, with no special case anywhere in it.
//
//
// HOW TO USE IT IN A NEW PROGRAM
//
//   1. Once, at the start:
//
//          sound_set_log(my_log_function);   /* optional, see below */
//
//          if (sound_start() == 1){
//              boom   = load_sound("..\\res\\boom.wav");
//              engine = load_sound("..\\res\\engine.wav");
//              set_sound_volume(engine, 16);          /* optional */
//          }
//
//   2. Once per frame, ALWAYS, whether anything is sounding or not:
//
//          sound_update();
//
//   3. Whenever you want to hear something:
//
//          play_sound(boom);            /* once */
//          loop_sound(engine);          /* over and over */
//          stop_looping_sound(engine);  /* enough of that */
//
//      And for background music, of any length at all:
//
//          play_song("..\\res\\music.wav");
//
//   4. Before leaving the program, without fail:
//
//          sound_end();
//
//
// WHAT A "VOICE" IS
//
// A voice is one slot in the mixer: it remembers which sound it is playing
// and how far into it it has got. SOUND_MAX_VOICES of them means that many
// sounds can be heard at the same time.
//
// You do NOT have to think about voices. play_sound() and loop_sound() pick
// a free one for you. They give you back the number they used only in case
// you ever want to cut that particular sound short with stop_sound(), and
// ignoring the returned value is perfectly normal.
//===========================================================


// How many different WAVs can be loaded at once. Each one costs whatever it
// weighs in memory, so this is only the size of the table.
#define SOUND_MAX_SAMPLES 		8

// How many sounds can be heard at the same time. Voices that are not
// sounding cost nothing, so this is cheap to raise.
#define SOUND_MAX_VOICES 		8

// The volume scale.
//
// SOUND_VOLUME_MAX is the sound exactly as it was recorded. Below it is
// quieter; ABOVE it is amplified, which is allowed and often what you want
// for one sound that has to be heard over everything else. Anything that
// then goes past what a byte holds is clamped by the mixer, so it distorts
// instead of wrapping round, which would turn a loud shot into a nasty crack.
#define SOUND_VOLUME_MAX 		32

// What a song starts at, unless set_song_volume() says otherwise.
//
// Half, and deliberately so: unlike an effect, a song is sounding ALL the
// time, so it is added to everything else on every single sample. At full
// volume it would leave no room for the effects on top and the mixer would
// spend the whole game clamping.
#define SOUND_SONG_DEFAULT_VOLUME 	16


// ---------- Starting and stopping ----------

// Finds the card, takes over its interrupt and starts the DMA running on
// silence. Returns 1 if there is sound, 0 if there is not.
//
// It loads NO files: which sounds exist is the program's business, not the
// library's. Call load_sound() for each one after this has returned 1.
int sound_start(void);

// Stops the card and gives the memory back. MUST be called before leaving
// the program, or the DMA carries on reading memory that is no longer ours.
void sound_end(void);

// Refills whichever half of the buffer the card has just finished playing,
// mixing every sound that is going into it.
//
// Has to be called once per frame from the main loop, and also inside any
// loop that waits for something: if it stops being called the sound
// stutters, because the card just replays the half it already has. It is
// cheap, and on most frames it finds there is nothing to do and returns
// straight away.
void sound_update(void);


// ---------- Loading ----------

// Loads one WAV and returns the number you play it by, or -1 if it could not
// be loaded (file missing, wrong format, or no slots left).
//
// The file must be 8 bit mono PCM. If it was recorded at a different rate it
// is converted here, once, because the card has a single output rate.
//
// A -1 is safe to keep and pass around: playing it simply does nothing.
int load_sound(char *file_name);

// How loud this sound is from now on, out of SOUND_VOLUME_MAX. It belongs to
// the sound rather than to each call, because in practice a given sound
// always wants the same volume: an engine is always in the background and a
// shot is always in front. Every sound starts at SOUND_VOLUME_MAX.
void set_sound_volume(int sound_id, int volume);


// ---------- Playing ----------

// Plays a sound once, from the beginning. Returns the voice it went to, or
// -1 if it could not be played.
//
// If every voice is busy, the one closest to finishing is taken over, since
// it was about to go quiet anyway. Looping sounds are never taken over.
int play_sound(int sound_id);

// Plays a sound over and over until something stops it. Returns the voice.
//
// Calling it again for a sound that is ALREADY looping does nothing and
// gives back the same voice. That is what lets you call it on every single
// frame while a key is held down, without the sound restarting seventy times
// a second, which would just be a click.
int loop_sound(int sound_id);


// ---------- Background music ----------

// Starts a song playing behind everything else, on a loop, until stop_song().
// Returns 1 if it is playing, 0 if it could not be started.
//
// A song is NOT loaded into memory: at 44100 bytes a second one minute would
// be 2.6 MB, which does not fit in a DOS machine at all. It is read from the
// file as it plays, a fraction of a second at a time, so the length of the
// song makes no difference to the memory it costs. It is always
// SONG_BUFFER_SIZE, whether the song lasts one minute or one hour.
//
// That is also why the file has to be EXACTLY 8 bit mono PCM at
// SOUND_SAMPLE_RATE (44100 Hz). load_sound() can convert a file because it
// does it once at startup; there is nowhere to do that while streaming.
// Convert it beforehand:
//
//     sox song.mp3 -b 8 -c 1 -e unsigned-integer -r 44100 music.wav
//
// The file is left open for as long as the music lasts, and going round at
// the end is seamless: there is no gap and no click.
//
// Only one song at a time. Calling it again replaces whatever was playing.
int play_song(char *file_name);

// Stops the music and closes the file.
void stop_song(void);

// How loud the music is, out of SOUND_VOLUME_MAX, from now on. It starts at
// SOUND_SONG_DEFAULT_VOLUME.
void set_song_volume(int volume);


// ---------- Silence ----------

// Silences one voice, the number play_sound() or loop_sound() gave back.
void stop_sound(int voice);

// Silences whatever voice is looping this sound, if any.
//
// This is the partner of loop_sound(), and between the two of them a program
// never has to remember a voice number at all: "engine on" is
// loop_sound(engine) and "engine off" is stop_looping_sound(engine), and
// both can be called on every frame without doing any harm.
void stop_looping_sound(int sound_id);

// Silences everything.
void stop_all_sounds(void);


// ---------- Reporting problems ----------

// Where this library should report problems. Optional: with no log function
// it stays completely quiet, which is the default.
//
// It exists because a game in graphics mode cannot have anything printed to
// the screen, so the library must not choose for itself where the text goes.
// It is also what keeps this file free of any game code: hand it your own
// log function once and sound.c never needs to know anything about your
// program, which is what makes it copyable into the next one as it is.
void sound_set_log(void (*log_function)(char *message));


#endif
