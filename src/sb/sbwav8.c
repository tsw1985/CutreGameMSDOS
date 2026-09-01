/* ============================================================
   SBWAV8.C - Reproductor de WAV 8 bits via DMA con doble buffer
   Version de DIAGNOSTICO: usa el canal DMA de 8 bits (controlador 1)
   en vez del de 16 bits, para descartar si el problema esta en el
   canal de 16 bits concreto de la tarjeta.
   Turbo C++ 3.0 - modelo de memoria LARGE
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <alloc.h>
#include <conio.h>

/* ---------- Formato WAV ---------- */

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
        printf("Error leyendo el header (archivo truncado?)\n");
        return 0;
    }
    if (strncmp(h->riff_id, "RIFF", 4) != 0 ||
        strncmp(h->wave_id, "WAVE", 4) != 0 ||
        strncmp(h->fmt_id,  "fmt ", 4) != 0 ||
        strncmp(h->data_id, "data", 4) != 0) {
        printf("No es un WAV valido, o tiene chunks extra no soportados\n");
        return 0;
    }
    if (h->audio_format != 1) {
        printf("Solo se soporta PCM sin comprimir\n");
        return 0;
    }
    if (h->bits_per_sample != 8) {
        printf("Esta version de diagnostico solo soporta 8 bits (archivo tiene %u)\n",
               h->bits_per_sample);
        return 0;
    }
    return 1;
}

/* ---------- Deteccion de la tarjeta (variable BLASTER) ---------- */

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
        printf("Variable BLASTER no encontrada, uso valores por defecto\n");
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

/* ---------- Puertos del DSP ---------- */

#define DSP_RESET(b)       (b + 0x6)
#define DSP_READ(b)        (b + 0xA)
#define DSP_WRITE(b)       (b + 0xC)
#define DSP_READ_STATUS(b) (b + 0xE)
#define DSP_ACK8(b)        (b + 0xE)   /* para 8 bits el ack de IRQ es el MISMO puerto que el de estado */

void dsp_write(unsigned base, unsigned char val)
{
    while (inp(DSP_WRITE(base)) & 0x80) ;   /* espera a que el puerto quede libre */
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

/* ---------- Mixer (registros de volumen) ---------- */

#define MIXER_ADDR(b) (b + 0x4)
#define MIXER_DATA(b) (b + 0x5)

void mixer_write(unsigned base, unsigned char index, unsigned char value)
{
    outp(MIXER_ADDR(base), index);
    outp(MIXER_DATA(base), value);
}

void mixer_unmute_max(unsigned base)
{
    mixer_write(base, 0x22, 0xFF);   /* volumen maestro (formato antiguo) */
    mixer_write(base, 0x04, 0xFF);   /* volumen de voz/DAC (formato antiguo) */
    mixer_write(base, 0x30, 0xFF);   /* volumen maestro izquierdo (SB16) */
    mixer_write(base, 0x31, 0xFF);   /* volumen maestro derecho (SB16) */
    mixer_write(base, 0x32, 0xFF);   /* volumen DAC izquierdo (SB16) */
    mixer_write(base, 0x33, 0xFF);   /* volumen DAC derecho (SB16) */
}

/* Lee y muestra el registro 0x81 del mixer: indica que canales DMA
   tiene la tarjeta habilitados de verdad (bits 0,1,3 = 8 bits;
   bits 5,6,7 = 16 bits). Util para el diagnostico. */
void mixer_mostrar_canales_dma(unsigned base)
{
    unsigned char val;
    outp(MIXER_ADDR(base), 0x81);
    val = inp(MIXER_DATA(base));
    printf("Mixer reg 0x81 (canales DMA habilitados) = 0x%02X\n", val);
    printf("  8 bits -> canal0:%d canal1:%d canal3:%d\n",
           (val & 0x01) != 0, (val & 0x02) != 0, (val & 0x08) != 0);
    printf("  16bits -> canal5:%d canal6:%d canal7:%d\n",
           (val & 0x20) != 0, (val & 0x40) != 0, (val & 0x80) != 0);
}

void dsp_set_sample_rate(unsigned base, unsigned rate)
{
    dsp_write(base, 0x41);              /* fijar frecuencia de salida (SB16) */
    dsp_write(base, (rate >> 8) & 0xFF);
    dsp_write(base, rate & 0xFF);
}

/* Arranca la reproduccion en modo auto-init de 8 bits.
   'num_bytes_bloque' es la longitud de UNA MITAD del buffer, en BYTES
   (a diferencia de la version de 16 bits, aqui no se divide entre 2). */
void dsp_start_playback_8(unsigned base, unsigned num_bytes_bloque, int stereo)
{
    unsigned char modo = 0x00;          /* SIN signo: los WAV de 8 bits son unsigned (0-255) */
    unsigned cuenta = num_bytes_bloque - 1;

    if (stereo) modo |= 0x20;

    dsp_write(base, 0xC6);              /* 8 bits, salida, auto-init */
    dsp_write(base, modo);
    dsp_write(base, cuenta & 0xFF);
    dsp_write(base, (cuenta >> 8) & 0xFF);
}

/* ---------- Controlador DMA (8237), canales de 8 bits (0-3) ---------- */

void dma8_setup(int channel, unsigned long phys_addr, unsigned long length_bytes)
{
    unsigned page, addr, count;
    unsigned addr_port, count_port, page_port;

    /* En el controlador de 8 bits, los puertos de direccion/cuenta de cada
       canal son consecutivos: canal*2 (direccion) y canal*2+1 (cuenta). */
    addr_port  = channel * 2;
    count_port = channel * 2 + 1;

    switch (channel) {
        case 0: page_port = 0x87; break;
        case 1: page_port = 0x83; break;
        case 2: page_port = 0x81; break;
        case 3: page_port = 0x82; break;
        default: return;   /* canal invalido para este controlador */
    }

    /* A diferencia del canal de 16 bits, aqui la direccion y la cuenta
       se expresan directamente en BYTES, sin dividir entre 2. */
    page  = (unsigned)((phys_addr >> 16) & 0xFF);
    addr  = (unsigned)(phys_addr & 0xFFFF);
    count = (unsigned)(length_bytes - 1);

    outp(0x0A, 0x04 | channel);         /* enmascara el canal mientras se programa */
    outp(0x0C, 0x00);                   /* limpia el flip-flop byte alto/bajo */

    /* modo: single (01) | auto-init (1) | lectura mem->dispositivo (10) | canal */
    outp(0x0B, 0x40 | 0x10 | 0x08 | channel);

    outp(addr_port, addr & 0xFF);
    outp(addr_port, (addr >> 8) & 0xFF);
    outp(page_port, page);

    outp(count_port, count & 0xFF);
    outp(count_port, (count >> 8) & 0xFF);

    outp(0x0A, channel);                /* desenmascara: listo para transferir */
}

/* ---------- Reserva de buffer alineado a un limite de pagina ---------- */

/* Devuelve un puntero ALINEADO dentro del bloque reservado, que es el que se
   usa para reproducir y para programar el DMA. Ese puntero normalmente NO
   coincide con el que devolvio farmalloc(), y farfree() solo acepta el
   original, asi que este se devuelve aparte en 'bloque_original' para poder
   liberarlo al terminar.

   Al salir:
     - valor de retorno  -> usar para el audio y para el DMA
     - *bloque_original  -> pasar a farfree() (queda a NULL si fallo la reserva) */
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
    inp(DSP_ACK8(sb_base));             /* confirma la IRQ de 8 bits ante el DSP */

    buffer_listo = 1;

    outp(0x20, 0x20);                   /* EOI al PIC maestro */
    if (irq_num >= 8) outp(0xA0, 0x20); /* EOI al PIC esclavo si aplica */
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

/* ---------- Programa principal ---------- */

#define HALF_SIZE 16384UL               /* 16 KB por mitad */
#define BUF_SIZE  (HALF_SIZE * 2)

int main(int argc, char *argv[])
{
    FILE *f;
    WavHeader h;
    SBConfig sb;
    char *buffer;
    char *buffer_bloque_original;   /* el puntero tal cual lo dio farmalloc: es el que hay que liberar */
    unsigned long fisica;
    int mitad_a_rellenar;

    if (argc < 2) {
        printf("Uso: SBWAV8 archivo.wav\n");
        return 1;
    }

    f = fopen(argv[1], "rb");
    if (!f) { printf("No se pudo abrir el archivo\n"); return 1; }

    if (!leer_header_wav(f, &h)) { fclose(f); return 1; }

    detectar_sb(&sb);
    sb_base = sb.base_port;
    irq_num = sb.irq;

    printf("Tarjeta en %Xh, IRQ %d, DMA8 %d\n", sb_base, irq_num, sb.dma8);
    printf("WAV: %lu Hz, %u canal(es), %u bits\n",
           h.sample_rate, h.num_channels, h.bits_per_sample);

    if (!dsp_reset(sb_base)) {
        printf("No se detecta el DSP en el puerto indicado\n");
        fclose(f);
        return 1;
    }

    mixer_unmute_max(sb_base);
    mixer_mostrar_canales_dma(sb_base);   /* dato de diagnostico: que canales DMA tiene la tarjeta */

    buffer = asignar_buffer_alineado(BUF_SIZE, 65536UL, &buffer_bloque_original);   /* alineado a 64 KB (limite del canal de 8 bits) */
    if (!buffer) { printf("Sin memoria para el buffer\n"); fclose(f); return 1; }

    fisica = ((unsigned long)FP_SEG(buffer) << 4) + FP_OFF(buffer);

    fread(buffer,             1, HALF_SIZE, f);
    fread(buffer + HALF_SIZE, 1, HALF_SIZE, f);

    instalar_isr(irq_num);

    dsp_set_sample_rate(sb_base, (unsigned)h.sample_rate);
    dma8_setup(sb.dma8, fisica, BUF_SIZE);

    dsp_start_playback_8(sb_base, (unsigned)HALF_SIZE, h.num_channels == 2);

    mitad_a_rellenar = 0;

    printf("Reproduciendo (8 bits, canal DMA %d)... pulsa una tecla para detener\n", sb.dma8);

    while (!feof(f)) {
        while (!buffer_listo) {
            if (kbhit()) { getch(); goto fin; }
        }
        buffer_listo = 0;

        {
            char *destino = buffer + (mitad_a_rellenar * HALF_SIZE);
            unsigned long leidos = fread(destino, 1, HALF_SIZE, f);
            if (leidos < HALF_SIZE)
                memset(destino + leidos, 128, HALF_SIZE - leidos); /* silencio = 128 en unsigned 8 bits */
        }
        mitad_a_rellenar = !mitad_a_rellenar;
    }

    while (!buffer_listo) ;

fin:
    dsp_write(sb_base, 0xDA);           /* sale del modo auto-init de 8 bits */
    restaurar_isr(irq_num);

    /* Aqui ya no hay ni DMA ni IRQ tocando el buffer, asi que ahora si se
       puede soltar la memoria. Se libera el puntero ORIGINAL de farmalloc,
       no el alineado que se ha estado usando para reproducir. */
    if (buffer_bloque_original != NULL) {
        farfree(buffer_bloque_original);
        buffer_bloque_original = NULL;
    }

    fclose(f);
    printf("Reproduccion terminada\n");
    return 0;
}
