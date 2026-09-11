# Chapter 14 — Music: playing from disk

*[Versión en español](README.md)*

**What you will get:** 2.7 MB of music playing on a 640 KB machine.

```
make
chap14
```

`1` shot over the top · `+`/`-` volume · `P` stop

---

## 1. The arithmetic that changes everything

| | |
|---|---|
| `res/prody8.wav` | **2,719,788 bytes** |
| Memory on a DOS machine | 640 KB |

**The song is more than four times the machine's total memory**, operating system
included.

`load_sound()` cannot handle that, and not because of a bug: it is physically
impossible.

## 2. Streaming: do not load it, read it in pieces

The answer is never to have it whole:

1. Read a piece of the file
2. Play it
3. Read the next one
4. When the file runs out, go back to the start

That is **streaming**. The piece is **16,384 bytes**: 0.37 seconds at 44100 Hz.

```c
static unsigned char song_buffer[SONG_BUFFER_SIZE];
```

A fixed buffer, reserved once. And here is the nice part:

| | Memory it costs |
|---|---|
| An effect | The size of the WAV |
| **A song** | **16 KB, however long it is** |

An hour-long song would cost the same 16 KB. Only the number of trips to disk
changes.

## 3. The requirement that catches everyone

> **The song has to be EXACTLY 44100 Hz, mono, 8-bit.**

Effects do not: `load_sound()` takes any rate and **converts** on load. It does it
**once, at startup**, and it does not mind taking half a second.

`play_song()` cannot do that. It reads the file **while it plays**, and there is
nowhere to convert on the fly: no time (you are mid-match) and no memory (that is
the whole problem).

If the song is not at 44100, `play_song()` says so in the log and stays silent.

## 4. Music and effects share the mixer

The same `sound_update()` from chapter 12:

- Mixes whatever effects are playing
- **And on top of that** refills the song's buffer by reading more file when
  needed

That is why effects are heard **over** the music without cutting it: to the mixer
the song is just another voice.

## 5. Why the disk is not a problem

Reading 16 KB off disk is not instant, but you have **0.37 seconds of headroom**
before the buffer runs dry. Any reasonable read fits in that.

If your loop stalls much longer than that, you hear the gap. Same symptom as
chapter 12's `U`.

## 6. Experiments

1. **Convert the song to 22050 Hz** and load it. `play_song()` rejects it and says
   so in the log.
2. **Lower `SONG_BUFFER_SIZE`** in `sound.c` to 4096 and rebuild. Less headroom:
   one slow frame and it cuts.
3. **Turn the music volume to maximum** and fire. Clipping, as in chapter 13.
4. **Remove the `sound_update()`.** The music cuts out after 0.37 seconds.

## 7. What to take away

| | |
|---|---|
| A song does not fit in memory. Not remotely | 2.7 MB in 640 KB |
| **Streaming: read it in pieces while it plays** | 16 KB, however long |
| The song **must already be at 44100** | Nowhere to convert on the fly |
| Music and effects, the same mixer | Which is why they coexist |

---

**Previous:** [Chapter 13](../../ch13/doc/README-EN.md) ·
**Next:** [Chapter 15 — The IPX driver](../../ch15/doc/README-EN.md)
