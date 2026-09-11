# Chapter 17 — Sending and receiving: a chat

*[Versión en español](README.md)*

**What you will get:** typing on one machine and reading it on the other.

```
./launch_game_both.sh
```
and in both: `cd tutorial\ch17` and `chap17`.

---

## 1. Messages, not a stream of bytes

This is the big difference from TCP, and it is worth being clear about.

**In TCP** you send 10 bytes and then 5, and the other end may read 15 at once, or
3 and then 12. TCP gives you a **stream**: the bytes arrive in order but the
boundaries between your sends **disappear**. You have to invent your own way of
saying where each message ends.

**Not here.** One `net_send()` is one `net_receive()`:

```c
	length = net_receive(incoming, NET_MAX_DATA);
```

Returns **one complete message**, or 0 if there is nothing. Never half a message,
never two stuck together.

That is called a **datagram**, and it saves you all the delimiting work.

## 2. `net_update()` is mandatory

```c
		net_update();
```

`net.c` leaves **four mailboxes** with the driver. When a packet arrives, the
driver puts it in one and marks it full.

`net_update()` is what goes and checks those four mailboxes, takes whatever is
there, and **puts them back empty and ready**.

If you do not call it, all four fill up and from then on **everything that
arrives is lost**. The driver keeps nothing of its own.

It is called *polling*: **the initiative is yours**. IPX offers the opposite (it
notifies you with an interrupt, the `esr_address`) and `net.c` **deliberately
does not use it**, because that routine would fire in the middle of whatever the
game was doing. Same decision as with the sound: the interrupt marks, the loop
works.

## 3. The drain loop

```c
		length = net_receive(incoming, NET_MAX_DATA);

		while (length > 0){
			...
			length = net_receive(incoming, NET_MAX_DATA);
		}
```

Several messages may have arrived in a single pass. You have to take them all out
or they pile up.

## 4. Sending is not arriving

```c
	if (net_send(line, position) == 1){ ... }
```

That `1` means **"I handed it to the driver"**. It does not mean it arrives.

IPX guarantees nothing. The packet can be lost and nobody tells you.

For a chat that is a real problem. **For a game it is not**: better to lose a
packet and carry on than to stall the match resending it. Chapter 18 explains how
that loss is absorbed without resending anything.

## 5. It is not just for text

```c
	net_send(line, position);
```

`net_send()` sends **bytes**: up to `NET_MAX_DATA` at a time. They can be:

- Text, as here
- A `struct` (`net_send(&my_struct, sizeof(my_struct))`)
- Three bytes that mean something only to you
- A chunk of a file, if you want to write a "send file"

Chapter 18 sends exactly that: a 20-byte `struct` with tank keys.

## 6. Experiments

1. **Send a very long line.** It gets cut at `NET_MAX_DATA`.
2. **Remove the `net_update()`.** You receive four messages and then nothing ever
   again. That is the bug that teaches you what it is for.
3. **Remove the `while` loop** and leave a single `net_receive()`. Type fast on
   the other machine and they pile up.
4. **Send a struct** instead of text. Define one with three `int`s, fill it and
   send it with `sizeof`.

## 7. What to take away

| | |
|---|---|
| **One `net_send()` = one `net_receive()`** | Datagrams, not a stream |
| **`net_update()` every time round** | Or the four mailboxes fill and everything is lost |
| Drain the queue with a `while` | Several may arrive |
| Sending is not arriving | And for a game that is fine |
| It sends bytes, not text | Structs, files, anything |

---

**Previous:** [Chapter 16](../../ch16/doc/README-EN.md) ·
**Next:** [Chapter 18 — Lockstep](../../ch18/doc/README-EN.md)
