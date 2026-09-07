//===========================================================
// Sound Blaster playback with software mixing.
//
// Adapted from src\sb\sbwav8.c, which streams ONE WAV straight from disk to
// the card. Two things had to change to make it usable in a game:
//
//   1. Several sounds at once. The card has one DAC, so they are added up
//      by software into the single buffer the DMA plays (sound_mix_half()).
//
//   2. Nothing may block. sbwav8.c sits in a while() waiting for the card
//      and reads from disk in the middle of playing. Here the WAVs are all
//      loaded into memory at startup, and sound_update() only does work
//      when the card has actually asked for more, so it returns straight
//      away on almost every frame.
//
// This file is a LIBRARY: it names no file, knows no game, and can be copied
// into another program together with header\sound.h and nothing else. What
// gets loaded and when it is played is decided entirely by the caller.
//
// No printf anywhere either: a caller in graphics mode would get garbage
// painted over its screen. Problems go to whatever sound_set_log() was given,
// and nowhere at all if it was never called.
//===========================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <alloc.h>
#include <conio.h>     /* inportb() / outportb() */
#include "header\sound.h"

// Output rate of the DSP, and therefore of everything: the card has ONE
// rate, so a WAV recorded at another one is resampled when it is loaded.
#define SOUND_SAMPLE_RATE 	16000

// Size of each half of the double buffer, in bytes = samples.
//
// This is a compromise. At 16000 Hz, 512 bytes is 32 ms of sound: that is
// how long the card takes to ask for more, and also the worst case delay
// before a new shot is heard. Smaller means a snappier shot but leaves less
// margin for a slow frame; bigger is safer but the shot lags behind the
// picture. The main loop runs at about 70 Hz (14 ms), so there is room for
// two frames inside every half.
#define SOUND_HALF_SIZE 	512
#define SOUND_BUFFER_SIZE 	(SOUND_HALF_SIZE * 2)

// How much of the song is kept in memory, waiting to be played.
//
// A song is NOT loaded: at 16000 bytes a second, one minute would be 937 KB
// and would not fit in a DOS machine at all. It is read from the file as it
// plays, and this is how far ahead we stay.
//
// 8192 bytes is half a second of music. That is the margin before the music
// is heard to break up if the game stalls, and it also means the disk is only
// touched about four times a second instead of on every frame.
#define SONG_BUFFER_SIZE 	8192

// When less than this is left, go and read more. Half the buffer, so every
// read is worth doing and there is always a quarter of a second in hand.
#define SONG_REFILL_LEVEL 	4096

// In unsigned 8 bit audio the middle of the wave, ie. silence, is 128 and
// not 0. Samples are turned into signed values by subtracting it before
// mixing, and it is added back afterwards.
#define SOUND_SILENCE 		128



/* ---------- WAV file format ---------- */

#pragma pack(push, 1)
typedef struct {
    char           riff_id[4];
    unsigned long  riff_size;
    char           wave_id[4];

    char           fmt_id[4];
    unsigned long  fmt_size;
    unsigned short audio_format;
    unsigned short num_channels;
    unsigned long  sample_rate;
    unsigned long  byte_rate;
    unsigned short block_align;
    unsigned short bits_per_sample;

    char           data_id[4];
    unsigned long  data_size;
} WavHeader;
#pragma pack(pop)


/* ---------- Card configuration ---------- */

typedef struct {
    unsigned int base_port;
    int irq;
    int dma8;
    int dma16;
} SBConfig;


/* ---------- A loaded sound, and a voice playing one ---------- */

struct sound_sample {

	unsigned char far *data;		// the samples, already at SOUND_SAMPLE_RATE
	unsigned char far *block;		// what farmalloc() returned, the one to free
	unsigned long length;			// how many samples
	int volume;						// out of SOUND_VOLUME_MAX, see set_sound_volume()

};

struct sound_voice {

	int is_playing;					// 1 while it is sounding
	int is_looping;					// 1 = starts again on reaching the end
	int sample_id;					// which sound it is playing
	int volume;						// out of SOUND_VOLUME_MAX
	unsigned long position;			// how far into the sample it has got

};


/* ---------- State ---------- */

// 0 = there is no card, or something failed. Every entry point checks this
// first and returns, so the game runs the same without sound.
static int sound_is_ready = 0;

static struct sound_sample sound_samples[SOUND_MAX_SAMPLES];
static struct sound_voice  sound_voices[SOUND_MAX_VOICES];

// The buffer the DMA plays, going round and round over its two halves
static unsigned char far *sound_buffer = NULL;
static unsigned char far *sound_buffer_block = NULL;	// the farmalloc pointer, to free
static int sound_half_to_fill = 0;						// 0 or 1

// Where the mixing is done, before being clamped into the real buffer. It
// has to be a signed int per sample: several voices added together go well
// past what a single byte can hold, and that is exactly what has to be seen
// in order to clamp it.
static int sound_mix_buffer[SOUND_HALF_SIZE];

// Card settings, and the interrupt vector we take over
static unsigned int sound_base_port = 0x220;
static int sound_irq = 5;
static int sound_dma = 1;
void interrupt (*sound_old_isr)(void) = NULL;
static int sound_isr_installed = 0;

// Raised by the interrupt handler when the card has finished a half and
// wants the next one. "volatile" because it changes behind the compiler's
// back, inside an interrupt: without it the check in sound_update() could
// be optimised away, since nothing in plain sight ever sets it.
static volatile int sound_buffer_ready = 0;

/* ---------- The song ---------- */

// The song is a channel of its own, not one of the voices. A voice plays
// something that is already in memory; this one is fed from a file that stays
// open for as long as the music lasts.
static FILE *song_file = NULL;

static int song_is_playing = 0;
static int song_volume = SOUND_SONG_DEFAULT_VOLUME;

// The music waiting to be played. song_position is where the mixer reads,
// song_fill is how much of the buffer holds real music.
static unsigned char song_buffer[SONG_BUFFER_SIZE];
static unsigned int  song_position = 0;
static unsigned int  song_fill = 0;

// Where the audio starts in the file, how long it is, and how much of it is
// still to be read on this lap. When the last one is 0 we go back to the
// start and the song begins again.
static unsigned long song_data_start = 0;
static unsigned long song_data_size = 0;
static unsigned long song_bytes_left = 0;

static char sound_log_text[100];


// Where this library reports problems. NULL, the default, means nowhere.
//
// This is what keeps this file free of any game code. Without it, sound.c
// would have to #include the log of one particular program and could not be
// copied into the next one as it is. The program hands over its own function
// once, and sound.c has no idea what that function does with the text.
static void (*sound_log_function)(char *message) = NULL;


static void sound_log(char *message)
{
	if (sound_log_function == NULL){
		return;
	}

	sound_log_function(message);
}


void sound_set_log(void (*log_function)(char *message))
{
	sound_log_function = log_function;
}


/* ---------- DSP ports ---------- */

#define DSP_RESET(b)       (b + 0x6)
#define DSP_READ(b)        (b + 0xA)
#define DSP_WRITE(b)       (b + 0xC)
#define DSP_READ_STATUS(b) (b + 0xE)
#define DSP_ACK8(b)        (b + 0xE)   /* for 8 bits the IRQ ack is the SAME port as the status */

#define MIXER_ADDR(b) (b + 0x4)
#define MIXER_DATA(b) (b + 0x5)


static void sound_dsp_write(unsigned int base, unsigned char value)
{
	while (inportb(DSP_WRITE(base)) & 0x80) ;	/* wait for the port to be free */
	outportb(DSP_WRITE(base), value);
}


// Resets the DSP and waits for it to answer 0xAA, which is how the card
// says it is there. This is also how we find out whether there is a card at
// all: if it never answers, there is nothing at that port.
static int sound_dsp_reset(unsigned int base)
{
	int i;

	outportb(DSP_RESET(base), 1);
	delay(1);
	outportb(DSP_RESET(base), 0);

	for (i = 0; i < 200; i++) {
		if (inportb(DSP_READ_STATUS(base)) & 0x80) {
			if (inportb(DSP_READ(base)) == 0xAA){
				return 1;
			}
		}
	}

	return 0;
}


static void sound_mixer_write(unsigned int base, unsigned char index, unsigned char value)
{
	outportb(MIXER_ADDR(base), index);
	outportb(MIXER_DATA(base), value);
}


// Turns every volume up. Both the old register layout and the SB16 one are
// written, because we do not know which card is really there.
static void sound_mixer_unmute(unsigned int base)
{
	sound_mixer_write(base, 0x22, 0xFF);	/* master (old layout) */
	sound_mixer_write(base, 0x04, 0xFF);	/* voice / DAC (old layout) */
	sound_mixer_write(base, 0x30, 0xFF);	/* master left (SB16) */
	sound_mixer_write(base, 0x31, 0xFF);	/* master right (SB16) */
	sound_mixer_write(base, 0x32, 0xFF);	/* DAC left (SB16) */
	sound_mixer_write(base, 0x33, 0xFF);	/* DAC right (SB16) */
}


static void sound_dsp_set_sample_rate(unsigned int base, unsigned int rate)
{
	sound_dsp_write(base, 0x41);			/* set output rate (SB16) */
	sound_dsp_write(base, (rate >> 8) & 0xFF);
	sound_dsp_write(base, rate & 0xFF);
}


// Starts 8 bit auto-init output. "Auto-init" means the card loops round the
// buffer on its own for ever, raising an interrupt every block_size bytes
// to say "I have finished this half, refill it". That is what lets sound
// carry on while the game is busy doing other things.
static void sound_dsp_start_playback(unsigned int base, unsigned int block_size)
{
	unsigned char mode = 0x00;				/* UNSIGNED: 8 bit WAVs are 0-255 */
	unsigned int count = block_size - 1;

	sound_dsp_write(base, 0xC6);			/* 8 bit, output, auto-init */
	sound_dsp_write(base, mode);
	sound_dsp_write(base, count & 0xFF);
	sound_dsp_write(base, (count >> 8) & 0xFF);
}


/* ---------- DMA controller (8237), 8 bit channels 0-3 ---------- */

static void sound_dma_setup(int channel, unsigned long phys_addr, unsigned long length_bytes)
{
	unsigned int page, addr, count;
	unsigned int addr_port, count_port, page_port;

	addr_port  = channel * 2;
	count_port = channel * 2 + 1;

	switch (channel) {
		case 0: page_port = 0x87; break;
		case 1: page_port = 0x83; break;
		case 2: page_port = 0x81; break;
		case 3: page_port = 0x82; break;
		default: return;
	}

	page  = (unsigned int)((phys_addr >> 16) & 0xFF);
	addr  = (unsigned int)(phys_addr & 0xFFFF);
	count = (unsigned int)(length_bytes - 1);

	outportb(0x0A, 0x04 | channel);				/* mask the channel while it is programmed */
	outportb(0x0C, 0x00);						/* clear the high/low byte flip-flop */

	/* mode: single (01) | auto-init (1) | memory to device (10) | channel */
	outportb(0x0B, 0x40 | 0x10 | 0x08 | channel);

	outportb(addr_port, addr & 0xFF);
	outportb(addr_port, (addr >> 8) & 0xFF);
	outportb(page_port, page);

	outportb(count_port, count & 0xFF);
	outportb(count_port, (count >> 8) & 0xFF);

	outportb(0x0A, channel);					/* unmask: ready to transfer */
}


static void sound_dma_stop(int channel)
{
	outportb(0x0A, 0x04 | channel);				/* mask the channel: it stops reading memory */
}


/* ---------- Interrupt handler ---------- */

// Runs every time the card finishes a half. It does as little as possible:
// acknowledge, raise a flag, and get out. The mixing is done later by
// sound_update(), from the main loop, because doing it here would hold up
// every other interrupt in the machine, the keyboard included.
void interrupt sound_isr(void)
{
	inportb(DSP_ACK8(sound_base_port));			/* tell the DSP we have seen the IRQ */

	sound_buffer_ready = 1;

	outportb(0x20, 0x20);						/* EOI to the master PIC */
	if (sound_irq >= 8){
		outportb(0xA0, 0x20);					/* EOI to the slave PIC as well */
	}
}


static void sound_install_isr(int irq)
{
	int vector;

	if (irq < 8){
		vector = 0x08 + irq;
	}else{
		vector = 0x70 + (irq - 8);
	}

	sound_old_isr = getvect(vector);
	setvect(vector, sound_isr);

	/* unmask the line in the PIC, so the interrupt actually gets through */
	if (irq < 8){
		outportb(0x21, inportb(0x21) & ~(1 << irq));
	}else{
		outportb(0xA1, inportb(0xA1) & ~(1 << (irq - 8)));
	}

	sound_isr_installed = 1;
}


static void sound_restore_isr(int irq)
{
	int vector;

	if (sound_isr_installed == 0){
		return;
	}

	if (irq < 8){
		vector = 0x08 + irq;
	}else{
		vector = 0x70 + (irq - 8);
	}

	if (irq < 8){
		outportb(0x21, inportb(0x21) | (1 << irq));
	}else{
		outportb(0xA1, inportb(0xA1) | (1 << (irq - 8)));
	}

	setvect(vector, sound_old_isr);

	sound_isr_installed = 0;
}


/* ---------- Finding the card ---------- */

// Reads the BLASTER environment variable, which is what the sound card
// driver leaves behind saying where it is: eg. "A220 I5 D1 H5 T6". If it is
// not there we try the usual defaults and let sound_dsp_reset() decide
// whether there is really anything at that port.
static void sound_detect_card(SBConfig *cfg)
{
	char *blaster;
	char *p;

	cfg->base_port = 0x220;
	cfg->irq       = 5;
	cfg->dma8      = 1;
	cfg->dma16     = 5;

	blaster = getenv("BLASTER");
	if (blaster == NULL){
		return;
	}

	p = blaster;
	while (*p) {
		switch (*p) {
			case 'A': cfg->base_port = (unsigned int) strtol(p + 1, NULL, 16); break;
			case 'I': cfg->irq   = atoi(p + 1); break;
			case 'D': cfg->dma8  = atoi(p + 1); break;
			case 'H': cfg->dma16 = atoi(p + 1); break;
		}
		while (*p && *p != ' ') p++;
		while (*p == ' ') p++;
	}
}


/* ---------- Memory for the DMA ---------- */

// Reserves a block the DMA can play from, aligned to a 64 KB boundary.
//
// The 8 bit DMA channels cannot cross a 64 KB physical boundary: they only
// count the bottom 16 bits of the address, so on reaching the end of a 64 KB
// page they wrap round to its start instead of carrying on. A buffer lying
// across one of those boundaries would play its second half as noise.
//
// This is sbwav8.c's asignar_buffer_alineado() as it stands: ask for 64 KB
// more than needed and start at the first boundary inside the block, which
// makes crossing one impossible.
//
// The pointer that comes back is NOT the one farmalloc() gave, and farfree()
// only takes that one, so it is handed back separately in original_block.
static unsigned char far *sound_alloc_dma_buffer(unsigned long size, unsigned long align, unsigned char far **original_block)
{
	unsigned char far *block;
	unsigned long physical;
	unsigned long remainder;

	*original_block = NULL;

	block = (unsigned char far *)farmalloc(size + align);
	if (block == NULL){
		return NULL;
	}

	*original_block = block;

	physical = ((unsigned long)FP_SEG(block) << 4) + FP_OFF(block);
	remainder = physical % align;

	if (remainder != 0){
		physical = physical + (align - remainder);
	}

	return (unsigned char far *)MK_FP((unsigned int)(physical >> 4), 0);
}


/* ---------- Loading a WAV ---------- */

// Reads one WAV into memory, resampling it if it was not recorded at
// SOUND_SAMPLE_RATE.
//
// Resampling is needed because the DSP has a single output rate: a 22255 Hz
// file played at 16000 would come out slow and low pitched. It is done the
// cheap way, by picking the nearest sample, which for an engine or an
// explosion is more than good enough, and it happens once at startup so it
// costs nothing while playing.
//===========================================================
// Loads one WAV into memory and hands back the number it is played by.
//
// This is the one function a program HAS to call for every sound it wants,
// and it is what replaced a sound_init() with four file names hard coded
// inside it. The library does not know, and does not want to know, which
// sounds your game has: it keeps up to SOUND_MAX_SAMPLES of them and gives
// each one a number.
//
// Everything is resampled to SOUND_SAMPLE_RATE, because the DSP has a single
// output rate: a 22255 Hz file played at 16000 would come out slow and low
// pitched. It is done the cheap way, by picking the nearest sample, which
// for an engine or an explosion is more than good enough, and it happens
// once at loading time so it costs nothing while playing.
//===========================================================
int load_sound(char *file_name)
{
	FILE *file;
	WavHeader header;
	unsigned char far *source;
	unsigned char far *destination;
	unsigned char far *source_block;
	unsigned char far *destination_block;
	unsigned long source_length;
	unsigned long destination_length;
	unsigned long i;
	int slot;
	int sound_id;

	// The first slot with nothing in it. Slots are not reused while the
	// program runs, so a number handed out here stays good until sound_end().
	sound_id = -1;

	for (slot = 0; slot < SOUND_MAX_SAMPLES; slot++){

		if (sound_samples[slot].data == NULL){
			sound_id = slot;
			break;
		}

	}

	if (sound_id == -1){
		sound_log("Sound: no free sample slot, raise SOUND_MAX_SAMPLES");
		return -1;
	}

	file = fopen(file_name, "rb");
	if (file == NULL){
		return -1;
	}

	if (fread(&header, sizeof(WavHeader), 1, file) != 1){
		fclose(file);
		return -1;
	}

	/* only plain 8 bit mono PCM */
	if (strncmp(header.riff_id, "RIFF", 4) != 0 ||
	    strncmp(header.wave_id, "WAVE", 4) != 0 ||
	    strncmp(header.fmt_id,  "fmt ", 4) != 0 ||
	    strncmp(header.data_id, "data", 4) != 0 ||
	    header.audio_format != 1 ||
	    header.bits_per_sample != 8 ||
	    header.num_channels != 1){
		fclose(file);
		return -1;
	}

	source_length = header.data_size;

	source = (unsigned char far *)farmalloc(source_length);
	if (source == NULL){
		fclose(file);
		return -1;
	}
	source_block = source;

	if (fread(source, 1, (unsigned int)source_length, file) != source_length){
		farfree(source_block);
		fclose(file);
		return -1;
	}

	fclose(file);

	/* already at the right rate: keep it as it is */
	if (header.sample_rate == SOUND_SAMPLE_RATE){

		sound_samples[sound_id].data = source;
		sound_samples[sound_id].block = source_block;
		sound_samples[sound_id].length = source_length;
		sound_samples[sound_id].volume = SOUND_VOLUME_MAX;

		return sound_id;

	}

	/* recorded at another rate: rebuild it at ours */
	destination_length = (source_length * SOUND_SAMPLE_RATE) / header.sample_rate;

	destination = (unsigned char far *)farmalloc(destination_length);
	if (destination == NULL){
		farfree(source_block);
		return -1;
	}
	destination_block = destination;

	for (i = 0; i < destination_length; i++){
		destination[i] = source[(i * header.sample_rate) / SOUND_SAMPLE_RATE];
	}

	farfree(source_block);

	sound_samples[sound_id].data = destination;
	sound_samples[sound_id].block = destination_block;
	sound_samples[sound_id].length = destination_length;
	sound_samples[sound_id].volume = SOUND_VOLUME_MAX;

	return sound_id;
}


/* ---------- The mixer ---------- */

// Builds SOUND_HALF_SIZE samples out of every voice that is sounding, and
// writes them into one half of the buffer.
//
// Samples arrive as unsigned bytes where 128 is silence, so they are turned
// into signed values around 0 before being added: adding them raw would just
// pile up the 128s and saturate immediately.
//
// The sum is kept in a signed int, because several voices together go well
// past what a byte holds. Only at the end is it clamped back into a byte.
// Clamping is what a real mixer does when the sum is too loud: the sound
// distorts, which is much better than wrapping round, which would turn the
// loudest part of a shot into a horrible crack.
static void sound_mix_half(unsigned char far *destination)
{
	unsigned int i;
	int voice_index;
	int value;
	struct sound_voice *voice;
	struct sound_sample *sample;

	for (i = 0; i < SOUND_HALF_SIZE; i++){
		sound_mix_buffer[i] = 0;
	}

	for (voice_index = 0; voice_index < SOUND_MAX_VOICES; voice_index++){

		voice = &sound_voices[voice_index];

		if (voice->is_playing == 0){
			continue;
		}

		sample = &sound_samples[voice->sample_id];

		if (sample->data == NULL){
			voice->is_playing = 0;
			continue;
		}

		for (i = 0; i < SOUND_HALF_SIZE; i++){

			/* reached the end: start again, or go quiet */
			if (voice->position >= sample->length){

				if (voice->is_looping == 1){
					voice->position = 0;
				}else{
					voice->is_playing = 0;
					break;
				}

			}

			value = (int)sample->data[voice->position] - SOUND_SILENCE;
			value = (value * voice->volume) / SOUND_VOLUME_MAX;

			sound_mix_buffer[i] = sound_mix_buffer[i] + value;

			voice->position = voice->position + 1;

		}

	}

	// The song, mixed in exactly like a voice. The only difference is where
	// the bytes come from: song_refill() puts them there from the file, a
	// little at a time, instead of them all being in memory at once.
	if (song_is_playing == 1){

		for (i = 0; i < SOUND_HALF_SIZE; i++){

			// The buffer ran dry, which means the disk did not keep up. The
			// rest of this half is left silent and the music carries on from
			// the same place once more has been read: a gap, not a jump.
			if (song_position >= song_fill){
				break;
			}

			value = (int)song_buffer[song_position] - SOUND_SILENCE;
			value = (value * song_volume) / SOUND_VOLUME_MAX;

			sound_mix_buffer[i] = sound_mix_buffer[i] + value;

			song_position = song_position + 1;

		}

	}

	for (i = 0; i < SOUND_HALF_SIZE; i++){

		value = sound_mix_buffer[i];

		if (value > 127){
			value = 127;
		}

		if (value < -128){
			value = -128;
		}

		destination[i] = (unsigned char)(value + SOUND_SILENCE);

	}
}


/* ---------- The song: reading it as it plays ---------- */

//===========================================================
// Tops the song buffer back up from the file.
//
// Called from sound_update() every time a half has been mixed, which is about
// 31 times a second, but it only touches the disk when less than
// SONG_REFILL_LEVEL bytes are left. That works out at roughly four reads a
// second, of about 4 KB each: 16 KB a second, which any disk can manage
// without the game noticing.
//
// Going round at the end of the song happens right here, in the middle of a
// read, so the last byte of the song and the first byte of the next lap end
// up next to each other in the buffer. The loop is seamless.
//===========================================================
static void song_refill(void)
{
	unsigned int remaining;
	unsigned int space;
	unsigned int wanted;
	unsigned int got;
	int attempts;

	if (song_is_playing == 0){
		return;
	}

	if (song_file == NULL){
		return;
	}

	remaining = song_fill - song_position;

	// Still plenty in hand: leave the disk alone. This is the usual case.
	if (remaining > SONG_REFILL_LEVEL){
		return;
	}

	// Slide what is left to the front, so the rest of the buffer is free
	// space again. It is at most SONG_REFILL_LEVEL bytes, four times a
	// second, which costs nothing.
	if (remaining > 0){
		memmove(song_buffer, song_buffer + song_position, remaining);
	}

	song_fill     = remaining;
	song_position = 0;

	// Bounded on purpose: filling the buffer takes one read, or two when the
	// song ends in the middle of it. If a broken file made neither work, this
	// must not turn into a loop with no way out.
	attempts = 0;

	while (song_fill < SONG_BUFFER_SIZE){

		attempts = attempts + 1;

		if (attempts > 4){
			break;
		}

		space = SONG_BUFFER_SIZE - song_fill;

		// Never read past the end of the audio: what follows it in the file
		// is not music, and the next lap has to start at the beginning
		if ((unsigned long)space > song_bytes_left){
			wanted = (unsigned int)song_bytes_left;
		}else{
			wanted = space;
		}

		if (wanted == 0){

			// Nothing left on this lap and nothing to loop round to either:
			// the file has no audio in it at all
			if (song_data_size == 0){
				break;
			}

		}else{

			got = (unsigned int)fread(song_buffer + song_fill, 1, wanted, song_file);

			song_fill       = song_fill + got;
			song_bytes_left = song_bytes_left - (unsigned long)got;

			// Shorter than its header claimed, or the read failed. Treat it
			// as the end and go round.
			if (got < wanted){
				song_bytes_left = 0;
			}

		}

		if (song_bytes_left == 0){
			fseek(song_file, song_data_start, SEEK_SET);
			song_bytes_left = song_data_size;
		}

	}
}


void stop_song(void)
{
	song_is_playing = 0;

	if (song_file != NULL){
		fclose(song_file);
		song_file = NULL;
	}

	song_position = 0;
	song_fill     = 0;

	song_data_start = 0;
	song_data_size  = 0;
	song_bytes_left = 0;
}


void set_song_volume(int volume)
{
	song_volume = volume;
}


//===========================================================
// Starts a song playing in the background, on a loop, until stop_song().
//
// Only one song at a time: this replaces whatever was playing.
//
// Unlike load_sound(), which reads the whole file into memory and resamples
// it once, this one reads the file as it goes and therefore cannot convert
// anything on the way past. The WAV has to arrive at exactly the rate the
// card runs at, which is what the last check below is about.
//===========================================================
int play_song(char *file_name)
{
	WavHeader header;

	stop_song();

	if (sound_is_ready == 0){
		return 0;
	}

	song_file = fopen(file_name, "rb");

	if (song_file == NULL){
		sound_log("Song: could not open the file");
		return 0;
	}

	if (fread(&header, sizeof(WavHeader), 1, song_file) != 1){
		sound_log("Song: the file is too short to be a WAV");
		fclose(song_file);
		song_file = NULL;
		return 0;
	}

	// The plain 44 byte header, which is what sox writes. A file carrying
	// extra chunks before the audio would fail here.
	if (strncmp(header.riff_id, "RIFF", 4) != 0 ||
	    strncmp(header.wave_id, "WAVE", 4) != 0 ||
	    strncmp(header.fmt_id,  "fmt ", 4) != 0 ||
	    strncmp(header.data_id, "data", 4) != 0){
		sound_log("Song: not a plain 44 byte header WAV, convert it with sox");
		fclose(song_file);
		song_file = NULL;
		return 0;
	}

	if (header.audio_format != 1 ||
	    header.bits_per_sample != 8 ||
	    header.num_channels != 1){
		sound_log("Song: the WAV has to be 8 bit mono PCM");
		fclose(song_file);
		song_file = NULL;
		return 0;
	}

	if (header.sample_rate != SOUND_SAMPLE_RATE){
		sprintf(sound_log_text, "Song: the WAV is at %lu Hz and it has to be %d Hz",
		        header.sample_rate, SOUND_SAMPLE_RATE);
		sound_log(sound_log_text);
		fclose(song_file);
		song_file = NULL;
		return 0;
	}

	song_data_start = sizeof(WavHeader);
	song_data_size  = header.data_size;
	song_bytes_left = header.data_size;

	song_position = 0;
	song_fill     = 0;

	song_is_playing = 1;

	// Fill it before the first note is due, so the music starts now and not
	// half a second from now
	song_refill();

	sound_log("Song: playing");

	return 1;
}


/* ---------- What the game calls ---------- */

//===========================================================
// Starts the card up. Returns 1 if there is sound, 0 if there is not.
//
// It loads NO files. That is the whole difference between a library and the
// sound_init() this grew out of, which had the four WAV files of one
// particular game written into it and was therefore of no use to any other
// program. Load yours with load_sound() once this has returned 1.
//===========================================================
int sound_start(void)
{
	SBConfig card;
	int i;

	sound_is_ready = 0;

	for (i = 0; i < SOUND_MAX_SAMPLES; i++){
		sound_samples[i].data = NULL;
		sound_samples[i].block = NULL;
		sound_samples[i].length = 0;
		sound_samples[i].volume = SOUND_VOLUME_MAX;
	}

	stop_all_sounds();
	stop_song();

	sound_detect_card(&card);
	sound_base_port = card.base_port;
	sound_irq = card.irq;
	sound_dma = card.dma8;

	/* if the DSP does not answer there is no card: leave quietly */
	if (sound_dsp_reset(sound_base_port) == 0){
		sound_log("Sound: no Sound Blaster found, playing without sound");
		return 0;
	}

	sound_buffer = sound_alloc_dma_buffer(SOUND_BUFFER_SIZE, 65536UL, &sound_buffer_block);
	if (sound_buffer == NULL){
		sound_log("Sound: not enough memory for the DMA buffer");
		return 0;
	}

	/* start on silence: the card plays from the very first moment, and what
	   it plays is whatever is in the buffer, so it had better be silence and
	   not leftover memory */
	for (i = 0; i < SOUND_BUFFER_SIZE; i++){
		sound_buffer[i] = SOUND_SILENCE;
	}

	sound_half_to_fill = 0;
	sound_buffer_ready = 0;

	sound_mixer_unmute(sound_base_port);
	sound_install_isr(sound_irq);
	sound_dsp_set_sample_rate(sound_base_port, SOUND_SAMPLE_RATE);

	sound_dma_setup(sound_dma,
	                ((unsigned long)FP_SEG(sound_buffer) << 4) + FP_OFF(sound_buffer),
	                SOUND_BUFFER_SIZE);

	sound_dsp_start_playback(sound_base_port, SOUND_HALF_SIZE);

	sound_is_ready = 1;

	sound_log("Sound: ready");

	return 1;
}


void sound_end(void)
{
	int i;

	// The song file has to be closed, and the music has to stop being mixed,
	// before the card is stopped and the memory handed back
	stop_song();

	// Order matters, and it is the same reason as always: while the card is
	// still running, the DMA is reading this memory behind our back. Stop
	// the card, then take the interrupt back, and only then give the memory
	// away. The other way round, the DMA would be reading memory that no
	// longer belongs to us.
	if (sound_is_ready == 1){
		sound_dsp_write(sound_base_port, 0xDA);		/* leave 8 bit auto-init */
		sound_dma_stop(sound_dma);
	}

	sound_restore_isr(sound_irq);

	if (sound_buffer_block != NULL){
		farfree(sound_buffer_block);
		sound_buffer_block = NULL;
		sound_buffer = NULL;
	}

	for (i = 0; i < SOUND_MAX_SAMPLES; i++){
		if (sound_samples[i].block != NULL){
			farfree(sound_samples[i].block);
			sound_samples[i].block = NULL;
			sound_samples[i].data = NULL;
			sound_samples[i].length = 0;
		}
	}

	sound_is_ready = 0;
}


void sound_update(void)
{
	unsigned char far *destination;

	if (sound_is_ready == 0){
		return;
	}

	// Almost every frame there is nothing to do: the card has not finished
	// its half yet. Only when the interrupt has raised the flag is there
	// anything to mix, which at 512 samples is about 3 times a second per
	// 70 frames.
	if (sound_buffer_ready == 0){
		return;
	}

	sound_buffer_ready = 0;

	destination = sound_buffer + (sound_half_to_fill * SOUND_HALF_SIZE);

	sound_mix_half(destination);

	// Straight after mixing, so the music that was just consumed is put back.
	// Almost every time this finds there is still plenty and returns.
	song_refill();

	if (sound_half_to_fill == 0){
		sound_half_to_fill = 1;
	}else{
		sound_half_to_fill = 0;
	}
}


//===========================================================
// Finds a voice for a new sound.
//
// First choice is one that is not doing anything. If they are all busy, the
// one closest to finishing is taken over: it was about to go quiet anyway,
// so it is the least missed.
//
// Looping sounds are never taken over, because they are things like an
// engine that is meant to keep going. If absolutely everything is looping,
// -1 comes back and the new sound is simply not heard, which is far better
// than cutting the engine off.
//===========================================================
static int sound_find_voice(void)
{
	int voice_index;
	int best_voice;
	unsigned long best_remaining;
	unsigned long remaining;
	struct sound_sample *sample;

	for (voice_index = 0; voice_index < SOUND_MAX_VOICES; voice_index++){

		if (sound_voices[voice_index].is_playing == 0){
			return voice_index;
		}

	}

	best_voice = -1;
	best_remaining = 0;

	for (voice_index = 0; voice_index < SOUND_MAX_VOICES; voice_index++){

		if (sound_voices[voice_index].is_looping == 1){
			continue;
		}

		sample = &sound_samples[sound_voices[voice_index].sample_id];

		remaining = sample->length - sound_voices[voice_index].position;

		if (best_voice == -1){
			best_voice = voice_index;
			best_remaining = remaining;
		}else{
			if (remaining < best_remaining){
				best_voice = voice_index;
				best_remaining = remaining;
			}
		}

	}

	return best_voice;
}


//===========================================================
// Is this a sound we can actually play? Everything public checks this, which
// is why a -1 from load_sound() can be kept and passed around freely: it
// simply never plays.
//===========================================================
static int sound_id_is_valid(int sound_id)
{
	if (sound_is_ready == 0){
		return 0;
	}

	if (sound_id < 0 || sound_id >= SOUND_MAX_SAMPLES){
		return 0;
	}

	if (sound_samples[sound_id].data == NULL){
		return 0;
	}

	return 1;
}


void set_sound_volume(int sound_id, int volume)
{
	if (sound_id < 0 || sound_id >= SOUND_MAX_SAMPLES){
		return;
	}

	sound_samples[sound_id].volume = volume;
}


int play_sound(int sound_id)
{
	int voice;

	if (sound_id_is_valid(sound_id) == 0){
		return -1;
	}

	voice = sound_find_voice();

	if (voice == -1){
		return -1;
	}

	sound_voices[voice].sample_id  = sound_id;
	sound_voices[voice].position   = 0;
	sound_voices[voice].volume     = sound_samples[sound_id].volume;
	sound_voices[voice].is_looping = 0;
	sound_voices[voice].is_playing = 1;

	return voice;
}


int loop_sound(int sound_id)
{
	int voice_index;
	int voice;

	if (sound_id_is_valid(sound_id) == 0){
		return -1;
	}

	// Already looping this very sound: leave it exactly as it is. This is
	// what lets the caller ask for the engine on every single frame while a
	// key is held, without the sound starting from the beginning seventy
	// times a second, which would just be a click.
	for (voice_index = 0; voice_index < SOUND_MAX_VOICES; voice_index++){

		if (sound_voices[voice_index].is_playing == 1 &&
		    sound_voices[voice_index].is_looping == 1 &&
		    sound_voices[voice_index].sample_id == sound_id){
			return voice_index;
		}

	}

	voice = sound_find_voice();

	if (voice == -1){
		return -1;
	}

	sound_voices[voice].sample_id  = sound_id;
	sound_voices[voice].position   = 0;
	sound_voices[voice].volume     = sound_samples[sound_id].volume;
	sound_voices[voice].is_looping = 1;
	sound_voices[voice].is_playing = 1;

	return voice;
}


void stop_sound(int voice)
{
	if (voice < 0 || voice >= SOUND_MAX_VOICES){
		return;
	}

	sound_voices[voice].is_playing = 0;
	sound_voices[voice].is_looping = 0;
	sound_voices[voice].position = 0;
}


void stop_looping_sound(int sound_id)
{
	int voice_index;

	for (voice_index = 0; voice_index < SOUND_MAX_VOICES; voice_index++){

		if (sound_voices[voice_index].is_playing == 1 &&
		    sound_voices[voice_index].is_looping == 1 &&
		    sound_voices[voice_index].sample_id == sound_id){
			stop_sound(voice_index);
		}

	}
}


void stop_all_sounds(void)
{
	int i;

	for (i = 0; i < SOUND_MAX_VOICES; i++){
		stop_sound(i);
	}
}
