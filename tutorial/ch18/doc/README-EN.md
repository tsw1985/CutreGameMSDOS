# Chapter 18 — Lockstep: the networked game

*[Versión en español](README.md)*

**The most important chapter of the block.** Here is the idea that makes a game
work over a network, and it is not the one you expect.

```
./play.sh both
```
and in both: `cd tutorial\ch18` and `chap18`. Both are driven with the arrows.

---

## 1. What does not work: sending positions

The first thing anyone thinks of:

> "I send where my tank is, and the other draws it there."

It looks obvious and it makes a bad game:

- The other tank looks **jerky**, because packets do not arrive at a perfect rate
- If a packet is lost, their tank **teleports**
- For two tanks and their bullets you have to send a fair few bytes per frame
- And the bullets move at their own pace, so they need syncing too

You can fix it with interpolation and prediction, and that is how modern games
work. It is a lot of effort.

## 2. What is done: sending keys

You send **which keys you pressed**. One byte. Chapter 8's byte.

And **both machines simulate the whole match**, both tanks, both bullets.

If both do exactly the same arithmetic on the same inputs, they reach the same
result. **There is no need to send positions because both compute them.**

That is called **lockstep**.

| | Positions | Lockstep |
|---|---|---|
| Bytes per frame | Many | **One** |
| If a packet is lost | Teleport | You wait (and ch19 detects it) |
| Requirement | None | **Determinism** |

## 3. Determinism: the precondition for everything

> Same inputs ⟶ same result. **Always.**

In practice that means:

- No random numbers without a shared seed
- Nothing that depends on the clock
- **Nothing that depends on machine speed**

And here is the good news: **this game was already deterministic without trying.**
The tank moves 2 pixels **per frame**, not per millisecond. A slow machine runs
slower in real time but covers exactly the same pixels.

If the game used delta time (movement proportional to elapsed time), none of this
would work, because the two machines would never measure exactly the same.

## 4. Input delay

```c
	net_set_local_input(local_input);
```

That call **does not store the keys for the current frame**. It stores them for
frame **current + NET_INPUT_DELAY**, which is 5 frames. About 70 ms.

Why introduce a delay on purpose?

```
   frame 95:  I send my keys "for frame 100"
   frame 96:  ...in transit...
   frame 97:  ...in transit...
   frame 98:  arrives
   frame 99:
   frame 100: I use them. They had been waiting two frames.
```

**Without the delay**, every frame would have to wait for a packet that **has just
left**, and any hiccup on the network would show as a stutter.

With 5 frames of headroom, the packet is almost always already waiting when you
need it.

⚠️ **And here is a trap that looks like an improvement:** applying *your* keys
instantly and the other player's with a delay. It feels better… and **desyncs
within the first second**, because the two machines would be doing different
arithmetic. Both have to be equally late.

## 5. The price: waiting

```c
		while (net_has_remote_input() == 0){
			net_poll();
			...
		}
```

If the other machine falls behind, **you stop**. Both always sit on the same
frame, never one ahead.

That is where the name *lockstep* comes from: marching in step, tied together.

With 5 frames of delay this wait is almost always zero. When it is not, it shows
as a stutter on **both** machines at once.

Note that the real game calls `sound_update()` **inside this wait**. A network
hiccup must not become a sound hiccup.

## 6. And here chapter 8 pays off

```c
		if (local_player_is_1 == 1){
			player1_input = net_get_local_input();
			player2_input = net_get_remote_input();
		}else{
			player1_input = net_get_remote_input();
			player2_input = net_get_local_input();
		}

		process_player_input(&player1, player1_input);
		process_player_input(&player2, player2_input);
```

**From that line on, the rest of the program is literally chapter 8's.** Compare
them:

```
diff tutorial/ch08/chap08.c tutorial/ch18/chap18.c
```

`process_player_input()`, `tut_try_move()`, the collisions, the animation, the
drawing: **not one line changed**.

That is what chapter 8 sowed by separating *"which keys"* from *"where the keys
come from"*. The bits now arrive over a cable, and nothing underneath cares.

**If you write a game and want to be able to add networking one day, make that
separation from the start.** It is the only thing you have to plan for.

## 7. The networked frame, five steps

```
  1. read my keys
  2. hand them in (they go to current frame + 5) and send them
  3. WAIT for theirs for THIS frame
  4. distribute: mine to my tank, theirs to theirs
  5. simulate and draw, exactly as always
  6. net_advance_frame()
```

The order is sacred. And step 6 only happens once both machines have finished the
frame with the same state.

## 8. Experiments

1. **`diff` with chapter 8.** It is the best way to see what networking adds.
2. **Lower `NET_INPUT_DELAY` to 1** in `header/lockstep.h`. More responsive, and
   any network hiccup shows as a stutter.
3. **Raise it to 20.** Perfectly smooth, and the controls feel mushy.
4. **Apply your input instantly** (bypass the delay system for the local player).
   It desyncs quickly: chapter 19 catches that.

## 9. What to take away

| | |
|---|---|
| **Do not send positions: send keys** | One byte per frame |
| Both machines simulate the whole match | And reach the same result |
| **Determinism is the precondition** | Per frame, not per millisecond |
| Input delay gives the network headroom | And must apply to BOTH |
| The price is waiting | Both march in step |
| **Chapter 8's separation is what makes it possible** | Not one line changed underneath |

---

**Previous:** [Chapter 17](../../ch17/doc/README-EN.md) ·
**Next:** [Chapter 19 — The checksum](../../ch19/doc/README-EN.md)
