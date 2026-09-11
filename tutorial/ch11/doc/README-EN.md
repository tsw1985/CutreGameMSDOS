# Chapter 11 — Finding the Sound Blaster

*[Versión en español](README.md)*

**What you will get:** knowing whether there is a sound card and where it is. In
text mode, so you can read it.

**Which real code is used:** `src/sound.c`.

```
make
chap11
```

---

## 1. There are no drivers in DOS

This is the first thing to accept. No plug and play, no registry, no layer that
tells you what hardware you have. **Your program talks to the card directly.**

And to talk to it you need three things: which **port** it is at, which **IRQ** it
uses, and which **DMA** channel.

## 2. The BLASTER variable

The convention the industry settled on was an environment variable set by the
card's installer:

```
SET BLASTER=A220 I5 D1 H5 P330 T6
```

| | |
|---|---|
| `A220` | Base port, in hexadecimal |
| `I5` | IRQ, the interrupt it uses |
| `D1` | 8-bit DMA channel |
| `H5` | 16-bit DMA channel |
| `P330` | MIDI port |
| `T6` | Card type |

`sound.c` cares about **A, I and D**. If the variable is absent it tries A220 I5
D1, which is the most common.

## 3. What a port is

A port is an address, but in **a separate space from memory**. It is read and
written with different instructions: `IN` and `OUT`, which in Turbo C are
`inportb()` and `outportb()`.

It is how the processor talks to hardware. The VGA has its own (the `0x3DA` of
the retrace, the `0x3C8`/`0x3C9` of the palette), the keyboard has its own
(`0x60`), and the Sound Blaster has its own.

## 4. What the DSP is, and how the card is detected

The **DSP** is the part of the card that handles digital sound. You send it
one-byte commands over its ports.

Detection is a handshake:

1. Write a **1** to `base + 6` (the reset port)
2. Wait a moment
3. Write a **0** to the same port
4. Read from the data port

If a card is there, it answers **0xAA**. That byte is the whole detection: if it
arrives, there is a card; if it does not within a timeout, there is not.

## 5. The log: why `sound.c` is a library

```c
	sound_set_log(my_log);
```

`sound.c` **cannot write anywhere.** It does no `printf`, opens no files, knows
nothing about `game.log`. All it does is call a function you hand it.

- The game hands it `tanks_log()`, which writes to `game.log`.
- This chapter hands it one that does `printf`, because we are still in text
  mode.

That is what makes `sound.c` a **library** rather than "this game's sound": it
imposes nothing on the program using it. You could lift it into another project
as is.

**Call it before `sound_start()`**, or you miss the startup messages.

## 6. Sound is optional, and that is a decision

If `sound_start()` returns 0, **the game runs exactly the same, in silence**.
Every sound call checks whether there is a card and does nothing if there is not.

Never let a missing sound card stop someone playing. It sounds obvious, and there
are games from the era that refuse to start.

## 7. `sound_end()` is not optional

```c
	sound_end();
```

It gives the IRQ back to whoever had it and **stops the DMA**.

Leave the DMA running on exit and the card keeps reading memory that is no longer
yours: **you get noise until you reboot**. It is the sound equivalent of not
restoring the keyboard vector.

## 8. Experiments

1. **Remove `sound_set_log()`.** Complete silence: the card is found just the
   same but you learn nothing.
2. **In DOSBox, set `sbtype=none`** in `dosbox.conf` and run it. You get to see
   the failure path.
3. **Change `sbbase=220` to `sbbase=240`** in DOSBox without touching BLASTER.
   `sound.c` will look where it is not.
4. **Print `BLASTER` in your DOSBox.** Compare with what it detects.

## 9. What to take away

| | |
|---|---|
| **There are no drivers in DOS** | Your program talks to the hardware |
| The `BLASTER` variable says where the card is | A, I, D |
| A port is a separate address space | `IN` / `OUT` |
| Detection is a reset and waiting for `0xAA` | |
| **You hand it the log** | Which is what makes it a library |
| `sound_end()` or noise until reboot | |

---

**Previous:** [Chapter 10](../../ch10/doc/README-EN.md) ·
**Next:** [Chapter 12 — Playing a WAV](../../ch12/doc/README-EN.md)
