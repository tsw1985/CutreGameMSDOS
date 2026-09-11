# Chapter 15 — Finding the IPX driver

*[Versión en español](README.md)*

**What you will get:** knowing whether there is a network and opening a socket.
In text mode.

**Which real code is used:** `src/net.c`.

```
make
chap15
```

---

## 1. What IPX is, and what it is not

In 1995 there was no TCP/IP in DOS as standard. What offices had were **Novell
NetWare** networks, and their protocol was called **IPX**.

IPX **is not TCP/IP**, and the differences matter:

| | IPX |
|---|---|
| Connections | **There are none.** You just send loose packets |
| Guaranteed delivery | **No.** A packet can vanish without warning |
| Guaranteed order | **No.** They can arrive swapped |
| Addresses | The card's MAC. Not typed: discovered |

It sounds worse than TCP/IP. **For a game it is better**: you do not want a lost
packet to stall the match while it is resent. You want to carry on and fix it
another way (chapter 18).

The closest modern equivalent is UDP.

## 2. How the driver is found

IPX is a **TSR**: a program loaded before yours that stays resident in memory.
You have to ask whether it is there.

The convention is **INT 2F**, the "multiplex interrupt": the TSR noticeboard.
Each one has its number and answers if it is present.

```
   AX = 0x7A00   ->   "IPX, are you there?"
```

If it comes back with **AL = 0xFF**, it is. And it leaves the address of its
entry point in **ES:DI**.

## 3. The odd part: a far call, not an interrupt

And here is what gives `net.c` its assembly.

IPX **is not called with an interrupt**. It is called with a **FAR CALL** to that
address, like an ordinary function that happens to live in another segment.

It is the only DOS API that works this way. Everything else (video, disk,
keyboard, DOS itself) goes through interrupts.

Which function you want goes in **BX**:

| BX | Function |
|---|---|
| 0x0000 | Open socket |
| 0x0001 | Close socket |
| 0x0003 | Send |
| 0x0004 | Listen |
| 0x0009 | Tell me my address |
| 0x000A | Yield for a moment |

## 4. What a socket is

A 16-bit number identifying **what a packet is about**.

All sorts of packets go over the same cable: your game's, another game's, the
file server's. **They all reach your card.** The socket is what lets you keep
only yours.

`net.c` uses **0x869C**. Any number from 0x8000 up will do: Novell reserved that
range for unregistered programs.

If another program already holds it, opening fails. That is why `net_start()` can
say *"there may be another copy running"*.

## 5. The local id

```c
	net_get_local_id()
```

A random number `net.c` draws at startup. It is not for addressing anything: it
is for **deciding who is player 1** in chapter 16, without spending a single
packet.

## 6. `net_end()` is not optional

It closes the socket. A socket left open cannot be used by the next program.

## 7. How to try it

**The easy way, from Linux:**

```bash
./play.sh both
```

Brings up two DOSBox windows already connected to each other. In both:
`cd tutorial\ch15` and `chap15`.

**By hand in DOSBox:** `ipx=true` in `dosbox.conf`, and inside:
`ipxnet startserver` in one, `ipxnet connect <ip>` in the other.

**On real DOS:** load `LSL`, your card's ODI driver and `IPXODI` first.

## 8. What to take away

| | |
|---|---|
| IPX has no connections and no guarantees | And for a game that is fine |
| Found with **INT 2F, AX=7A00** | `AL=0xFF` = present |
| **Called with a far call**, not an interrupt | The only case in DOS |
| A **socket** filters the packets that are yours | 0x869C here |

---

**Previous:** [Chapter 14](../../ch14/doc/README-EN.md) ·
**Next:** [Chapter 16 — Finding each other](../../ch16/doc/README-EN.md)
