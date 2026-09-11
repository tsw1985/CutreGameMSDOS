# Chapter 19 — The checksum: catching a desync

*[Versión en español](README.md)*

**What you will get:** causing a desync on purpose with the `D` key and watching
it get detected.

```
./play.sh both
```
and in both: `cd tutorial\ch19` and `chap19`.

---

## 1. The nastiest bug there is

Chapter 18 works **as long as** both machines do the same arithmetic.

What if one computes something different? That is a **desync**. And the worst
part is not that it happens: it is **that you cannot see it**.

- Both matches keep running
- **Neither gives any error**
- Each is convinced it is fine
- They simply **tell different stories**

The player sees the other tank fire into empty space, bump into nothing, die for
no reason. And there is not one message anywhere.

Compare it with a normal bug: read a null pointer and you crash, and you know
where. A desync **does not crash**. It carries on.

## 2. Why you cannot compare the whole state

The obvious idea would be: *"have each machine send its full state and compare
them"*.

That does not work, for two reasons:

- The match state is **hundreds of bytes** (two tanks with positions, directions,
  bullets, animations, scores). Sending it every frame is exactly what lockstep
  avoids.
- And if you did send it, **you would no longer need lockstep**: you would be
  sending positions, which is what chapter 18 ruled out.

What you want is to **detect** that there is a difference, not to **transmit** the
state. And to detect, a summary is enough.

## 3. What a checksum is

A **checksum** is a small number computed from a lot of data, such that:

> If the data is the same, the number comes out the same.
> If the data changes, the number changes (almost certainly).

It is a one-way summary: from the state you get the number, but from the number
you cannot rebuild the state. And you do not need to: **you only want to compare
it**.

Here the summary fits in **2 bytes** and represents the entire match state.

## 4. How it is generated

```c
static unsigned int compute_state_checksum(){

	unsigned int checksum;

	checksum = 0;

	checksum = checksum + (player1.position_x * 3);
	checksum = checksum + (player1.position_y * 5);
	checksum = checksum + (player1.current_direction * 7);

	checksum = checksum + (player2.position_x * 31);
	checksum = checksum + (player2.position_y * 37);
	checksum = checksum + (player2.current_direction * 41);

	return checksum;

}
```

It takes each piece of state, multiplies it by a number, and adds it all up.

### Why multiply: so position matters

If you added them raw:

```c
	checksum = player1.position_x + player1.position_y + ...;
```

then a tank at **(100, 50)** and one at **(50, 100)** would give **the same
number**. Two completely different states, same summary. The checksum would see
nothing.

Multiplying each field by a different constant makes each one contribute
differently, and the swap shows:

| | Raw sum | With multipliers |
|---|---|---|
| Tank at (100, 50) | 150 | 100·3 + 50·5 = **550** |
| Tank at (50, 100) | 150 | 50·3 + 100·5 = **650** |
| Distinguished? | **No** | **Yes** |

### Why primes

You could use 2, 4, 6, 8… but multiples of each other create **cancellations**:
with multipliers 2 and 4, a +2 change in the first field cancels a −1 change in
the second.

Primes (3, 5, 7, 11, 31, 37, 41…) share no factors, so accidental cancellations
are far rarer.

This is not cryptography: it is a cheap trick that works more than well enough.

### Overflow does not matter

`unsigned int` is 16 bits. Multiply positions by 41, add six fields, and you go
past 65,535 immediately.

**It does not matter.** The overflow is **also deterministic**: both machines do
exactly the same and reach the same number. All you lose is that two different
states could give the same summary by chance, and with 65,536 possible values
that is rare and, above all, **would be caught at the next check**.

## 5. What goes in and, above all, what does NOT

**In:** everything both machines have to compute identically. Positions,
directions, bullets, score, the pause counter.

**Not in, and just as important:** anything that is local to each machine.

In chapter 22 `camera_x` and `camera_y` will appear. They are **legitimately
different** on each side, because each machine follows its own tank. Putting them
here would give a **false** desync on frame 1 of every match.

> **The rule:** if a value is computed by the drawing code, it does not go in the
> checksum.

Same with `local_player_is_1`: it is 1 on one machine and 0 on the other, **by
design**.

## 6. The game's tripwire

`src/main.c` also puts in two things that are **not state**:

```c
	checksum = checksum + ((unsigned int)map_width * 73);
	checksum = checksum + ((unsigned int)map_height * 79);
```

`map_width` never changes during a match. What is it doing there?

It is a **deliberate tripwire**. Start one machine with `/bigmap` and the other
without, and the maps are different sizes, the walls are in different places, and
the two simulations come apart in a way that is impossible to diagnose.

Putting it in the checksum turns that case into **one clear line in the log** at
the first check, instead of two screens diverging with no explanation.

Cost: two additions every 30 frames. One of the best cost/benefit ratios in the
project.

## 7. How it travels: free

```c
	net_set_local_checksum(compute_state_checksum());
```

`lockstep.c` puts it **inside the same packet as the keys**:

```c
struct lockstep_message {
	unsigned long  base_frame;
	unsigned long  checksum_frame;
	unsigned int   checksum_value;
	unsigned char  count;
	unsigned char  has_checksum;
	unsigned char  inputs[NET_REDUNDANCY];
};
```

And here is the neat bit: **that packet already existed**. The IPX header is **42
bytes** and this message is **20**. Sending the checksum costs **neither an extra
packet nor a frame of latency**: it rides in a gap you were already paying for.

Which is why it can afford to be sent often.

## 8. Not every frame: every 30

```c
	checksum_countdown = NET_CHECKSUM_INTERVAL;   /* 30 */
```

There is no need to check every frame. A desync **does not heal itself**: once
the two matches separate, they stay separate. So it makes no difference whether
you find out on frame 100 or frame 130.

Checking every 30 frames (under half a second) gives a precise enough diagnosis
and only occupies the packet field now and then.

## 9. The subtle detail: they are not compared on the spot

Here is something that is not obvious and that a lot of people implement wrong.

You compute the checksum for frame 100. You send it. **It arrives at the other
machine several frames later**, because the packet takes time and because chapter
18's input delay shifts everything.

By the time it arrives, the other machine is **already on frame 104**. It cannot
compare it with "its current checksum": it has to compare it with **the one it
computed itself on frame 100**.

That is why `lockstep.c` keeps its own in a ring buffer:

```c
static unsigned long local_checksum_frame[NET_INPUT_BUFFER_SIZE];
static unsigned int  local_checksum_value[NET_INPUT_BUFFER_SIZE];
static unsigned char local_checksum_valid[NET_INPUT_BUFFER_SIZE];
```

And when one arrives from outside:

```c
static void net_check_remote_checksum(unsigned long frame, unsigned int value){

	/* too old: no longer kept */
	if (frame < simulation_frame){ ... }

	/* too new: impossible */
	if (frame >= simulation_frame + NET_INPUT_BUFFER_SIZE){ ... }

	/* the ring slot is not for that frame */
	if (local_checksum_frame[index] != frame){ ... }

	if (local_checksum_value[index] == value){
		return;                     /* they match: all good */
	}

	/* they do NOT match */
	sprintf(lockstep_log_text, "NET DESYNC at frame %lu: mine %u theirs %u",
	        frame, local_checksum_value[index], value);
	tanks_log(lockstep_log_text);

	desync_detected = 1;

}
```

Note the three checks before comparing. **Comparing checksums from two different
frames would always give a false desync**, because the state changes every frame.
The frame number travels with the checksum precisely so they can be paired up.

## 10. What comes out in the log

```
NET DESYNC at frame 412: mine 51230 theirs 51237
```

Three pieces of information, and all three matter:

| | |
|---|---|
| `frame 412` | **Where to look.** The bug is on that frame or just before |
| `mine` / `theirs` | Confirm there is a real difference, not a corrupt packet |

And it is written **once only**: `desync_detected` stays at 1 and never warns
again. Otherwise you would get thousands of identical lines, because once
separated the matches never agree again.

## 11. Press `D`

```c
		if (keys[KEY_D] && desync_forced == 0){
			player1.position_x = player1.position_x + 1;
			desync_forced = 1;
		}
```

Moves the tank **one pixel**, on this machine only. One.

The other machine never finds out and keeps computing without it. From that frame
on the two matches are different and **you still cannot see it**: one pixel does
not show.

But the checksum catches it at the next check, at most 30 frames later.

**That is exactly what it is for:** turning an invisible bug into a line in the
log.

## 12. It fixes nothing, and that is fine

The checksum **does not repair** the desync. Both matches stay apart.

Games that do fix it (by sending the full state to resynchronise) need a lot more
machinery, and for a two-tank game it is not worth it.

Here the checksum does one thing: **tell you the exact frame it broke on**. And
that is all you need, because that is where the bug is.

Without it you would search blind across the whole match, on two machines, not
even knowing whether the problem was yours or the network's.

## 13. Typical causes of a desync

For when it happens for real, in order of likelihood:

1. **Something that depends on real time** rather than the frame number. Number
   one by a distance.
2. **A random number** without a shared seed.
3. **Uninitialised memory** that happens to hold different things on each machine.
4. **The two machines loaded different data**: a different map, a different file.
   That is what the tripwire in point 6 is for.
5. **Applying your own input before the other player's**, skipping the delay. It
   feels better and desyncs within the first second.

## 14. Experiments

1. **Press `D` on one machine only.** Warning on exit, and a line in the log.
2. **Press it on BOTH at once.** Both apply the same +1 → **no desync**. That
   shows what matters is the *difference*, not the change.
3. **Remove the multipliers** and add raw. Swap the two tanks' positions by hand:
   the checksum does not notice.
4. **Put something local in**, for instance `local_player_is_1`. Desync on frame
   1, every time. That is the bug from point 5.
5. **Lower `NET_CHECKSUM_INTERVAL` to 1** in `header/lockstep.h`. Checks every
   frame: caught sooner, and the packet always carries the field.
6. **Remove the frame number** from the comparison (compare against the current
   checksum without checking which frame it is for). Constant, false desyncs.

## 15. What to take away

| | |
|---|---|
| A desync **cannot be seen**: both matches carry on happily | |
| A **checksum** is a small summary that only exists to be compared | 2 bytes for the whole state |
| **Multiply by primes** so order and position matter | Otherwise (100,50) = (50,100) |
| Overflow does not matter: **it is deterministic too** | |
| **Nothing local inside**: never the camera | It would always be a false positive |
| It travels **free** inside the key packet | The header already cost 42 bytes |
| **Compared by frame number**, not on the spot | The packet arrives late |
| It does not fix; **it tells you the exact frame** | And that is enough |

---

**Previous:** [Chapter 18](../../ch18/doc/README-EN.md) ·
**Next:** [Chapter 20 — The world stops being the screen](../../ch20/doc/README-EN.md)
