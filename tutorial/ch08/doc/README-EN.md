# Chapter 8 — Two players, and the idea that holds up the course

*[Versión en español](README.md)*

**What you will get:** two tanks moving at once on the same screen.

**And above all:** the abstraction that will let chapter 18 add networking
**without touching a single line** of movement, collisions or drawing.

```
make
chap08
```

Player 1: arrows. Player 2: W A S D.

---

## 1. From here on, `tutlib.h`

What you already learned (cutting sprites, the keyboard, animation, look before
you leap) has moved into `tutorial/tutlib.h`, **one single copy for the whole
course**, so each chapter contains only the new idea.

It is all commented, with the chapter number where each piece is explained. Open
it once and forget it.

## 2. Two tanks are almost free

```c
	player_init(&player1);
	player_init(&player2);
	...
	process_player_input(&player1, input1);
	process_player_input(&player2, input2);
```

The same calls, one per player. **That is what chapter 6 bought** by putting a
tank's whole state in a `struct`.

If position, direction, bullet and the rest were loose variables (`tank_x`,
`tank_y`, `tank_dir`…), each new player would be another batch of variables **and
a copy of every function**. With the struct, it is a parameter.

The `0` and `21` in `tut_load_tank_sprites()` are the sheet row: the blue tank is
on top and the red one right below.

## 3. THE BIG IDEA OF THE COURSE

Look at these two functions, and above all at what they do **not** do:

```c
static unsigned char read_input_from_keys(...){

	bits = 0;
	if (keys[key_up]){    bits = bits | 0x01; }
	if (keys[key_down]){  bits = bits | 0x02; }
	...
	return bits;

}


static void process_player_input(struct player *p, unsigned char input_bits){

	if (input_bits & 0x01){ ... }
	else if (input_bits & 0x02){ ... }

}
```

The first reads the keyboard and returns **one byte with five bits**.

The second takes that byte and **never mentions the keyboard**. There is not a
single `keys[]` inside it. It does not know a keyboard exists.

| bit | value | means |
|---|---|---|
| 0 | `0x01` | up |
| 1 | `0x02` | down |
| 2 | `0x04` | left |
| 3 | `0x08` | right |
| 4 | `0x10` | fire |

### Why not read `keys[]` directly

It would be shorter. You could write `if (keys[KEY_UP])` inside the movement and
save yourself a function and a parameter.

**And it would be the decision that stopped you adding networking later.**

That byte is the **boundary** between *"where the orders come from"* and *"what
is done with them"*. Everything below the boundary — movement, collisions,
animation, firing, drawing — stops knowing where anything came from.

## 4. The proof: the diff with chapter 18

Do not take my word for it. Chapter 18 is the networked game, and this is the
**actual difference** in `process_player_input()` between the two:

```
$ diff ch08/chap08.c ch18/chap18.c   (that function only)

<	if (input_bits & 0x01){            >	if (input_bits & NET_INPUT_UP){
<	}else if (input_bits & 0x02){      >	}else if (input_bits & NET_INPUT_DOWN){
<	}else if (input_bits & 0x04){      >	}else if (input_bits & NET_INPUT_LEFT){
<	}else if (input_bits & 0x08){      >	}else if (input_bits & NET_INPUT_RIGHT){
```

**Four lines, and they are purely cosmetic.** `NET_INPUT_UP` is defined in
`header/lockstep.h` as… `0x01`:

```c
#define NET_INPUT_UP		0x01
#define NET_INPUT_DOWN		0x02
#define NET_INPUT_LEFT		0x04
#define NET_INPUT_RIGHT		0x08
#define NET_INPUT_FIRE		0x10
```

**The same values.** Chapter 18 uses the names rather than the numbers because
those are the bits that travel down the wire, and it reads better. But
functionally `process_player_input()` is **the same function byte for byte**.

And `tut_try_move()`, `tut_update_animation()`, `tut_pick_sprite()`,
`bmp_is_wall()`, the drawing: **identical**. Zero changes.

The only thing that changes in ch18 is where `input_bits` comes from:

```c
	/* chapter 8 */
	input1 = read_input_from_keys(KEY_UP, KEY_DOWN, ...);

	/* chapter 18 */
	player1_input = net_get_remote_input();     /* off the wire */
```

Check it yourself when you get there:

```
diff tutorial/ch08/chap08.c tutorial/ch18/chap18.c
```

## 5. And this is not a trick specific to this game

It is the general pattern, and it has a name: **separating the input source from
the logic**.

With that boundary in place, the bits can come from:

| | |
|---|---|
| The keyboard | Chapter 8 |
| **A network cable** | Chapter 18 |
| A recorded file | Replays |
| An AI | A computer-controlled player |
| A gamepad | A new device |

**And none of those requires touching the game.** Replays are especially neat:
save each frame's byte to a file and you have a recording of the whole match,
because the game is deterministic (chapter 18).

> If you write a game and think you might ever want networking, replays or AI,
> **make this separation from the start**. It is the only thing you have to plan
> for, and after that it is free.

## 6. Why each player needs its own chain

```c
	if (input_bits & 0x01){ ... }
	else if (input_bits & 0x02){ ... }
```

The `if / else if` is what prevents diagonals: **only one direction gets through
per frame**, the first one found.

And that is why **each player calls the function separately**. If they shared a
single chain, player 1's `else` would eat player 2's turn and only one of them
would move per frame.

It is a subtle bug, because the game *nearly* works: the second player just looks
jerky.

## 7. Simultaneous keys were already solved

Having both players move at once needs nothing new here. Chapter 5's keyboard
handler solved it, with the 128-slot array where each key turns its own on when
pressed and off when released.

Here we only collect what was already there.

## 8. Why a byte of bits and not five variables

You could return a struct with five `int`s. It would work. But:

- **A byte is what gets sent over the network.** Chapter 18 sends literally that
  byte, one per frame per player. Five integers would be 10 bytes instead of 1.
- **It fits in the packet whole**, and chapter 18 sends eight frames of past
  input for redundancy: 8 bytes, not 80.
- And **testing a bit is an AND**, which is as cheap as it gets.

Choosing the byte was not about elegance: it was because ten chapters later it
has to go into a packet.

## 9. Experiments

1. **Merge the two calls** into one shared `if / else if` chain. The second
   player moves jerkily.
2. **Print `input1` in binary** on exit. Confirm that holding two keys lights two
   bits, even though the tank only uses one.
3. **Drop the `else`s.** Diagonals, with the sprite facing elsewhere.
4. **Give both players the same keys.** They move like a mirror, which shows
   `process_player_input()` cannot tell who is who.
5. **Write a `read_input_from_script()`** that returns a fixed sequence of bytes
   (up for 30 frames, right for 30…) and feed it to player 2. You have just
   written a crude AI **without touching the game**. That is the boundary
   working.

## 10. What to take away

| | |
|---|---|
| Two players = two `struct`s | Almost no extra code |
| **The keys become a byte of bits** | That is the boundary |
| What comes after **does not know a keyboard exists** | |
| **The diff with ch18 is 4 cosmetic lines** | And `NET_INPUT_UP` is `0x01` |
| With that boundary: network, replays, AI, gamepads | All free |
| Each player, its own `if/else if` chain | Or one of them goes jerky |

---

**Previous:** [Chapter 7](../../ch07/doc/README-EN.md) ·
**Next:** [Chapter 9 — Bullets](../../ch09/doc/README-EN.md)
