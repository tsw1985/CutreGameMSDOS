# Chapter 13 — Mixing: several sounds at once

*[Versión en español](README.md)*

**What you will get:** two engines and a shot playing simultaneously. And hearing
the distortion when you push it too far.

```
make
chap13
```

`1` shot · `2` explosion · `3`/`4` engines · **`5` volumes to maximum** ·
`0` stop everything

---

## 1. The card has one channel

Here is the surprise. An 8-bit Sound Blaster plays **one stream of samples**.
One. It has no eight channels or anything like it.

So how do two engines and a shot play at once?

**You add them yourself, by hand, sample by sample, before handing them to the
card.** That is mixing, and `sound_update()` does it every time it refills half
the buffer.

```
   engine 1:  .-'-.  .-'-.  .-'-.
   engine 2: ~~^~~^~~^~~^~~^~~
   shot:           |||||
                 +
   ------------------------------
   what comes out:  the sum of all three
```

## 2. Clipping, and what it sounds like

Each sample fits in a byte. Add three loud sounds and the sum **goes out of
range**.

When that happens the mixer has to **clip**: leave it at the maximum. And
clipping sounds like **dirty distortion**, like a broken speaker.

**Press `5`** in the program, turn on both engines and fire. That is clipping.

## 3. Which is why the game's volumes are low

```c
	set_sound_volume(fire,    16);
	set_sound_volume(engine1, 12);
	set_sound_volume(engine2, 12);
	set_sound_volume(died,    34);
```

These are the real numbers from `src/main.c`, and they are not arbitrary:

- The **engines** are low (12) because they play **all the time** and there are
  two. If they were loud you would hear nothing else.
- The **shot** (16) has to be heard *over* the engines.
- The **explosion** (34) is a rare, important event: it can afford to be loud.

Mixing well is more about deciding what matters than about programming.

## 4. Play once versus loop

| | |
|---|---|
| `play_sound(id)` | Plays once and ends by itself |
| `loop_sound(id)` | Plays and starts again, **forever** |

Call `play_sound()` again before it finishes and **both copies play overlapped**.
For a gunshot that is correct.

A loop, though, **does not end on its own**. Somebody has to stop it:

```c
	stop_looping_sound(engine1);
```

In the game the engine starts when you press an arrow and stops when you let go.
And there is one case people always forget: **when the tank dies**. If nobody
stops the engine there, the tank keeps roaring while it burns.

## 5. Voices

Internally `sound.c` has a limited number of "voices": slots where something can
be playing. Fill them all and ask for another sound and the new one does not
play.

For this game (two engines, two shots, an explosion, a song) there are plenty.
But if you write a game with many simultaneous effects, that is the first thing
to look at.

## 6. Experiments

1. **Press `5` and fire with both engines.** Listen to the clipping.
2. **Set every volume to 4.** Clean but inaudible. The balance is between those
   two extremes.
3. **Turn on an engine and exit with ESC** without stopping it. `sound_end()`
   cuts it, but try removing `sound_end()` and you get chapter 11's lesson.
4. **Fire ten times very fast.** At some point they stop overlapping: you ran out
   of voices.

## 7. What to take away

| | |
|---|---|
| **The card has one channel; you do the mixing** | Adding sample by sample |
| If the sum overflows you must **clip**, and it sounds bad | |
| Low volumes are not shyness: they avoid clipping | |
| `play_sound` ends; `loop_sound` does not | A loop needs someone to stop it |

---

**Previous:** [Chapter 12](../../ch12/doc/README-EN.md) ·
**Next:** [Chapter 14 — Music](../../ch14/doc/README-EN.md)
