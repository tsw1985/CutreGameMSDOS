# Chapter 12 — Playing a WAV

*[Versión en español](README.md)*

**What you will get:** sound. And an understanding of DMA, which is the piece
that makes audio possible on a machine of this era.

```
make
chap12
```

`1` shot, `2` explosion, **`U` disables `sound_update()`** ← try that one.

---

## 1. Loading is not playing

```c
	fire = load_sound("..\\..\\res\\fire.wav");   /* slow: there is a disk */
	...
	play_sound(fire);                              /* instant */
```

`load_sound()` opens the WAV, puts it in memory, converts it to 44100 Hz if
needed, and returns **a number**.

`play_sound()` takes that number and starts it playing.

That is why **every sound is loaded at startup** and only `play_sound()` is
called during the game. Load the WAV at the moment of firing and the game would
stop for half a second every time.

If `load_sound()` returns **-1** it could not. And note that this is **not checked
before each `play_sound()`**: `play_sound(-1)` does nothing. So the game sounds
just as good with whatever files are missing.

## 2. What DMA is

Here is the idea of the chapter.

The card **cannot read your arrays**. What it can do is **DMA**: direct memory
access. You tell it:

> "Start reading at this address, this many bytes, at this rate"

and from then on **the card pushes those bytes out of the speaker on its own,
without bothering the processor.**

That is what makes sound possible on a 486: if the processor had to hand over
44,100 samples a second by hand, there would be no time left for the game.

## 3. The buffer is tiny, and split in two

```
   4096 bytes in total, less than a tenth of a second

   +------------------+------------------+
   |     half 0       |     half 1       |
   +------------------+------------------+
```

The card reads round and round: half 0, half 1, half 0…

When it **finishes a half**, it fires an interrupt.

## 4. Why the interrupt does not do the work

`sound.c`'s interrupt routine does the bare minimum: it notes *"half 0 needs
refilling"* and leaves.

**It does not mix in there.** An interrupt routine runs in the middle of whatever
the program was doing, so it has to last as little as possible. Mixing 2,048
samples inside an interrupt is asking for trouble.

It is exactly the same decision as in the keyboard handler: the interrupt marks,
the normal loop works.

## 5. And that is why `sound_update()` exists

```c
		sound_update();
```

From your normal loop, it sees the note the interrupt left and **refills the half
that is due**, mixing every sound currently playing.

**You have to call it every time round the loop.** If you do not, the card reads
whatever was already in the buffer and you hear the last fragment on repeat.

Press `U` in the program and listen. That noise is exactly what happens when your
loop stalls for more than a tenth of a second.

That is why the game calls `sound_update()` **even inside the network wait** of
chapter 18: a hiccup on the network must not become a hiccup in the sound.

## 6. Volume is per sound

```c
	set_sound_volume(fire, 32);
```

There is no global volume. Each sound has its own, from 0 to `SOUND_VOLUME_MAX`.
Chapter 13 explains why that matters so much.

## 7. Experiments

1. **Press U and fire.** The noise you hear is the buffer repeating.
2. **Put a long empty `for` in the loop** to fake a slow frame. Same effect,
   without touching `sound_update()`.
3. **Fire twice in quick succession.** Both copies overlap: that is already
   mixing, and it is the next chapter.
4. **Load a WAV that does not exist.** `load_sound()` returns -1 and
   `play_sound(-1)` does nothing. The program does not flinch.

## 8. What to take away

| | |
|---|---|
| Loading and playing are different things | Load at startup, play in the game |
| **DMA: the card reads memory on its own** | Without it there is no audio on a 486 |
| A small buffer split into two halves | Under 0.1 s in total |
| The interrupt only marks; the loop works | Same as the keyboard |
| **`sound_update()` every time round** | Or the last fragment repeats |

---

**Previous:** [Chapter 11](../../ch11/doc/README-EN.md) ·
**Next:** [Chapter 13 — Mixing](../../ch13/doc/README-EN.md)
