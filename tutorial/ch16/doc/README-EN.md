# Chapter 16 — Getting two machines to find each other

*[Versión en español](README.md)*

**What you will get:** two machines paired **without typing any address**.

```
./launch_game_both.sh
```
and in both windows: `cd tutorial\ch16` and `chap16`.

---

## 1. The problem

In TCP/IP you type an IP. In IPX **there is no IP**: a machine's address is its
card's **MAC**, twelve hexadecimal digits nobody knows by heart and which change
if you change the card.

Making the player type `00:1A:2B:3C:4D:5E` would be horrible.

## 2. The answer: broadcast

There is a special address:

```
   FF:FF:FF:FF:FF:FF
```

It means **"every machine on this network segment"**. Send there and everyone
gets it.

And then the protocol is three steps:

```
   Machine A                          Machine B
      |                                   |
      |----- HELLO to everyone ---------->|
      |                                   |
      |<---- HELLO_ACK (direct) ----------|
      |                                   |
      |  now I know where it is, because  |
      |  every IPX packet carries its     |
      |  sender                           |
```

1. I shout **HELLO** to everyone, four times a second
2. If somebody hears me, they answer **HELLO_ACK**, and that reply carries
   **their address**
3. Now I know where they are. From then on I talk only to them

**Nobody typed anything.** That is the goal.

## 3. Three doors to the same mechanism

```c
	net_wait_for_client(s);     /* listens and does not shout  -> "server" */
	net_connect_to_server(s);   /* shouts until answered -> "client" */
	net_find_peer(s);           /* shouts AND listens -> two equals */
```

All three use **the same loop** underneath.

And there is something important there: **"server" and "client" do not exist in
IPX**. They are a convention built on top. In IPX both machines are exactly
equal, and the only difference is who shouts and who listens.

For a two-player game, `net_find_peer()` is the comfortable one: start both in
any order and they find each other.

## 4. Who is player 1, without spending a packet

Somebody has to drive the blue tank and somebody the red one, and **both machines
have to agree**.

You could negotiate: *"I want to be 1"*, *"fine, I will be 2"*… Packets, waits,
and an awkward case if both ask for the same thing at once.

It is done far more simply. Each machine drew a random number at startup, and
after pairing **each one knows both numbers**. So both apply the same rule:

```c
	if (net_get_local_id() < net_get_remote_id()){
		/* I am player 1 */
	}
```

> The one with the smaller number is player 1.

Both do the same arithmetic on the same data, so **they reach the same
conclusion**. Zero packets, zero waiting, zero awkward cases.

## 5. And that is lockstep in miniature

That trick — **having both sides compute the same thing rather than ask each
other** — is exactly the idea of chapter 18, applied to a small decision.

If it convinces you here, lockstep will feel natural.

## 6. A detail that matters

`net.c` answers a HELLO **always**, not only while pairing. That covers two
cases: a lost ACK, and the other machine starting later.

## 7. Experiments

1. **Start them in either order.** It does not matter: `net_find_peer()` shouts
   and listens at the same time.
2. **Start only one.** After 30 seconds it gives up.
3. **Look at both ids** on both screens. Confirm the smaller one declares itself
   player 1 and the larger one player 2, without ever negotiating.
4. **Start three copies.** Two pair up and the third is left out.

## 8. What to take away

| | |
|---|---|
| No IP to type: they find each other | |
| **Broadcast to FF:FF:FF:FF:FF:FF** | HELLO / HELLO_ACK |
| Server and client are a convention, not IPX | Both machines are equal |
| **The role is computed, not negotiated** | By comparing two ids |

---

**Previous:** [Chapter 15](../../ch15/doc/README-EN.md) ·
**Next:** [Chapter 17 — A chat](../../ch17/doc/README-EN.md)
