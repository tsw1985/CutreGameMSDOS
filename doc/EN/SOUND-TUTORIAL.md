# Tutorial: using the sound module in any DOS project

`src/sound.c` is a **standalone module**. It knows nothing about tanks or
about this game: you tell it which WAV files to load, it gives you a number
for each one, and it plays them when you ask.

This document explains how to use it in a new program. If what you want is to
understand how it works inside (the DMA, the interrupt, the mixing), that is
in [SOUND.md](SOUND.md).

---

## 1. What do I copy into my project

**Two files, and nothing else:**

```
src/sound.c
header/sound.h
```

They depend on no other file in this repository. Their only dependencies are
standard Turbo C headers:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <alloc.h>
#include <conio.h>
```

To build it, one line in your Makefile like any other:

```
tcc -c -O2 -mh -Iheader -obin\sound.obj src\sound.c
```

And add `bin\sound.obj` to the link list.

---

## 2. The four steps

It is always these four, in this order. There is nothing more to it.

```c
#include "header\sound.h"

int boom;

int main()
{
    /* ---- STEP 1: switch the card on ---- */
    if (sound_start() == 1){

        /* ---- STEP 2: load the sounds ---- */
        boom = load_sound("..\\res\\boom.wav");

    }

    while (playing){

        /* ---- STEP 3: once per loop, ALWAYS ---- */
        sound_update();

        if (something_exploded){
            play_sound(boom);
        }

    }

    /* ---- STEP 4: switch it off ---- */
    sound_end();

    return 0;
}
```

And that is it. That is a program with sound.

### The four steps, one at a time

**`sound_start()`** finds the Sound Blaster, takes over its interrupt and
starts the DMA playing silence. It returns **1 if there is a card and 0 if
there is not**.

The important part: if it returns 0, **you do not have to do anything
special**. Every other function checks for itself that there is no card and
quietly does nothing. Your program runs exactly the same, in silence, without
a single extra `if` scattered through your code.

**`load_sound(path)`** loads a WAV and gives you back **a number**, which is
how you refer to it from then on. It returns **-1** if it could not (missing
file, wrong format, no free slots).

A -1 is also safe to keep and use: `play_sound(-1)` simply does nothing. You
do not have to check for it unless you want to.

**`sound_update()` IS NOT OPTIONAL.** It is the one of the four you can forget
and break the sound with. Stop calling it and the card replays the piece it
already had, over and over, and it stutters.

Call it inside **any loop that waits** for something too: while a level loads,
while it waits for a key, while it waits on the network. The moment your main
loop stops for more than an instant, this still has to be called.

It is cheap: almost every time it checks whether the card has asked for more
data, sees that it has not, and returns immediately.

**`sound_end()`** is required before leaving. Skip it and the DMA carries on
reading memory that is no longer yours, and the interrupt still points at a
program that is no longer there. That hangs the machine.

---

## 3. Playing and stopping

Three functions, and you need no more:

```c
play_sound(boom);              /* plays ONCE */

loop_sound(engine);            /* plays over and over */
stop_looping_sound(engine);    /* that is enough */
```

### The trick with `loop_sound()`

It can be called on **every single frame** with no problem. If that sound is
already looping, it does absolutely nothing.

That is why a car engine is this simple to write:

```c
if (key_held){
    loop_sound(engine);
}else{
    stop_looping_sound(engine);
}
```

That `loop_sound()` runs seventy times a second while the key is held, and the
sound **does not restart**. Without that guard you would hear a click seventy
times a second instead of an engine.

### You do not have to think about voices

A "voice" is one slot in the mixer: there can be **8 sounds at once**.
`play_sound()` finds a free slot on its own.

It gives you back the voice number it used, but **you can safely ignore it**.
It is only useful if you want to cut that particular sound short before it
ends:

```c
int voice;

voice = play_sound(alarm);
/* ...later... */
stop_sound(voice);
```

And if you want complete silence:

```c
stop_all_sounds();
```

### If all 8 voices fill up

`play_sound()` takes over the voice that is closest to finishing, since it was
about to go quiet anyway. It **never takes over a looping sound**, so a shot
cannot silence the engine. If absolutely everything were looping, it returns
-1 and the new sound is not heard.

---

## 4. Volume

Volume belongs to **the sound**, not to each call. In practice an engine
always wants to be in the background and a shot always wants to be in front:

```c
engine = load_sound("..\\res\\engine.wav");
set_sound_volume(engine, 16);     /* half: in the background */

boom = load_sound("..\\res\\boom.wav");
set_sound_volume(boom, 64);       /* double: over everything else */
```

The scale is `SOUND_VOLUME_MAX`, which is **32**:

| Value | What it does |
| --- | --- |
| 32 | The sound exactly as recorded |
| below 32 | Quieter |
| above 32 | **Amplified** |

Amplifying is allowed and normal. If the sum of everything goes past what fits
in a byte, the mixer **clamps** it instead of wrapping round: the sound
distorts a little, which is infinitely better than the horrible crack that
overflow would produce.

Every sound starts at 32, so if you never call `set_sound_volume()` everything
plays at its original level.

---

## 5. What the WAVs have to be

This is the first thing to check when a sound "does not play":

| Requirement | Value |
| --- | --- |
| Bits | **8** (not 16) |
| Channels | **1**, mono (not stereo) |
| Encoding | **PCM**, uncompressed |
| Sample rate | Anything |

The first three are compulsory: if they are not met, `load_sound()` returns -1
and that sound does not exist.

The rate does not matter: if the WAV was recorded at 22050 Hz and the card
runs at 16000, it is **converted when loaded**. It happens once, at startup,
so it costs nothing while playing.

To convert a file with `sox`:

```
sox input.wav -b 8 -c 1 -e unsigned-integer -r 16000 output.wav
```

And in Audacity: *Track → Split Stereo to Mono*, then *File → Export →
WAV 8-bit unsigned PCM*.

---

## 6. Finding out what is going on (optional)

The library is **silent by default**. It never prints anything.

That is deliberate: a program in graphics mode cannot write to the screen, a
`printf()` would paint garbage over the game. So the library does not decide
for you where the text goes; you tell it:

```c
void my_log(char *message)
{
    FILE *f;

    f = fopen("debug.log", "a");

    if (f != NULL){
        fprintf(f, "%s\n", message);
        fclose(f);
    }
}

    /* before sound_start() */
    sound_set_log(my_log);
```

From then on it tells you things like `Sound: ready` or `Sound: no Sound
Blaster found, playing without sound`.

If you never call `sound_set_log()`, nothing bad happens: it carries on
working, quietly.

And this is also what makes `sound.c` copyable: it does not have to include
any file of yours in order to write to your log.

---

## 7. A complete program, start to finish

This compiles and works as it is:

```c
#include <stdio.h>
#include <conio.h>
#include "header\sound.h"

int main()
{
    int boom;
    int engine;
    int key;

    if (sound_start() == 0){
        printf("No Sound Blaster. Leaving.\n");
        return 1;
    }

    boom   = load_sound("boom.wav");
    engine = load_sound("engine.wav");

    if (boom == -1 || engine == -1){
        printf("Could not load the WAV files.\n");
        sound_end();
        return 1;
    }

    set_sound_volume(engine, 16);

    printf("SPACE = explosion   M = engine on   N = engine off   ESC = quit\n");

    while (1){

        /* ALWAYS, on every pass */
        sound_update();

        if (kbhit()){

            key = getch();

            if (key == 27){                  /* ESC */
                break;
            }

            if (key == ' '){
                play_sound(boom);
            }

            if (key == 'm' || key == 'M'){
                loop_sound(engine);
            }

            if (key == 'n' || key == 'N'){
                stop_looping_sound(engine);
            }

        }

    }

    sound_end();

    return 0;
}
```

Notice that `sound_update()` is outside the `if (kbhit())`. It has to run on
**every** pass of the loop, whether a key was pressed or not.

---

## 8. Full reference

| Function | What it does |
| --- | --- |
| `sound_start()` | Switches the card on. **1 = there is sound, 0 = there is not** |
| `sound_end()` | Switches off and frees. **Required before leaving** |
| `sound_update()` | **Once per loop, always** |
| `load_sound(path)` | Loads a WAV. Returns its number, or **-1** |
| `set_sound_volume(id, vol)` | Out of 32. Above that it amplifies |
| `play_sound(id)` | Plays once. Returns the voice (can be ignored) |
| `loop_sound(id)` | Plays on a loop. Calling it repeatedly is harmless |
| `stop_looping_sound(id)` | Stops that looping sound |
| `stop_sound(voice)` | Stops that particular voice |
| `stop_all_sounds()` | Complete silence |
| `sound_set_log(function)` | Where to report problems. Optional |

Constants you may change in `sound.h`:

| Constant | Value | What it is |
| --- | --- | --- |
| `SOUND_MAX_SAMPLES` | 8 | How many different WAVs can be loaded |
| `SOUND_MAX_VOICES` | 8 | How many sounds can be heard at once |
| `SOUND_VOLUME_MAX` | 32 | The volume scale |

And inside `sound.c`, if you ever need them:

| Constant | Value | What it is |
| --- | --- | --- |
| `SOUND_SAMPLE_RATE` | 16000 | The rate everything comes out at |
| `SOUND_HALF_SIZE` | 512 | How long the card takes to ask for more (32 ms) |

Lowering `SOUND_HALF_SIZE` makes shots heard sooner, but leaves less margin if
a frame takes too long. Raising it is safer but the sound lags behind the
picture.

---

## 9. Common mistakes

| Symptom | Almost certainly |
| --- | --- |
| The sound stutters | You are not calling `sound_update()` in some loop that waits |
| `load_sound()` returns -1 | The WAV is not 8 bit mono PCM, or the path is wrong |
| `sound_start()` returns 0 | No card, or the `BLASTER` variable is not set |
| A very fast clicking noise | You are using `play_sound()` every frame where you meant `loop_sound()` |
| The machine hangs on exit | `sound_end()` is missing |
| One sound cuts another off | All 8 voices are full. Raise `SOUND_MAX_VOICES` |
| Everything sounds distorted | Volumes too high adding up. Turn the background ones down |

### About the BLASTER variable

The library reads the `BLASTER` environment variable, which is where the sound
card driver leaves a note saying where it is:

```
SET BLASTER=A220 I5 D1 H5 T6
```

`A` is the port, `I` the interrupt, `D` the 8 bit DMA channel. If that
variable does not exist, the usual values are tried (A220 I5 D1) and the card
is left to answer or not.

In DOSBox it is already set for you.

---

## 10. And how this repository's game uses it

This is exactly how `src/main.c` does it:

```c
sound_set_log(tanks_log);

if (sound_start() == 1){

    sound_fire     = load_sound("..\\res\\fire.wav");
    sound_engine_1 = load_sound("..\\res\\engip1.wav");
    sound_engine_2 = load_sound("..\\res\\engip2.wav");
    sound_died     = load_sound("..\\res\\died.wav");

    set_sound_volume(sound_fire,     64);
    set_sound_volume(sound_engine_1, 34);
    set_sound_volume(sound_engine_2, 34);
    set_sound_volume(sound_died,     64);

}
```

The engines at 34 and the shots at 64, so a shot is heard over two engines at
full throttle without the sum blowing up.

And in the main loop, a single line:

```c
sound_update();
```
