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
// No printf anywhere: the game is in graphics mode and any printing would
// paint garbage over the screen. Problems are reported through tanks_log().
//===========================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <alloc.h>
#include <conio.h>     /* inportb() / outportb() */
#include "header\sound.h"
#include "header\util.h"

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

// In unsigned 8 bit audio the middle of the wave, ie. silence, is 128 and
// not 0. Samples are turned into signed values by subtracting it before
// mixing, and it is added back afterwards.
#define SOUND_SILENCE 		128

// Volumes are out of this, so 64 is full volume
#define SOUND_VOLUME_MAX 	64


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

static struct sound_sample sound_samples[SOUND_TOTAL_SAMPLES];
static struct sound_voice  sound_voices[SOUND_TOTAL_VOICES];

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
static int sound_load_sample(char *file_name, int sample_id)
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

	sound_samples[sample_id].data = NULL;
	sound_samples[sample_id].block = NULL;
	sound_samples[sample_id].length = 0;

	file = fopen(file_name, "rb");
	if (file == NULL){
		return 0;
	}

	if (fread(&header, sizeof(WavHeader), 1, file) != 1){
		fclose(file);
		return 0;
	}

	/* only plain 8 bit mono PCM, which is what all our files are */
	if (strncmp(header.riff_id, "RIFF", 4) != 0 ||
	    strncmp(header.wave_id, "WAVE", 4) != 0 ||
	    strncmp(header.fmt_id,  "fmt ", 4) != 0 ||
	    strncmp(header.data_id, "data", 4) != 0 ||
	    header.audio_format != 1 ||
	    header.bits_per_sample != 8 ||
	    header.num_channels != 1){
		fclose(file);
		return 0;
	}

	source_length = header.data_size;

	source = (unsigned char far *)farmalloc(source_length);
	if (source == NULL){
		fclose(file);
		return 0;
	}
	source_block = source;

	if (fread(source, 1, (unsigned int)source_length, file) != source_length){
		farfree(source_block);
		fclose(file);
		return 0;
	}

	fclose(file);

	/* already at the right rate: keep it as it is */
	if (header.sample_rate == SOUND_SAMPLE_RATE){

		sound_samples[sample_id].data = source;
		sound_samples[sample_id].block = source_block;
		sound_samples[sample_id].length = source_length;

		return 1;

	}

	/* recorded at another rate: rebuild it at ours */
	destination_length = (source_length * SOUND_SAMPLE_RATE) / header.sample_rate;

	destination = (unsigned char far *)farmalloc(destination_length);
	if (destination == NULL){
		farfree(source_block);
		return 0;
	}
	destination_block = destination;

	for (i = 0; i < destination_length; i++){
		destination[i] = source[(i * header.sample_rate) / SOUND_SAMPLE_RATE];
	}

	farfree(source_block);

	sound_samples[sample_id].data = destination;
	sound_samples[sample_id].block = destination_block;
	sound_samples[sample_id].length = destination_length;

	return 1;
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

	for (voice_index = 0; voice_index < SOUND_TOTAL_VOICES; voice_index++){

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


/* ---------- What the game calls ---------- */

int sound_init()
{
	SBConfig card;
	int i;

	sound_is_ready = 0;

	for (i = 0; i < SOUND_TOTAL_SAMPLES; i++){
		sound_samples[i].data = NULL;
		sound_samples[i].block = NULL;
		sound_samples[i].length = 0;
	}

	sound_stop_all();

	sound_detect_card(&card);
	sound_base_port = card.base_port;
	sound_irq = card.irq;
	sound_dma = card.dma8;

	/* if the DSP does not answer there is no card: leave quietly */
	if (sound_dsp_reset(sound_base_port) == 0){
		tanks_log("Sound: no Sound Blaster found, playing without sound");
		return 0;
	}

	if (sound_load_sample("..\\res\\fire.wav", SOUND_SAMPLE_FIRE) == 0){
		tanks_log("Sound: could not load fire.wav");
	}

	if (sound_load_sample("..\\res\\engip1.wav", SOUND_SAMPLE_ENGINE_1) == 0){
		tanks_log("Sound: could not load engip1.wav");
	}

	if (sound_load_sample("..\\res\\engip2.wav", SOUND_SAMPLE_ENGINE_2) == 0){
		tanks_log("Sound: could not load engip2.wav");
	}

	if (sound_load_sample("..\\res\\died.wav", SOUND_SAMPLE_DIED) == 0){
		tanks_log("Sound: could not load died.wav");
	}

	sound_buffer = sound_alloc_dma_buffer(SOUND_BUFFER_SIZE, 65536UL, &sound_buffer_block);
	if (sound_buffer == NULL){
		tanks_log("Sound: not enough memory for the DMA buffer");
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

	tanks_log("Sound: ready");

	return 1;
}


void sound_shutdown()
{
	int i;

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

	for (i = 0; i < SOUND_TOTAL_SAMPLES; i++){
		if (sound_samples[i].block != NULL){
			farfree(sound_samples[i].block);
			sound_samples[i].block = NULL;
			sound_samples[i].data = NULL;
			sound_samples[i].length = 0;
		}
	}

	sound_is_ready = 0;
}


void sound_update()
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

	if (sound_half_to_fill == 0){
		sound_half_to_fill = 1;
	}else{
		sound_half_to_fill = 0;
	}
}


void sound_play(int voice, int sample_id, int volume)
{
	if (sound_is_ready == 0){
		return;
	}

	if (voice < 0 || voice >= SOUND_TOTAL_VOICES){
		return;
	}

	sound_voices[voice].sample_id = sample_id;
	sound_voices[voice].position = 0;
	sound_voices[voice].volume = volume;
	sound_voices[voice].is_looping = 0;
	sound_voices[voice].is_playing = 1;
}


void sound_loop(int voice, int sample_id, int volume)
{
	if (sound_is_ready == 0){
		return;
	}

	if (voice < 0 || voice >= SOUND_TOTAL_VOICES){
		return;
	}

	// Already looping this very sample: leave it alone. This is what lets
	// the caller ask for the engine on every single frame while the key is
	// held, without the sound starting from the beginning 70 times a second,
	// which would just be a click.
	if (sound_voices[voice].is_playing == 1 &&
	    sound_voices[voice].is_looping == 1 &&
	    sound_voices[voice].sample_id == sample_id){
		return;
	}

	sound_voices[voice].sample_id = sample_id;
	sound_voices[voice].position = 0;
	sound_voices[voice].volume = volume;
	sound_voices[voice].is_looping = 1;
	sound_voices[voice].is_playing = 1;
}


void sound_stop(int voice)
{
	if (voice < 0 || voice >= SOUND_TOTAL_VOICES){
		return;
	}

	sound_voices[voice].is_playing = 0;
	sound_voices[voice].is_looping = 0;
	sound_voices[voice].position = 0;
}


void sound_stop_all()
{
	int i;

	for (i = 0; i < SOUND_TOTAL_VOICES; i++){
		sound_stop(i);
	}
}
