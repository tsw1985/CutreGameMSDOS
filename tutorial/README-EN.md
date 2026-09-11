# Course: from a black screen to two tanks fighting over a network

*[Versión en español](README.md)*

One chapter per idea. Each one is a program that compiles, runs and does a
single thing, and each one adds **one** piece to the last.

---

## How it works

Each chapter is a folder:

```
tutorial/
   ch01/
      chap01.c          <- the lesson. Short, fully commented
      Makefile          <- make, and chap01.exe lands right here
      doc/
         README.md      <- the full explanation (Spanish)
         README-EN.md   <- this, in English
   ch02/
      ...
```

From DOS, one chapter at a time:

```
cd tutorial\ch01
make
chap01
```

Or **all of them at once**, from `tutorial\`:

```
makeall           build all 23
makeall 07        build only 07
cleanall          delete every .exe and .obj
```

`make` and `make clean` work too, and call those same `.bat` files.

> **Why a `.bat` and not everything inside the Makefile.** You have to step into
> each folder, and a `cd` launched from MAKE runs in a child `COMMAND.COM` that
> dies at the end of the line: MAKE stays where it was. In a batch file the `cd`
> does persist.
>
> And that is why the Makefile says `command /c makeall.bat` and not
> `makeall.bat` on its own: a `.bat` is not an executable, it is a script you
> have to hand to the interpreter. Called directly, MAKE answers *"bad command
> or file name"*.

Each `chapNN.exe` stays **in its own chapter's folder**, not in a shared `bin`,
so you can step in, run it and study it on its own.

## The rule of this course: **there is no duplicated code**

This is not a toy game written on the side. Each chapter **links against the
real files** in `src/` and includes the real headers from `header/`:

```make
$(CC) $(CFLAGS) -echap02.exe chap02.c ..\..\src\bmp.c
```

What lives in `chapNN.c` is **only the lesson**: the `main()` that calls the
game's functions in the right order. Everything else is the real code.

That has three good consequences:

- **The course does not go stale.** Fix a bug in `bmp.c` and every chapter is
  fixed with it.
- **You are reading the code that actually runs**, not a simplified teaching
  version that later resembles nothing.
- **The `diff` between chapters is the lesson.** Try this:

  ```
  diff tutorial/ch04/chap04.c tutorial/ch05/chap05.c
  ```

  That tells you exactly what has to be added to go from "a sprite standing
  still" to "a sprite you drive with the keyboard". No noise.

**The one exception**, flagged wherever it appears: `set_text_mode()` and
`wait_retrace()` live inside `src/main.c`, which has its own `main()`, so they
cannot be linked against. Eight lines in total, kept in `tutorial/tutlib.h`,
one single copy for the whole course.

## Resources

Chapters use the game's own BMPs and WAVs, without copying them:

```c
bmp_fill_background_in_main_buffer("..\\..\\res\\cutre.bmp");
```

From `tutorial\ch01\`, that `..\..\res\` is the project's `res\` folder.

---

## The syllabus

### Block 1 — Graphics

| | Chapter | The idea |
|---|---|---|
| ✅ | [**ch01** — Mode 13h and video memory](ch01/doc/README-EN.md) | The screen is memory. The byte is an index, not a colour |
| ✅ | [**ch02** — Loading a BMP](ch02/doc/README-EN.md) | Pixels and palette are two separate loads. BMPs are upside down |
| ✅ | [**ch03** — Double buffer and retrace](ch03/doc/README-EN.md) | Why it flickers, why it tears, and how to fix both |
| ✅ | [**ch04** — Sprites](ch04/doc/README-EN.md) | Cutting, transparency, and the clipping that stops memory corruption |
| ✅ | [**ch05** — The keyboard](ch05/doc/README-EN.md) | Scancodes, INT 9, and several keys at once |

### Block 2 — The game

| | Chapter | The idea |
|---|---|---|
| ✅ | [**ch06** — Animation](ch06/doc/README-EN.md) | Two track frames and a speed counter |
| ✅ | [**ch07** — Collisions against the map](ch07/doc/README-EN.md) | The collision BMP, and why the picture lies |
| ✅ | [**ch08** — Two players](ch08/doc/README-EN.md) | Separating "the keys" from "where the keys come from" |
| ✅ | [**ch09** — Bullets](ch09/doc/README-EN.md) | Firing, flying, hitting. The edge detector |
| ✅ | [**ch10** — Hit, explosion and round](ch10/doc/README-EN.md) | The pause, the score, the restart |

### Block 3 — Sound

| | Chapter | The idea |
|---|---|---|
| ✅ | [**ch11** — Finding the Sound Blaster](ch11/doc/README-EN.md) | The BLASTER variable, the DSP, the reset |
| ✅ | [**ch12** — A WAV over DMA](ch12/doc/README-EN.md) | The card reads memory on its own. `sound_update()` |
| ✅ | [**ch13** — Mixing](ch13/doc/README-EN.md) | Several sounds at once, volume, and clipping |
| ✅ | [**ch14** — Music](ch14/doc/README-EN.md) | Streaming: 2.7 MB on a 640 KB machine |

### Block 4 — Network

| | Chapter | The idea |
|---|---|---|
| ✅ | [**ch15** — Finding IPX](ch15/doc/README-EN.md) | INT 2F, the far call, opening a socket |
| ✅ | [**ch16** — Pairing two machines](ch16/doc/README-EN.md) | Broadcast, HELLO and ACK. Nobody types an address |
| ✅ | [**ch17** — Sending and receiving](ch17/doc/README-EN.md) | A complete chat. Datagrams, not a stream |
| ✅ | [**ch18** — Lockstep](ch18/doc/README-EN.md) | Why positions are not sent. Determinism and input delay |
| ✅ | [**ch19** — The checksum](ch19/doc/README-EN.md) | Desyncs. Press D and cause one |

### Block 5 — The big map

| | Chapter | The idea |
|---|---|---|
| ✅ | [**ch20** — The world stops being the screen](ch20/doc/README-EN.md) | The problem **before** the solution: you lose the tank |
| ✅ | [**ch21** — The window](ch21/doc/README-EN.md) | The *stride*, and why a `memcpy` stops working. **You** move the window |
| ✅ | [**ch22** — The camera](ch22/doc/README-EN.md) | Making the window follow the tank. The dead zone |
| ✅ | [**ch23** — 1-bit mask and memory](ch23/doc/README-EN.md) | Fragmentation: the expensive lesson |

---

## If you already know some of it

- Only interested in the **camera**? Chapters 1, 4, 20, 21, 22 and 23. Plus the
  [full manual](../doc/EN/CAMERA-MANUAL.md).
- Only the **network**? 15 to 19, plus the [network manual](../doc/EN/NETWORK-MANUAL.md).
- Only the **sound**? 11 to 14, plus the [sound tutorial](../doc/EN/SOUND-TUTORIAL.md).

The manuals under `doc/` are the deep reference. This course is the way up.

## What you need

- Borland Turbo C++ 3.0
- DOSBox, or a real DOS machine
- For chapters 11 to 14, a Sound Blaster (or the one DOSBox emulates)
- For chapters 15 to 19, two instances: use `launch_game_both.sh`

No chapter needs NASM.
