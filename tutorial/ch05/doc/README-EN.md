# Chapter 5 — The keyboard, properly

*[Versión en español](README.md)*

**What you will get:** a tank you drive with the arrow keys, using the same
keyboard system the game uses.

**Which real code is used:** `src/bmp.c`, `src/players.c`, `header/players.h`.

```
make
chap05
```

---

## 1. Why `getch()` does not work

It is the first thing anyone tries, and it fails for three separate reasons:

**It blocks.** `getch()` sits there until you press something. A game cannot
stop: it has to keep drawing, moving bullets and making sound even when you
touch nothing.

**It does not know whether you are still holding.** It tells you "the A key was
pressed". It does not tell you "A is held down right now". For the tank to keep
moving while you hold the arrow, you need the second.

**One key at a time.** If player 1 uses the arrows and player 2 uses WASD and
both move at once, `getch()` cannot cope.

## 2. What a scancode is

The keyboard does not send letters. It sends the **number of the physical key**.

| Key | Scancode |
|---|---|
| Up arrow | 0x48 |
| Down arrow | 0x50 |
| Left arrow | 0x4B |
| Right arrow | 0x4D |
| ESC | 0x01 |
| W | 0x11 |

That distinction matters: the key to the left of the 1 is always the same
scancode, whatever is printed on it in your country. A game wants positions, not
letters.

## 3. The bit 7 trick

Every time you touch a key, the keyboard sends **one** scancode. And another one
when you release it, with bit 7 set:

```
   Press the up arrow    ->  0x48         (72)
   Release it            ->  0x48 + 128   (200)
```

```c
	if (scancode & 0x80){
		keys[scancode - 128] = 0;   /* released */
	}else{
		keys[scancode] = 1;         /* pressed */
	}
```

**That is the whole thing.** With an array of 128 slots, each key turns its own
on when pressed and off when released, **independently of the others**. Reading
several at once comes for free.

## 4. Interrupt 9

So who reads the scancode? An **interrupt**.

When you touch a key the hardware interrupts the processor: it stops whatever it
was doing, jumps to a function, and when that function ends it returns exactly
where it was. Your program never notices.

The keyboard's is **INT 9**. DOS has its own installed (the one that makes
`getch` work), and we put ours in its place:

```c
static void install_kbd(){
	old_kbd_handler = getvect(IRQ_KEYBOARD);   /* save the DOS one */
	setvect(IRQ_KEYBOARD, new_kbd_handler);    /* install ours */
}
```

Three details that are not optional:

**`interrupt far`.** Tells Turbo C this function is a handler: it must save every
register on entry, restore them on exit, and return with `IRET` instead of
`RET`. Without it, the interrupted program finds its registers changed.

**`volatile` on `keys[]`.** That array is modified by something the compiler
cannot see. Without `volatile` it may decide nobody changes it and keep the value
in a register, and your loop would always read the same thing.

**`outp(0x20, 0x20)` at the end.** Tells the interrupt controller you are done.
Forget it and **no further interrupt ever arrives**: the keyboard goes dead.

## 5. Put it back on exit, or you hang the machine

```c
static void uninstall_kbd(){
	setvect(IRQ_KEYBOARD, old_kbd_handler);
}
```

This is not optional tidying. When your program ends, DOS frees its memory, but
INT 9 **still points at your function**, which no longer exists.

The next key anyone presses jumps into memory that is now something else. The
machine hangs.

That is why the game exits with ESC and not by closing the window: so it gets a
chance to restore the vector.

## 6. The main loop

This is the skeleton of any game, and you already have all of it:

```c
	do {
		/* 1. read input     */
		/* 2. update         */
		/* 3. draw           */
		/* 4. wait and blit  */
	} while (!keys[KEY_ESC]);
```

It spins non-stop. Each time round it **looks** at the keyboard, it does not wait
for anything. That is why the tank keeps moving while you hold the arrow.

## 7. Why you cannot go diagonally

```c
		if (keys[KEY_UP]){
			...
		}else if (keys[KEY_DOWN]){
```

`if / else if`, not four loose `if`s. With two arrows held, only the first gets
through.

**That is a design decision, not a limitation.** A tank that moves in eight
directions needs eight sets of sprites and a different collision system. With
four, everything is simpler and the game controls better.

Drop the `else`s and rebuild: the tank goes diagonally and you will see the
sprite does not follow — it faces one way and moves another.

## 8. Experiments

1. **Drop the `else`s.** Diagonals, and a mismatched sprite.
2. **Print the scancode** of whatever you press. In text mode, before going into
   graphics: `while(1){ if(kbhit()) printf("%02X ", getch()); }`. That is how you
   get the scancodes of any keys you want.
3. **Comment out `outp(0x20, 0x20)`.** The keyboard answers once and dies.
4. **Comment out `uninstall_kbd()`.** Exit the program and press a key in DOS.
   (Do it in DOSBox, not on a machine with unsaved work.)
5. **Raise the step from 2 to 6.** Faster, and you can see the movement is
   stepped. That number is the game's `PIXEL_TO_MOVE`.

## 9. What to take away

| | |
|---|---|
| `getch()` blocks and gives no held keys | Which is why it will not do |
| The keyboard sends **scancodes**, not letters | Physical position, not character |
| **Bit 7 tells press from release** | That is where simultaneous keys come from |
| The handler needs `interrupt far`, `volatile` and the `outp(0x20,0x20)` | All three or nothing |
| **Restore the vector on exit** | Or you hang the machine |

---

**Previous:** [Chapter 4](../../ch04/doc/README-EN.md) ·
**Next:** [Chapter 6 — Animation](../../ch06/doc/README-EN.md)
