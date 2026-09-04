# SOUND.md — how the game's sound works

A document about `src/sound.c` and `header/sound.h`. It explains what each
function does, in what order they are called, how the game uses them, and
**why** each decision was taken.

It goes with [`SBWAV8-FLOW.md`](SBWAV8-FLOW.md), which documents the original
`sbwav8.c` player this code came from.

---

## 1. The problem to start with

`sbwav8.c` plays **one** WAV: it reads it off the disk and feeds it to the
card. The game needs two things that program does not do:

**Several sounds at once.** The Sound Blaster has **one single DAC**. There is
no way to tell it "play these three files": the hardware only knows how to
consume one stream of bytes. The only way out is to **add the samples up
ourselves** before handing them over. That is a software mixer, and it is the
heart of this module.

**Never blocking.** `sbwav8.c` sits in a `while (!buffer_ready)` and reads from
disk in the middle of playback. In a game running at 70 fps that is
unacceptable: an `fread` that takes 40 ms eats three frames. Here the WAVs are
loaded whole into memory at startup, and the function called every frame
returns immediately unless there is real work to do.

---

## 2. The hardware concepts

Four ideas you need clear for the rest to make sense.

### DMA

The CPU does not hand the samples to the card one by one. You tell the **DMA
controller** (the 8237 chip) "here is a block of memory, you give it to the
card". From then on the transfer happens by itself, with the CPU doing
nothing. That is why music can play while the game draws.

### Auto-init

In normal mode the DMA transfers the block and stops. In **auto-init** mode,
when it reaches the end it goes back to the start and carries on **forever**.
The buffer becomes a circle. Nothing ever has to be restarted.

### Double buffer

If the buffer is a circle the card is reading from, writing into the part it is
reading right now would be heard as a click. The fix is to split it into two
halves:

```
   +------------------+------------------+
   |      half 0      |      half 1      |
   +------------------+------------------+
     the card reads      we write
     from this one       into this one

   ...and when it finishes half 0, the roles swap
```

The card is always reading from one half while we refill the other. They never
tread on each other.

### The interrupt

How do we know when a half has finished? The card tells us: you program it with
a "block size" equal to half a half, and it **raises an IRQ** every time it
consumes that much. That IRQ is what says "I am done with this half, refill
it for me".

With that, the full life cycle of the sound is:

```
  startup -> the DMA is programmed in auto-init and the DSP is started
                      |
                      v
     the card eats half 0 ---> IRQ ---> sound_isr()
                      |                     |
                      |               raises a flag
                      |                     |
                      |                     v
                      |         (next frame) sound_update()
                      |                     |
                      |          sound_mix_half() refills half 0
                      v
     the card eats half 1 ---> IRQ ---> ... and round again
```

---

## 3. The two data structures

The whole mixer rests on telling a **sound** apart from a **voice**.

```c
struct sound_sample {          // a WAV loaded into memory
    unsigned char far *data;   // the samples, already at SOUND_SAMPLE_RATE
    unsigned char far *block;  // what farmalloc() returned, so it can be freed
    unsigned long length;      // how many samples
};

struct sound_voice {           // something that is playing right now
    int is_playing;
    int is_looping;
    int sample_id;             // which sound it is playing
    int volume;                // out of SOUND_VOLUME_MAX (64)
    unsigned long position;    // how far into it it has got
};
```

**A sample is the sound; a voice is one playback of that sound.** The
distinction matters: `fire.wav` is a single sample loaded once, but it plays
through two different voices, player 1's and player 2's. If they were the same
thing, one shot would cut the other one off.

The voices are **fixed**, not handed out dynamically:

| Voice | What plays on it |
|---|---|
| `SOUND_VOICE_ENGINE_1` | player 1's engine, looping |
| `SOUND_VOICE_ENGINE_2` | player 2's engine, looping |
| `SOUND_VOICE_FIRE_1` | player 1's shot |
| `SOUND_VOICE_FIRE_2` | player 2's shot |
| `SOUND_VOICE_EXPLOSION` | dying, shared |

**Why fixed and not a pool.** With a pool ("give me the first free voice") you
would need priority rules to decide who to steal from when none are left, and a
shot could cut off an engine. With fixed voices the behaviour is always the
same and known in advance: firing twice in a row only cuts off **your own**
previous shot, never anything else. In a two tank game with five sounds, a pool
would be complexity with no benefit whatsoever.

The explosion does share a voice on purpose: if both tanks die on the same
frame you hear **one** bang and not two on top of each other, which would be
twice as loud and would distort.

---

## 4. The functions, one by one

### 4.1 Startup

#### `sound_init()` — line 603

The way in. It does this, in order:

1. Clears every sample and voice
2. `sound_detect_card()` — where the card is
3. `sound_dsp_reset()` — does it answer?
4. `sound_load_sample()` × 4 — loads the WAVs
5. `sound_alloc_dma_buffer()` — memory for the DMA
6. Fills the buffer with **silence**
7. `sound_mixer_unmute()` — turns the volumes up
8. `sound_install_isr()` — takes over the IRQ
9. `sound_dsp_set_sample_rate()` — 16000 Hz
10. `sound_dma_setup()` — programs the 8237
11. `sound_dsp_start_playback()` — go

It returns **1 if there is sound and 0 if there is not**, and when it returns 0
it sets `sound_is_ready = 0`, which makes **every** other function here return
without doing anything. The game does not have to check for anything: with no
card it plays exactly the same, in silence. That is why a failing
`sound_init()` does not abort the game.

Step 6 is not cosmetic. The DMA starts reading **the instant** the DSP is
started, and whatever it reads is what you hear. If the buffer held whatever
happened to be in that memory, the first thing you would hear on entering the
game would be a roar of garbage.

#### `sound_detect_card()` — line 347

Reads the `BLASTER` environment variable, which the card's driver sets:

```
BLASTER=A220 I5 D1 H5 T6
         |    |  |
         |    |  +-- 8 bit DMA channel
         |    +----- IRQ
         +---------- base port, in hex
```

If it does not exist, it uses the usual values (A220 I5 D1). It never fails:
what decides whether there is really a card is the next step.

#### `sound_dsp_reset()` — line 163

Resets the DSP and waits for it to answer `0xAA`. That `0xAA` is how the card
says "I am here". If it has not answered after 200 tries, there is no card at
that port and `sound_init()` backs out cleanly.

**This is the real detection.** `sound_detect_card()` only reads an environment
variable, which can be wrong or point at a card that is no longer there.

#### `sound_load_sample()` — line 427

Reads a whole WAV into memory and, if it has to, **resamples it**.

About the resampling: the DSP has **one single output rate**. Our files do not
agree:

| File | Original rate |
|---|---|
| `fire.wav` | 16000 Hz |
| `engip1.wav` | **22255 Hz** |
| `engip2.wav` | 16000 Hz |
| `died.wav` | 16000 Hz |

If `engip1.wav` were played at 16000 it would sound **slow and deep**, because
its samples were meant to be consumed faster. So it is rebuilt at 16000 as it
loads:

```c
destination_length = (source_length * SOUND_SAMPLE_RATE) / header.sample_rate;

for (i = 0; i < destination_length; i++){
    destination[i] = source[(i * header.sample_rate) / SOUND_SAMPLE_RATE];
}
```

This is the cheapest method there is, taking the nearest sample. It is not high
fidelity, but for an engine and an explosion it is plenty, and it **happens
once at startup**, so it costs nothing while you play. The alternative would
have been resampling on the fly in the mixer, which would be a fixed point add
per sample: more code and more CPU every frame, for nothing.

The header is read in one go with `fread(&header, sizeof(WavHeader), 1, f)`,
which assumes the WAV has exactly 44 bytes of header and no extra chunks.
Checked on all four files: `fmt ` at 12 and `data` at 36. A WAV with a `LIST`
metadata chunk would break this, which is why all four signatures are validated
before going on.

#### `sound_alloc_dma_buffer()` — line 391

Reserves the buffer's memory, aligned to a 64 KB boundary.

**Why.** 8 bit DMA channels carry an address counter of only 16 bits, plus a
separate page register for the high bits. On reaching the end of a 64 KB page
**the counter wraps to the start of that same page** instead of moving on to
the next one. A buffer straddling a boundary would play its second half as
noise.

The fix is to ask for 64 KB more than needed and start at the first boundary
that falls inside the block, which makes crossing one impossible.

```c
block = farmalloc(size + align);
physical = ((unsigned long)FP_SEG(block) << 4) + FP_OFF(block);
remainder = physical % align;
if (remainder != 0){
    physical = physical + (align - remainder);
}
return MK_FP((unsigned int)(physical >> 4), 0);
```

**The pointer it returns is NOT the one `farmalloc()` gave**, and `farfree()`
only accepts the original. That is why the original comes out separately, in
`original_block`, and is kept in `sound_buffer_block` so it can be freed at the
end. It is a classic silent bug: freeing the aligned one corrupts the heap.

> A version that only asked for `size * 2` and simply avoided **crossing** a
> boundary (which is the real hardware rule) was tried, to save 64 KB of
> conventional memory. It works just as well on paper, but this one was put
> back because it is the one that had been proven on real hardware for a long
> time. When something touches DMA and works, you do not touch it for elegance.

#### `sound_mixer_unmute()` — line 192

Turns every mixer volume up to maximum. It writes **both sets of registers**,
the old one (`0x22`, `0x04`) and the SB16 one (`0x30`-`0x33`), because we do
not know what card is really there. Writing to registers a card does not have
is harmless; not writing them and having them muted is not.

#### `sound_install_isr()` — line 291

Two things:

1. Saves the old interrupt vector and puts ours in
2. **Unmasks the line in the PIC**, so the interrupt actually arrives

```c
outportb(0x21, inportb(0x21) & ~(1 << irq));
```

Without the second step the PIC keeps blocking that IRQ and it never arrives,
however correct the vector is. Turning an IRQ into a vector is not direct:
IRQs 0-7 go to vectors `0x08 + irq` and 8-15 to `0x70 + (irq - 8)`, because
they belong to two different controllers.

This lives happily alongside the game's keyboard `INT 9`: they are different
vectors and each handler sends its own EOI.

#### `sound_dma_setup()` — line 229

Programs the 8237. The order **matters** and is not negotiable:

```c
outportb(0x0A, 0x04 | channel);   // 1. mask: do not transfer while being programmed
outportb(0x0C, 0x00);             // 2. clear the high/low byte flip-flop
outportb(0x0B, 0x40|0x10|0x08|channel);  // 3. mode: single + auto-init + memory->device
outportb(addr_port,  addr & 0xFF);       // 4. address, low byte
outportb(addr_port,  (addr >> 8) & 0xFF);//    address, high byte
outportb(page_port,  page);              //    page (bits 16-23)
outportb(count_port, count & 0xFF);      // 5. count, low byte
outportb(count_port, (count >> 8) & 0xFF);//   count, high byte
outportb(0x0A, channel);          // 6. unmask: it may transfer now
```

The **flip-flop** in step 2 is the classic trap: the address and the count are
written to the same port, twice each, and the chip keeps an internal bit that
decides whether what arrives is the low or the high byte. If it is not cleared
first, it may be in the opposite state to the one you expect and the bytes get
swapped: the address comes out backwards and the DMA reads from anywhere.

The page register is not consecutive, each channel has its own (`0x87`, `0x83`,
`0x81`, `0x82`), hence the `switch`.

#### `sound_dsp_start_playback()` — line 215

```c
sound_dsp_write(base, 0xC6);   // 8 bit, output, auto-init
sound_dsp_write(base, 0x00);   // UNsigned
sound_dsp_write(base, count & 0xFF);
sound_dsp_write(base, (count >> 8) & 0xFF);
```

The `0x00` mode byte says **unsigned**, which is correct: in an 8 bit WAV the
samples run 0 to 255 with silence at 128, not -128 to 127. Getting it wrong is
heard as brutal distortion, because silence would be read as full volume.

`count` is the size of **half** a half minus one, and it is what decides how
often the IRQ arrives.

### 4.2 Running

#### `sound_isr()` — line 278

The interrupt handler. It does the least it possibly can:

```c
inportb(DSP_ACK8(sound_base_port));   // acknowledge the IRQ to the DSP
sound_buffer_ready = 1;               // raise the flag
outportb(0x20, 0x20);                 // EOI to the master PIC
if (sound_irq >= 8){ outportb(0xA0, 0x20); }
```

**Why it does not mix here.** It would be the natural thing: a half has
finished, refill it now. But an ISR runs with interrupts disabled, so anything
slow in it delays every other one — the game's keyboard included. Mixing 512
samples across 5 voices inside the interrupt would make the keyboard start
dropping keypresses. Raising a flag and leaving takes microseconds.

The `inportb` on the ack port is **mandatory**. Without it the DSP considers
the interrupt unserviced and never raises another one: the sound stops after
the first. For 8 bit that port is `base+0xE`, the same as the status port (for
16 bit it would be `base+0xF`, and that is exactly the bug in the original
`SBWAV.C`).

`sound_buffer_ready` is **`volatile`**. Without it the compiler sees that
nothing in `sound_update()` ever writes that variable and may optimise the
check away entirely. `volatile` tells it "this changes on its own, always read
it from memory".

#### `sound_update()` — line 714

Called **once per frame** from the main loop.

```c
if (sound_is_ready == 0){ return; }
if (sound_buffer_ready == 0){ return; }   // <-- it almost always leaves here

sound_buffer_ready = 0;
sound_mix_half(sound_buffer + (sound_half_to_fill * SOUND_HALF_SIZE));
sound_half_to_fill = !sound_half_to_fill;
```

The important line is the **second one**: at 16000 Hz with halves of 512
samples, the card asks for data about 31 times a second, and the loop runs at
70. Which means **more than half the frames this function does two comparisons
and leaves**. The cost of sound in the game is practically zero except for
those 31 times.

In the main loop it sits **outside** the `if` that separates the two states of
a round:

```c
if (explosion_pause_counter == 0){ ...playing... }else{ ...burning... }

sound_update();     // <-- outside, always
```

If it were inside the "playing" branch, during the half second of the explosion
nobody would refill the buffer, the card would replay the last half over and
over, and the bang would come out as a stutter. The sound has to keep running
**always**, whatever the game is doing.

#### `sound_mix_half()` — line 531

The mixer. This is where "several sounds at once" happens.

```c
// 1. clear the accumulator
for (i...) sound_mix_buffer[i] = 0;

// 2. add every active voice
for (each active voice){
    for (i = 0; i < SOUND_HALF_SIZE; i++){
        if the sample ran out: back to the start (loop) or switch the voice off
        value = (int)sample->data[voice->position] - 128;   // to signed
        value = (value * voice->volume) / 64;               // volume
        sound_mix_buffer[i] += value;
        voice->position++;
    }
}

// 3. clip and back to a byte
for (i...){
    value = sound_mix_buffer[i];
    if (value >  127) value =  127;
    if (value < -128) value = -128;
    destination[i] = value + 128;
}
```

Three decisions here:

**The `- 128` and the `+ 128`.** In unsigned 8 bit, silence is 128, not 0.
Adding the raw samples would add each voice's 128 too and saturate instantly
with just two sounds. They have to be brought into a range centred on zero,
added there, and put back into the card's format at the end.

**The accumulator is an `int`, not a `char`.** Five voices added together go
comfortably beyond what fits in a byte. And that is precisely what you need to
be able to see in order to clip it properly: accumulating in a byte would wrap
around without us ever noticing.

**It clips, it does not divide.** The alternative to clipping would be dividing
the sum by the number of voices, which never distorts but makes **everything
get quieter the moment anything else plays**. A shot would sound weaker just
because an engine is running. Clipping is what a real mixing desk does: if you
push too hard it distorts, and distorting a peak sounds infinitely better than
wrapping, which would turn the loudest part of a shot into a horrible click.

That is why the engines sit at `SOUND_VOLUME_ENGINE 34` out of 64, a little
over half: two engines flat out plus a shot would be clipping constantly.

### 4.3 What the game uses

Only four functions, and none of them knows anything about the hardware.

#### `sound_play(voice, sample_id, volume)` — line 744

One shot, from the beginning. If that voice was already playing, it is cut off.

#### `sound_loop(voice, sample_id, volume)` — line 762

The same but looping, **with one detail that is the key to how it is used**:

```c
if (sound_voices[voice].is_playing == 1 &&
    sound_voices[voice].is_looping == 1 &&
    sound_voices[voice].sample_id == sample_id){
    return;                                  // already playing: touch nothing
}
```

Without that check, calling `sound_loop()` every frame while the player holds
the key down would restart the engine **70 times a second**: it would never get
past the first millisecond of the WAV and you would only hear a buzz. With it,
the game can ask for "engine on" on every single frame without thinking, which
is exactly what it does.

#### `sound_stop(voice)` / `sound_stop_all()` — lines 790 and 802

They silence. They deliberately do not check `sound_is_ready`, only the index
bounds, so that `sound_init()` can use them before anything is ready.

### 4.4 Shutting down

#### `sound_shutdown()` — line 679

**The order is the only thing that matters here, and it is critical:**

```c
sound_dsp_write(sound_base_port, 0xDA);   // 1. leave auto-init
sound_dma_stop(sound_dma);                // 2. mask the DMA channel
sound_restore_isr(sound_irq);             // 3. give the interrupt back
farfree(sound_buffer_block);              // 4. NOW free it
```

While the card is in auto-init, **the DMA keeps reading that memory on its
own**, without going through the CPU. Freeing it before stopping it means the
DMA would carry on playing memory that now belongs to somebody else: noise at
best, and at worst a hang that is hard to explain once the heap reuses it. And
giving the interrupt vector back before stopping the card means an IRQ would
arrive at the old handler, which expects none of this.

It is called from `main.c:501`, **before** `player_free()` and
`bmp_delete_buffers()`.

---

## 5. Where the game calls it

| `src/main.c` | What |
|---|---|
| `:269` | `sound_init()`, after `init_graphics()` |
| `:417` | `sound_update()`, once per frame, outside the state `if` |
| `:312` | `sound_update()` again, inside the network wait. See below |
| `:993` | `sound_play()` for the shot, **only if `player_fire_bullet()` returns 1** |
| `:1019` / `:1021` | `sound_loop()` / `sound_stop()` for the engine, driven by `is_driving` |
| `:384` `:385` | `sound_stop()` on both engines when a tank is hit |
| `:391` | `sound_play()` for the explosion |
| `:501` | `sound_shutdown()` on the way out |

Four decisions in the wiring:

**The engine uses `is_driving`, not `is_moving`.** `is_moving` is turned off
again by the track animation as soon as it advances a frame, so the engine
would cut in and out several times a second. `is_driving` is simply "a
direction key is held", which also means pushing against a wall keeps revving,
which is what it should sound like.

**The engines have to be stopped by hand on a hit.** During the explosion
pause the keyboard is not read, so nothing else would ever switch them off and
they would keep looping while the tanks burn.

**The shot only sounds if a bullet came out.** `player_fire_bullet()` returns 1
or 0 and the sound hangs off that. Otherwise, pressing fire with your bullet
still in the air would go "bang" with nothing coming out. The rule lives in
`player_fire_bullet()`, not duplicated in `main.c`, so the two cannot drift
apart if the firing conditions ever change.

**Over the network there is a second `sound_update()`.** In network mode the
loop stops to wait for the other machine's keys, and that wait can last longer
than a frame if the network is having a bad moment. Without calling the sound
in there, the card would replay the last half and you would hear a stutter at
exactly the worst time. It is the same reasoning as putting it outside the
state `if`, applied to the other way the loop can come to a halt. See
`NETWORK.md`.

Each tank carries in its `struct player` which voice and which sample it uses
(`sound_engine_voice`, `sound_engine_sample`, `sound_fire_voice`), so that
`process_player_input()` stays generic and does not have to know that player 1
is the one with `engip1.wav`.

---

## 6. The numbers you can change

In `header/sound.h` and at the top of `src/sound.c`:

| Constant | Value | What happens if you change it |
|---|---|---|
| `SOUND_SAMPLE_RATE` | 16000 | Higher = better quality and more CPU. The WAVs resample themselves to whatever you set. |
| `SOUND_HALF_SIZE` | 512 | **The compromise that matters.** See below. |
| `SOUND_VOLUME_FIRE` | 64 | Out of 64. Full. |
| `SOUND_VOLUME_ENGINE` | 34 | Lower it if the engines drown out the shots. |
| `SOUND_VOLUME_DIED` | 64 | |

About `SOUND_HALF_SIZE`, the only delicate one. At 16000 Hz, 512 samples are
**32 ms**:

- It is the **maximum delay** between pressing fire and hearing it.
- It is also the **margin** you have to refill before the card runs out of
  data.

The loop runs at ~70 Hz, that is 14 ms a frame, so two frames fit inside each
half: there is margin, but not a lot. If you hear stuttering on the real
machine (the sign that a frame took too long and the card repeated a half),
**raise it to 1024**: the margin doubles to 64 ms, at the cost of the shot
being heard a fraction later.

---

## 7. Two problems that took some finding

They are kept here because they will come back if more files are added.

### `Code has no effect` on every `outp()`

Compiling `sound.c` inside the game's Makefile produced 21
`Code has no effect` warnings, one per `outp(...)`, and none on the `inp(...)`.

The difference between them is that `inp()`'s value is used inside an
`if`/`while` and `outp()`'s is thrown away. With **`-O2`**, Turbo C considers
throwing the result away to be code with no effect. `sbwav8.c` never showed it
because its `b.bat` compiles with `tcc -ml`, **without optimisation**.

The fix was already in the project: `bmp.c` uses **`outportb()`** from
`dos.h`, which returns `void`, and compiles clean with `-O2`. All 26 port
accesses in `sound.c` were changed to `outportb()` / `inportb()`. They write
and read exactly the same byte on exactly the same port.

### `Fatal: Unable to execute command: tcc`

Adding `sound.obj` stopped the link from working. It was not the Makefile:

```
before:  tcc -mh -ebin\game.exe  bin\main.obj ... bin\gameloop.obj   -> 110 characters
after:   ...  bin\gameloop.obj  bin\sound.obj                        -> 127 characters
```

**DOS only passes 127 characters of arguments** to a program (the PSP command
tail). One more `.obj` used it up exactly, and MAKE could not launch `tcc`. The
error does not mean `tcc` is missing, it means it could not be started.

It was fixed with a **response file**, which is how the big projects of the era
did it for this very reason:

```make
	@echo bin\main.obj bin\util.obj bin\video.obj > bin\link.rsp
	@echo bin\bmp.obj bin\players.obj bin\gameloop.obj >> bin\link.rsp
	@echo bin\sound.obj bin\net.obj >> bin\link.rsp
	$(LD) $(LDFLAGS) -ebin\game.exe @bin\link.rsp
```

The line drops to 36 characters. **If you add another `.c`, put its `.obj` in
one of the `echo`s** (and if one `echo` ever gets too long, split it into
another `>>`).

---

## 8. If nothing plays

Look at the log. `sound_init()` leaves its diagnosis there, because in graphics
mode nothing can be printed to the screen:

| Line in the log | What is wrong |
|---|---|
| `Sound: ready` | All good |
| `Sound: no Sound Blaster found...` | The DSP did not answer. Check the `BLASTER` variable; A220 I5 D1 is assumed by default. |
| `Sound: could not load X.wav` | The file is not in `res\`, or it is not 8 bit mono PCM |
| `Sound: not enough memory...` | The DMA buffer did not fit (64 KB + 1 KB) |

And if no line appears at all, `sound_init()` was never reached.
