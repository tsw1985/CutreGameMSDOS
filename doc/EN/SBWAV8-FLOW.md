# SBWAV8 - How the code flows

A document about **flow**, not theory: who calls whom, in what order, and where
everything lives. What the DSP or the DMA controller *is* you already have in
your manual; this only explains the path the program takes.

Main file: `sbwav8.c` (an **8 bit** WAV player using DMA with a double buffer).

---

## 1. Files in the folder

| File | What it is |
|---|---|
| `sbwav8.c` | The 8 bit player. **This is the one that works, and the one we are going to use.** |
| `SBWAV.C` | The earlier **16 bit** version. Same skeleton, different DMA channel. |
| `SBWAV8.EXE`, `*.OBJ` | Binaries built on the DOS machine. |
| `b.bat` | Build-and-test script. |
| `prodigy.wav` | The original 16 bit WAV (for `SBWAV.C`). |
| `prody8.wav` | The same one converted to 8 bits (for `sbwav8.c`). |

`b.bat` does three things:

```
del sbwav8.exe
tcc -ml sbwav8.c      <- LARGE model is mandatory (far pointers for the DMA buffer)
sbwav8.exe prody8.wav
```

---

## 2. Call graph

```
main()
 |
 |-- fopen(argv[1])
 |-- leer_header_wav(f, &h) ................ validates RIFF/WAVE/fmt/data, PCM, 8 bit
 |
 |-- detectar_sb(&sb) ...................... reads the BLASTER environment variable
 |     `-- getenv("BLASTER")                 -> base_port, irq, dma8, dma16
 |
 |-- dsp_reset(sb_base) .................... resets the DSP and waits for the 0xAA
 |     |-- outp(DSP_RESET)
 |     `-- inp(DSP_READ_STATUS) / inp(DSP_READ)
 |
 |-- mixer_unmute_max(sb_base) ............. turns every volume up
 |     `-- mixer_write() x6
 |
 |-- mixer_mostrar_canales_dma(sb_base) .... DIAGNOSTICS ONLY (register 0x81)
 |
 |-- asignar_buffer_alineado(BUF_SIZE, 64K, &buffer_bloque_original)
 |     `-- farmalloc() ..................... reserves 32 KB aligned to 64 KB;
 |                                           returns the aligned pointer and leaves
 |                                           the original in buffer_bloque_original
 |
 |-- fread() x2 ............................ preloads BOTH halves of the buffer
 |
 |-- instalar_isr(irq_num) ................. hooks isr_sb() onto the IRQ vector
 |     |-- getvect() / setvect()
 |     `-- outp(0x21 / 0xA1)                 unmasks the IRQ in the PIC
 |
 |-- dsp_set_sample_rate(sb_base, rate)
 |     `-- dsp_write() x3                    command 0x41 plus the rate
 |
 |-- dma8_setup(sb.dma8, physical, BUF_SIZE) programs the 8237 in auto-init mode
 |     `-- outp(0x0A, 0x0C, 0x0B, channel ports, page port)
 |
 |-- dsp_start_playback_8(base, HALF_SIZE, stereo)
 |     `-- dsp_write() x4                    command 0xC6 plus mode and count
 |
 |-- [ PLAYBACK LOOP ]  <---- see section 4
 |
 `-- fin:
       |-- dsp_write(base, 0xDA) ........... leave 8 bit auto-init mode
       |-- restaurar_isr(irq_num)
       |     |-- outp(0x21 / 0xA1)           masks the IRQ again
       |     `-- setvect(vector, old_isr)    puts the original vector back
       |-- farfree(buffer_bloque_original) . frees the block (no DMA or IRQ left)
       `-- fclose(f)


isr_sb()   <---- nothing in C ever calls it: the hardware fires it
 |-- inp(DSP_ACK8(sb_base)) ................ acknowledges the IRQ to the DSP
 |-- buffer_listo = 1 ...................... the only thing it says to main()
 `-- outp(0x20, 0x20) ...................... EOI to the PIC
```

---

## 3. Startup, step by step

The order **matters**, and it is deliberate:

1. **Open and validate the WAV** (`leer_header_wav`). The whole `WavHeader`
   struct is read in one `fread`. That is why it carries `#pragma pack(push,1)`:
   without it Turbo C would align the fields and the read would come out
   skewed. It only accepts "canonical" WAVs with a 44 byte header: if the file
   carries extra chunks (LIST, fact...), the `strncmp` on `data_id` fails and it
   is rejected.

2. **Find the card** (`detectar_sb`). It parses `BLASTER` (e.g.
   `A220 I5 D1 H5 T6`) letter by letter. If the variable does not exist it
   leaves the defaults (0x220 / IRQ 5 / DMA 1) and returns 0, **but the program
   carries on anyway**: `main` never checks the return value.

3. **Reset the DSP** (`dsp_reset`). This is the only real "is there a card
   there?" check: if the `0xAA` never arrives, it gives up.

4. **Turn the volumes up** (`mixer_unmute_max`). It writes both the old mixer
   registers (0x22, 0x04) and the SB16 ones (0x30-0x33), so it works whatever
   the model is.

5. **Reserve the buffer** (`asignar_buffer_alineado`). It asks `farmalloc` for
   `32 KB + 64 KB`, works out the **physical** address (`SEG<<4 + OFF`) and
   rounds it up to the next multiple of 64 KB. This is mandatory: an 8 bit DMA
   channel cannot cross a 64 KB page boundary, and if it does the sound either
   cuts out or loops on itself.

   Since the aligned pointer usually does **not** match the one `farmalloc`
   returned, and `farfree` only accepts the original, the function hands back
   **both**: the aligned one as its return value (the one used for playback) and
   the original through the `bloque_original` parameter, which `main` keeps in
   `buffer_bloque_original` to free at `fin:`.

6. **Preload both halves** with two 16 KB `fread`s. By the time the DMA starts
   there are already 32 KB of audio waiting.

7. **Install the ISR** (`instalar_isr`). It translates IRQ to vector
   (IRQ 0-7 -> 0x08+irq, IRQ 8-15 -> 0x70+irq-8), saves the old vector in
   `old_isr` and unmasks the line in the PIC.

8. **Set the rate, program the DMA and start**, in exactly this order:
   `dsp_set_sample_rate` -> `dma8_setup` -> `dsp_start_playback_8`.
   The DMA is programmed with the **whole** buffer (32 KB) and the DSP with
   **one half** (16 KB). That asymmetry is the heart of the double buffer: the
   DMA goes round and round the 32 KB without stopping (auto-init), and the DSP
   raises an IRQ every time it has consumed 16 KB, that is, at the end of each
   half.

---

## 4. The playback loop

```c
while (!feof(f)) {
    while (!buffer_listo) {              /* busy wait */
        if (kbhit()) { getch(); goto fin; }
    }
    buffer_listo = 0;

    destino = buffer + (mitad_a_rellenar * HALF_SIZE);
    leidos  = fread(destino, 1, HALF_SIZE, f);
    if (leidos < HALF_SIZE)
        memset(destino + leidos, 128, HALF_SIZE - leidos);

    mitad_a_rellenar = !mitad_a_rellenar;
}
while (!buffer_listo) ;                  /* let the last half play out */
```

How the two "threads" split the work:

```
        HARDWARE (DMA + DSP)                     PROGRAM (main)
        --------------------                     --------------
        reads the buffer by itself               waits in while(!buffer_listo)
        finishes half 0
             |
             `--> IRQ --> isr_sb()
                            buffer_listo = 1  -->  wakes up
                                                   refills half 0 with fread
        half 1 is already playing                  mitad_a_rellenar = 1
        finishes half 1
             |
             `--> IRQ --> isr_sb()
                            buffer_listo = 1  -->  refills half 1
        ...
```

In other words: **the hardware is always playing one half while the program
refills the other**. That is why there are no gaps.

Details of the loop:

- `buffer_listo` **must** be `volatile`: without it the compiler sees that
  nothing inside the `while` ever changes it and optimises the loop into an
  infinite hang.
- `memset(..., 128, ...)`: in unsigned 8 bit audio, silence is 128, not 0. This
  pads the tail of the last block so the end of the file is not garbage.
- `mitad_a_rellenar` starts at 0 and flips with `!`, always one half behind the
  DMA.
- Quitting on a keypress is a `goto fin`, which is the only cleanup path: you
  have to go through `dsp_write(0xDA)` + `restaurar_isr()` no matter what.
  **If the program ends without that, the machine is left with the IRQ hooked
  and the DMA still spinning.**

---

## 5. The ISR

`isr_sb()` is deliberately minimal. It does three things and no more:

1. `inp(DSP_ACK8(sb_base))` - tells the DSP the IRQ has been serviced. In 8 bit
   mode the ack port is the **same** as the status port (`base+0xE`); in the 16
   bit version it is `base+0xF`. Mixing them up is the classic bug: the first
   half plays and then silence, because the second IRQ never arrives.
2. Sets the `buffer_listo` flag. **It does not read from disk and does not call
   DOS.** All the heavy work is left to `main()`.
3. Sends the EOI to the PIC (0x20 to the master, and to the slave too if the
   IRQ is 8 or above).

The globals it shares with `main()` are `sb_base`, `irq_num`, `old_isr` and
`buffer_listo`. They are globals precisely because an ISR cannot take
parameters.

---

## 6. Differences from `SBWAV.C` (the 16 bit version)

Same skeleton, same sequence of calls. Only what depends on the sample size
changes:

| | `SBWAV.C` (16 bit) | `sbwav8.c` (8 bit) |
|---|---|---|
| Bit depth accepted | 16 | 8 |
| DSP command | `0xB6` | `0xC6` |
| Mode byte | `0x10` (signed) | `0x00` (unsigned) |
| DSP count | in **words** (bytes/2) | in **bytes** |
| IRQ ack | `base+0xF` | `base+0xE` |
| DMA channels | 4-7 (16 bit controller) | 0-3 (8 bit controller) |
| DMA ports | `0xC0/0xC2/0x8F...` tables | `channel*2` and `channel*2+1` |
| Alignment | 128 KB | 64 KB |
| Padding silence | 0 | 128 |
| Mixer | leaves it alone | `mixer_unmute_max` + diagnostics |

The 8 bit one started life as a diagnostic version, to rule out the card's 16
bit DMA channel. In the end it is the one that sounds right.

---

## 7. What had to change to put it in the game

> This section was the plan. **It has all been done**: the result is
> `src/sound.c`, documented in [`SOUND.md`](SOUND.md). It is kept here because
> it explains why that module looks the way it does.

As it stands this is a standalone program. To use it inside `CutreGameMSDOS`:

- **`main()` has to go**, split into something like `sound_init(file)` /
  `sound_update()` / `sound_stop()`, because the game already has its own
  `main()` in `src/main.c`.
- **The busy wait `while (!buffer_listo)` cannot stay.** In the game,
  `sound_update()` would be a function called once per frame that does
  `if (buffer_listo) { refill; }` and returns straight away, without blocking
  the loop.
- **IRQs living together:** the game already installs its own INT 9 vector
  (IRQ 1, keyboard) in `install_kbd()`. The card uses IRQ 5, so they are
  different vectors and do not clash; but both ISRs still have to stay short.
- **Cleanup is not optional:** `dsp_write(0xDA)`, `restaurar_isr()` and
  `farfree(buffer_bloque_original)` have to run on the same exit path where the
  game restores the video mode and the keyboard vector. Leaving by any other
  route leaves the machine in a bad state. **Order matters:** stop the DSP and
  drop the IRQ first, and only then free the buffer; the other way round you
  would be releasing memory the DMA is still reading.
- **Reading from disk with `fread`** while the game runs can stutter on slow
  machines. For short effects (a shot, an explosion) the sensible thing is to
  load the whole WAV into memory once and never touch the disk during play.
- `printf` cannot be used once the game is in graphics mode 13h: the error
  messages from these functions would have to go out through `tanks_log()`.
