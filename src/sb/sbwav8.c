/* ============================================================
   SBWAV8.C - 8 bit WAV player over DMA with a double buffer
   DIAGNOSTIC version: it uses the 8 bit DMA channel (controller 1)
   instead of the 16 bit one, to rule out whether the problem is in
   this particular card's 16 bit channel.
   Turbo C++ 3.0 - LARGE memory model
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <alloc.h>
#include <conio.h>

/* ---------- WAV format ---------- */

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

int leer_header_wav(FILE *f, WavHeader *h)
{
    if (fread(h, sizeof(WavHeader), 1, f) != 1) {
        printf("Error reading the header (file truncated?)\n");
        return 0;
    }
    if (strncmp(h->riff_id, "RIFF", 4) != 0 ||
        strncmp(h->wave_id, "WAVE", 4) != 0 ||
        strncmp(h->fmt_id,  "fmt ", 4) != 0 ||
        strncmp(h->data_id, "data", 4) != 0) {
        printf("Not a valid WAV, or it has extra chunks we do not support\n");
        return 0;
    }
    if (h->audio_format != 1) {
        printf("Only uncompressed PCM is supported\n");
        return 0;
    }
    if (h->bits_per_sample != 8) {
        printf("This diagnostic version only supports 8 bit (the file has %u)\n",
               h->bits_per_sample);
        return 0;
    }
    return 1;
}

/* ---------- Card detection (the BLASTER variable) ---------- */

typedef struct {
    unsigned int base_port;
    int irq;
    int dma8;
    int dma16;
} SBConfig;

int detectar_sb(SBConfig *cfg)
{
    char *blaster = getenv("BLASTER");
    char *p;

    cfg->base_port = 0x220;
    cfg->irq       = 5;
    cfg->dma8      = 1;
    cfg->dma16     = 5;

    if (blaster == NULL) {
        printf("BLASTER variable not found, using the defaults\n");
        return 0;
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
    return 1;
}

/* ---------- DSP ports ---------- */

#define DSP_RESET(b)       (b + 0x6)
#define DSP_READ(b)        (b + 0xA)
#define DSP_WRITE(b)       (b + 0xC)
#define DSP_READ_STATUS(b) (b + 0xE)
#define DSP_ACK8(b)        (b + 0xE)   /* for 8 bit, the IRQ ack port is the SAME as the status port */

void dsp_write(unsigned base, unsigned char val)
{
    while (inp(DSP_WRITE(base)) & 0x80) ;   /* wait until the port is free        */
    outp(DSP_WRITE(base), val);
}

int dsp_reset(unsigned base)
{
    int i;
    outp(DSP_RESET(base), 1);
    delay(1);
    outp(DSP_RESET(base), 0);

    for (i = 0; i < 200; i++) {
        if (inp(DSP_READ_STATUS(base)) & 0x80) {
            if (inp(DSP_READ(base)) == 0xAA) return 1;
        }
    }
    return 0;
}

/* ---------- Mixer (volume registers) ---------- */

#define MIXER_ADDR(b) (b + 0x4)
#define MIXER_DATA(b) (b + 0x5)

void mixer_write(unsigned base, unsigned char index, unsigned char value)
{
    outp(MIXER_ADDR(base), index);
    outp(MIXER_DATA(base), value);
}

void mixer_unmute_max(unsigned base)
{
    mixer_write(base, 0x22, 0xFF);   /* master volume (old format)      */
    mixer_write(base, 0x04, 0xFF);   /* voice/DAC volume (old format)   */
    mixer_write(base, 0x30, 0xFF);   /* master volume left  (SB16)      */
    mixer_write(base, 0x31, 0xFF);   /* master volume right (SB16)      */
    mixer_write(base, 0x32, 0xFF);   /* DAC volume left  (SB16)         */
    mixer_write(base, 0x33, 0xFF);   /* DAC volume right (SB16)         */
}

/* Reads and prints mixer register 0x81: it says which DMA channels the
   card really has enabled (bits 0,1,3 = 8 bit; bits 5,6,7 = 16 bit).
   Useful for diagnosis. */
void mixer_mostrar_canales_dma(unsigned base)
{
    unsigned char val;
    outp(MIXER_ADDR(base), 0x81);
    val = inp(MIXER_DATA(base));
    printf("Mixer reg 0x81 (DMA channels enabled) = 0x%02X\n", val);
    printf("  8 bit  -> ch0:%d ch1:%d ch3:%d\n",
           (val & 0x01) != 0, (val & 0x02) != 0, (val & 0x08) != 0);
    printf("  16 bit -> ch5:%d ch6:%d ch7:%d\n",
           (val & 0x20) != 0, (val & 0x40) != 0, (val & 0x80) != 0);
}

void dsp_set_sample_rate(unsigned base, unsigned rate)
{
    dsp_write(base, 0x41);              /* set the output rate (SB16)      */
    dsp_write(base, (rate >> 8) & 0xFF);
    dsp_write(base, rate & 0xFF);
}

/* Starts playback in 8 bit auto-init mode.
   'num_bytes_bloque' is the length of ONE HALF of the buffer, in BYTES
   (unlike the 16 bit version, here it is not divided by 2). */
void dsp_start_playback_8(unsigned base, unsigned num_bytes_bloque, int stereo)
{
    unsigned char modo = 0x00;          /* UNsigned: 8 bit WAVs run 0-255                    */
    unsigned cuenta = num_bytes_bloque - 1;

    if (stereo) modo |= 0x20;

    dsp_write(base, 0xC6);              /* 8 bit, output, auto-init  */
    dsp_write(base, modo);
    dsp_write(base, cuenta & 0xFF);
    dsp_write(base, (cuenta >> 8) & 0xFF);
}

/* ---------- DMA controller (8237), 8 bit channels (0-3) ---------- */

void dma8_setup(int channel, unsigned long phys_addr, unsigned long length_bytes)
{
    unsigned page, addr, count;
    unsigned addr_port, count_port, page_port;

    /* On the 8 bit controller, each channel's address/count ports are
       consecutive: channel*2 (address) and channel*2+1 (count). */
    addr_port  = channel * 2;
    count_port = channel * 2 + 1;

    switch (channel) {
        case 0: page_port = 0x87; break;
        case 1: page_port = 0x83; break;
        case 2: page_port = 0x81; break;
        case 3: page_port = 0x82; break;
        default: return;   /* invalid channel for this controller */
    }

    /* Unlike the 16 bit channel, here the address and the count are
       given directly in BYTES, with no dividing by 2. */
    page  = (unsigned)((phys_addr >> 16) & 0xFF);
    addr  = (unsigned)(phys_addr & 0xFFFF);
    count = (unsigned)(length_bytes - 1);

    outp(0x0A, 0x04 | channel);         /* mask the channel while programming it   */
    outp(0x0C, 0x00);                   /* clear the high/low byte flip-flop       */

    /* mode: single (01) | auto-init (1) | read memory->device (10) | channel */
    outp(0x0B, 0x40 | 0x10 | 0x08 | channel);

    outp(addr_port, addr & 0xFF);
    outp(addr_port, (addr >> 8) & 0xFF);
    outp(page_port, page);

    outp(count_port, count & 0xFF);
    outp(count_port, (count >> 8) & 0xFF);

    outp(0x0A, channel);                /* unmask: ready to transfer               */
}

/* ---------- Allocating a buffer aligned to a page boundary ---------- */

/* Returns an ALIGNED pointer inside the reserved block, which is the one used
   for playback and for programming the DMA. That pointer usually does NOT match
   the one farmalloc() returned, and farfree() only accepts the original, so the
   original is handed back separately in 'bloque_original' so it can be freed at
   the end.

   On return:
     - the return value  -> use for the audio and for the DMA
     - *bloque_original  -> pass to farfree() (left NULL if the alloc failed) */
char *asignar_buffer_alineado(unsigned long size, unsigned long align, char **bloque_original)
{
    char *bloque;
    unsigned long fisica, resto;

    *bloque_original = NULL;

    bloque = farmalloc(size + align);
    if (!bloque) return NULL;

    *bloque_original = bloque;

    fisica = ((unsigned long)FP_SEG(bloque) << 4) + FP_OFF(bloque);
    resto  = fisica % align;
    if (resto != 0) fisica += (align - resto);

    return (char far *) MK_FP((unsigned)(fisica >> 4), 0);
}

/* ---------- IRQ / ISR ---------- */

unsigned sb_base;
int irq_num;
void interrupt (*old_isr)(void);
volatile int buffer_listo = 0;

void interrupt isr_sb(void)
{
    inp(DSP_ACK8(sb_base));             /* acknowledge the 8 bit IRQ to the DSP   */

    buffer_listo = 1;

    outp(0x20, 0x20);                   /* EOI to the master PIC */
    if (irq_num >= 8) outp(0xA0, 0x20); /* EOI to the slave PIC if needed */
}

void instalar_isr(int irq)
{
    int vector = (irq < 8) ? (0x08 + irq) : (0x70 + (irq - 8));
    old_isr = getvect(vector);
    setvect(vector, isr_sb);

    if (irq < 8)
        outp(0x21, inp(0x21) & ~(1 << irq));
    else
        outp(0xA1, inp(0xA1) & ~(1 << (irq - 8)));
}

void restaurar_isr(int irq)
{
    int vector = (irq < 8) ? (0x08 + irq) : (0x70 + (irq - 8));
    if (irq < 8)
        outp(0x21, inp(0x21) | (1 << irq));
    else
        outp(0xA1, inp(0xA1) | (1 << (irq - 8)));
    setvect(vector, old_isr);
}

/* ---------- Main program ---------- */

#define HALF_SIZE 16384UL               /* 16 KB per half  */
#define BUF_SIZE  (HALF_SIZE * 2)

int main(int argc, char *argv[])
{
    FILE *f;
    WavHeader h;
    SBConfig sb;
    char *buffer;
    char *buffer_bloque_original;   /* the pointer exactly as farmalloc gave it: this is the one to free */
    unsigned long fisica;
    int mitad_a_rellenar;

    if (argc < 2) {
        printf("Usage: SBWAV8 file.wav\n");
        return 1;
    }

    f = fopen(argv[1], "rb");
    if (!f) { printf("Could not open the file\n"); return 1; }

    if (!leer_header_wav(f, &h)) { fclose(f); return 1; }

    detectar_sb(&sb);
    sb_base = sb.base_port;
    irq_num = sb.irq;

    printf("Card at %Xh, IRQ %d, DMA8 %d\n", sb_base, irq_num, sb.dma8);
    printf("WAV: %lu Hz, %u channel(s), %u bits\n",
           h.sample_rate, h.num_channels, h.bits_per_sample);

    if (!dsp_reset(sb_base)) {
        printf("No DSP found at that port\n");
        fclose(f);
        return 1;
    }

    mixer_unmute_max(sb_base);
    mixer_mostrar_canales_dma(sb_base);   /* diagnostics: which DMA channels the card has */

    buffer = asignar_buffer_alineado(BUF_SIZE, 65536UL, &buffer_bloque_original);   /* aligned to 64 KB (the 8 bit channel boundary) */
    if (!buffer) { printf("Not enough memory for the buffer\n"); fclose(f); return 1; }

    fisica = ((unsigned long)FP_SEG(buffer) << 4) + FP_OFF(buffer);

    fread(buffer,             1, HALF_SIZE, f);
    fread(buffer + HALF_SIZE, 1, HALF_SIZE, f);

    instalar_isr(irq_num);

    dsp_set_sample_rate(sb_base, (unsigned)h.sample_rate);
    dma8_setup(sb.dma8, fisica, BUF_SIZE);

    dsp_start_playback_8(sb_base, (unsigned)HALF_SIZE, h.num_channels == 2);

    mitad_a_rellenar = 0;

    printf("Playing (8 bit, DMA channel %d)... press any key to stop\n", sb.dma8);

    while (!feof(f)) {
        while (!buffer_listo) {
            if (kbhit()) { getch(); goto fin; }
        }
        buffer_listo = 0;

        {
            char *destino = buffer + (mitad_a_rellenar * HALF_SIZE);
            unsigned long leidos = fread(destino, 1, HALF_SIZE, f);
            if (leidos < HALF_SIZE)
                memset(destino + leidos, 128, HALF_SIZE - leidos); /* silence = 128 in unsigned 8 bit   */
        }
        mitad_a_rellenar = !mitad_a_rellenar;
    }

    while (!buffer_listo) ;

fin:
    dsp_write(sb_base, 0xDA);           /* leave 8 bit auto-init mode         */
    restaurar_isr(irq_num);

    /* By now neither the DMA nor the IRQ is touching the buffer, so the memory
       can finally be released. It frees farmalloc's ORIGINAL pointer, not the
       aligned one that was used for playback. */
    if (buffer_bloque_original != NULL) {
        farfree(buffer_bloque_original);
        buffer_bloque_original = NULL;
    }

    fclose(f);
    printf("Playback finished\n");
    return 0;
}
