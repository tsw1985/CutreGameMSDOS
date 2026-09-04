// ============================================================
//   SBWAV.C - 16 bit WAV player over DMA with a double buffer
//   Turbo C++ 3.0 - LARGE memory model
//   ============================================================ 

// Compile : tcc -ml sbwav.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <alloc.h>
#include <conio.h>

// ---------- WAV format ---------- 

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
    if (h->bits_per_sample != 16) {
        printf("This player only supports 16 bit (the file has %u)\n",
               h->bits_per_sample);
        return 0;
    }
    return 1;
}

// ---------- Card detection (the BLASTER variable) ---------- 

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

// ---------- DSP ports ---------- 

#define DSP_RESET(b)       (b + 0x6)
#define DSP_READ(b)        (b + 0xA)
#define DSP_WRITE(b)       (b + 0xC)
#define DSP_READ_STATUS(b) (b + 0xE)
#define DSP_ACK16(b)       (b + 0xF)

void dsp_write(unsigned base, unsigned char val)
{
    while (inp(DSP_WRITE(base)) & 0x80) ;   // wait until the port is free        
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

void dsp_set_sample_rate(unsigned base, unsigned rate)
{
    dsp_write(base, 0x41);              // set the output rate (SB16)        
    dsp_write(base, (rate >> 8) & 0xFF);
    dsp_write(base, rate & 0xFF);
}

// Starts playback in 16 bit auto-init mode.
//   'num_words_bloque' is the length of ONE HALF of the buffer, in 16 bit words
//   (not bytes). The DSP raises an IRQ each time it completes a block that size.

void dsp_start_playback_16(unsigned base, unsigned num_words_bloque, int stereo)
{
    unsigned char modo = 0x10;          // signed (16 bit WAVs are signed)           
    unsigned cuenta = num_words_bloque - 1;

    if (stereo) modo |= 0x20;

    dsp_write(base, 0xB6);              // 16 bit, output, auto-init  
    dsp_write(base, modo);
    dsp_write(base, cuenta & 0xFF);
    dsp_write(base, (cuenta >> 8) & 0xFF);
}

// ---------- DMA controller (8237), 16 bit channels (4-7) ---------- 

void dma16_setup(int channel, unsigned long phys_addr, unsigned long length_bytes)
{
    unsigned page, addr, count;
    int chan_rel = channel - 4;

    static unsigned page_port[4]  = {0x8F, 0x8B, 0x89, 0x8A};
    static unsigned addr_port[4]  = {0xC0, 0xC4, 0xC8, 0xCC};
    static unsigned count_port[4] = {0xC2, 0xC6, 0xCA, 0xCE};

    // For 16 bit channels the address is given in WORDS, not in bytes 
    page  = (unsigned)((phys_addr >> 16) & 0xFF);
    addr  = (unsigned)((phys_addr >> 1) & 0xFFFF);
    count = (unsigned)((length_bytes / 2) - 1);

    outp(0xD4, 0x04 | chan_rel);        // mask the channel while programming it   
    outp(0xD8, 0x00);                   // clear the high/low byte flip-flop       

    // mode: single (01) | auto-init (1) | read memory->device (10) | channel 
    outp(0xD6, 0x40 | 0x10 | 0x08 | chan_rel);

    outp(addr_port[chan_rel], addr & 0xFF);
    outp(addr_port[chan_rel], (addr >> 8) & 0xFF);
    outp(page_port[chan_rel], page);

    outp(count_port[chan_rel], count & 0xFF);
    outp(count_port[chan_rel], (count >> 8) & 0xFF);

    outp(0xD4, chan_rel);               // unmask: ready to transfer               
}

// ---------- Allocating a buffer aligned to a page boundary ---------- 

char *asignar_buffer_alineado(unsigned long size, unsigned long align)
{
    char *bloque;
    unsigned long fisica, resto;

    bloque = farmalloc(size + align);
    if (!bloque) return NULL;

    fisica = ((unsigned long)FP_SEG(bloque) << 4) + FP_OFF(bloque);
    resto  = fisica % align;
    if (resto != 0) fisica += (align - resto);

    // Teaching note: recalculating the pointer loses the original reference to
    //   'bloque', which is what farfree() would need. It is simplified here
    //   because DOS releases all of the process's memory when the program ends.
    //   In a real project you should keep 'bloque' separately.
    return (char far *) MK_FP((unsigned)(fisica >> 4), 0);
}

// ---------- IRQ / ISR ---------- 

unsigned sb_base;
int irq_num;
void interrupt (*old_isr)(void);
volatile int buffer_listo = 0;

void interrupt isr_sb(void)
{
    inp(DSP_ACK16(sb_base));            // acknowledge the 16 bit IRQ to the DSP   

    buffer_listo = 1;

    outp(0x20, 0x20);                   // EOI to the master PIC 
    if (irq_num >= 8) outp(0xA0, 0x20); // EOI to the slave PIC if needed 
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

// ---------- Main program ---------- 

#define HALF_SIZE 16384UL               // 16 KB per half  
#define BUF_SIZE  (HALF_SIZE * 2)

int main(int argc, char *argv[])
{
    FILE *f;
    WavHeader h;
    SBConfig sb;
    char *buffer;
    unsigned long fisica;
    int mitad_a_rellenar;

    if (argc < 2) {
        printf("Usage: SBWAV file.wav\n");
        return 1;
    }

    f = fopen(argv[1], "rb");
    if (!f) { printf("Could not open the file\n"); return 1; }

    if (!leer_header_wav(f, &h)) { fclose(f); return 1; }

    detectar_sb(&sb);
    
    
    sb_base = sb.base_port;
    irq_num = sb.irq;

    printf("Card at %Xh, IRQ %d, DMA16 %d\n", sb_base, irq_num, sb.dma16);
    printf("WAV: %lu Hz, %u channel(s), %u bits\n",
           h.sample_rate, h.num_channels, h.bits_per_sample);

     
    //       
    if (!dsp_reset(sb_base)) {
        printf("No DSP found at that port\n");
        fclose(f);
        return 1;
    }

    buffer = asignar_buffer_alineado(BUF_SIZE, 131072UL);   // aligned to 128 KB 
    if (!buffer) { printf("Not enough memory for the buffer\n"); fclose(f); return 1; }

    fisica = ((unsigned long)FP_SEG(buffer) << 4) + FP_OFF(buffer);

    // Fill both halves BEFORE starting, so it does not start on silence 
    fread(buffer,             1, HALF_SIZE, f);
    fread(buffer + HALF_SIZE, 1, HALF_SIZE, f);

    
    instalar_isr(irq_num);

    
    
    dsp_set_sample_rate(sb_base, (unsigned)h.sample_rate);

    //start comment here. dma16 break
    
    dma16_setup(sb.dma16, fisica, BUF_SIZE);

    
    
    
    // DSP block = one half, expressed in 16 bit WORDS 
    dsp_start_playback_16(sb_base, (unsigned)(HALF_SIZE / 2), h.num_channels == 2);

    mitad_a_rellenar = 0;   // the first half to refill is half 0           

    printf("Playing... press any key to stop\n");

    
    while (!feof(f)) {
        while (!buffer_listo) {
            if (kbhit()) { getch(); goto fin; }
        }
        buffer_listo = 0;

        {
            char *destino = buffer + (mitad_a_rellenar * HALF_SIZE);
            unsigned long leidos = fread(destino, 1, HALF_SIZE, f);
            if (leidos < HALF_SIZE)
                memset(destino + leidos, 0, HALF_SIZE - leidos); // silence at the end 
        }
        mitad_a_rellenar = !mitad_a_rellenar;
    }

    // let the last half already loaded play out 
    while (!buffer_listo) ;

fin:
    dsp_write(sb_base, 0xD9);           // leave 16 bit auto-init mode         
    
    //end comment here
    restaurar_isr(irq_num);
    
    
    
    fclose(f);
    printf("Playback finished\n");
    return 0;
}
