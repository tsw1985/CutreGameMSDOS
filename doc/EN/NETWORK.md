# NETWORK.md — how the game's network play works

A document about `src/net.c` and `header/net.h`, and about the changes the
network brought to `src/main.c`. It explains what each function does, why each
decision was taken, and how to set up two machines to try it.

> **This is the REFERENCE, for looking individual things up.**
> If what you want is to *understand* how it works, start with
> [`NETWORK-MANUAL.md`](NETWORK-MANUAL.md), a learning manual that goes from
> nothing, in order. Come back here afterwards.

You play over the network by starting the game like this:

```
game.exe /net
```

With no argument the game is exactly what it always was: two players on the
same keyboard. **None of that was touched.**

---

## Index

1. The problem to start with, and what IPX is
2. The decision underneath it all: lockstep
3. The input delay
4. The redundancy
5. The three layers of `net.c`
6. How the two machines find each other
7. Desync detection
8. Setting up the two machines in DOSBox
9. On real hardware
10. What changed in `main.c`
11. Constants you can change
12. Diagnosis: what to look for in the log
13. **The complete flow, from start to finish**
14. **Reference for every function**

---

## 1. The problem to start with

Turbo C++ 3.0 **has no networking at all**. No sockets, no `<sys/socket.h>`, no
name resolution. Its library is pure DOS: files, `int86()`, ports. And DOS does
not come with a TCP/IP stack either.

In DOS the network is **a driver you load before the game**, and there are
three ways of talking to it:

| Route | What you need | Work |
|---|---|---|
| **IPX** | An IPX driver loaded | None, the driver does it all |
| Packet driver + WATTCP | A packet driver plus linking a TCP/IP stack | A lot, and the stack has to be rebuilt for the HUGE model |
| Packet driver on its own | Write IP and UDP yourself | Reinventing IPX with more work |

**IPX** was chosen, for four reasons:

1. **There is no library to link.** This matters more than it sounds: the game
   is built in the HUGE model (`-mh`), and the prebuilt TCP/IP stacks of the
   era come for LARGE. With IPX nothing is linked, you just make a far call
   into the driver.
2. **No IP address has to be typed anywhere.** IPX has broadcast, so the two
   copies find each other on their own.
3. **DOSBox emulates IPX directly**, so you can develop and test with no
   network cards and no DOS drivers.
4. It is what Doom used.

**None of this code is DOSBox specific.** DOSBox implements the same calls a
real driver does, so the very same `game.exe`, with no rebuild and not one
`#ifdef`, runs on two DOSBoxes talking over UDP or on two real machines with
real cards and `IPXODI` loaded.

### What IPX actually is

**IPX** (*Internetwork Packet eXchange*) is Novell NetWare's network protocol,
from the mid 80s. It was the dominant protocol in DOS office networks for a
decade, before TCP/IP ate it. It is what Doom, Duke Nukem 3D, Warcraft II and
Command & Conquer used.

Set beside what you know today:

| | TCP/IP | IPX |
|---|---|---|
| Addresses | IP (4 bytes) + port | Network (4 bytes) + **node** (6 bytes) + **socket** (2 bytes) |
| The node is | — | **the card's MAC**, with no translation of any kind |
| Names | DNS | there are none, and none are needed |
| Equivalent of UDP | UDP | **plain IPX**, which is what we use |
| Equivalent of TCP | TCP | SPX (we do not use it) |

Three things make it the right choice here:

**1. The address is the MAC.** No IPs to assign, no netmasks to configure, no
DHCP to set up. The card comes with its address from the factory. And there is
broadcast (`FF:FF:FF:FF:FF:FF`), which is what lets the two copies of the game
find each other without anyone typing anything.

**2. There is no stack to link.** This is the decisive one in Turbo C++ 3.0.
With TCP/IP you would have to put WATTCP inside the binary, with its ARP, its
IP, its UDP and its buffer management, and rebuild it for the HUGE model. With
IPX **the driver is already loaded in memory** and you just make a far call.

**3. It is a datagram and nothing more.** IPX does not retry, does not order,
does not guarantee delivery and does no congestion control. That sounds like a
flaw and it is exactly what a game wants: TCP, with its retransmissions and its
Nagle algorithm, would give you hitches of hundreds of milliseconds recovering
a packet that is useless by the time it arrives. What we do with lost packets —
ignore them, because the next one carries the information again — is cheaper
and faster than anything TCP could do for you.

What IPX does **not** give you, and you have to add yourself: reliability (we
solve it with redundancy, section 4), knowing who is on the other end (the
HELLO in section 6), and making the two games agree (lockstep, section 2).

---

## 2. The decision underneath it all: lockstep

This is the important decision in the module, and it has nothing to do with
IPX.

**No position is ever sent.** Not the tank's, not the bullet's, not the score.
**Both** machines run the **whole** game, both tanks included, and the only
thing that travels is **one byte of pressed keys per player per frame**.

This works because the game meets a condition that is not common:

> It is **deterministic**. Everything is integers, there is not one `float`,
> there is no `rand()`, and nothing ever looks at the clock.

If the two machines start in the same state and receive the same sequence of
keys, they compute **exactly the same pixels** until the end of the game. They
cannot diverge.

The loop comes out like this:

```
frame N:  send my keys for frame N + NET_INPUT_DELAY
          wait for the other machine's keys for frame N
          simulate frame N with BOTH
```

What you gain:

- **Ridiculous traffic.** 5 bits per player per frame. The IPX header (30
  bytes) weighs more than everything else put together.
- **The game code does not change.** Collisions, bullets, explosions, drawing
  and restarting a round all stay exactly as they were.
- **The sound needs no network.** Both machines work out the same shot on the
  same frame, so each one calls `sound_play()` by itself, at the same time.
  Nothing about sound is ever sent.

What it costs:

- **If a packet does not arrive, both machines stop.** It is not that one runs
  worse: both wait. That is bought with the input delay and the redundancy,
  which is what the next sections are about.

### The alternative that was not chosen

The other way would be for one machine to be an authoritative server and send
positions. It is worse here: the whole state has to be serialised every frame
(20-30 bytes instead of 1), the client plays with visible lag on its own tank,
and you have to write interpolation code. It only pays off when determinism
cannot be guaranteed, and here it can.

---

## 3. The input delay: the decision you notice most

It lives in `NET_INPUT_DELAY`, and it is **the** constant to reach for if the
game hitches.

Your own keys are **not applied straight away**. They go into slot
`current_frame + NET_INPUT_DELAY` of the buffer and are read back when the game
reaches that frame, which is exactly as long as the other player's take to
arrive.

> **This is THE classic mistake everyone makes with lockstep the first time.**
> The temptation is to apply your own input immediately, because it feels much
> better, and the other player's with a delay. Do that and the two machines are
> simulating different games: **you desync within the first second.**

Both inputs are applied on the same frame on both machines. Always.

With the loop at ~70 Hz, one frame is 14.3 ms:

| `NET_INPUT_DELAY` | Jitter absorbed | Good for |
|---|---|---|
| 3 | ~43 ms | A cable, or two DOSBoxes on the same PC |
| **5** (current) | **~71 ms** | **Ordinary wifi** |
| 7 | ~100 ms | Bad wifi, or over the internet |

What it costs in playability is small: at `PIXEL_TO_MOVE 2`, five frames are
the equivalent of **10 pixels** of delay before you start moving, on a 320 wide
screen with 18 pixel tanks. This is not a fighting game.

---

## 4. The redundancy: why it is free

`NET_REDUNDANCY` is 8. Every packet carries **the last 8 frames of keys**, not
just the current one.

The reason is purely about size: the IPX header is 30 bytes. Sending 1 byte of
useful data or sending 8 costs **the same packet**. So the redundancy is free,
and we are generous with it.

In exchange, **no retransmission, no acknowledgements and no ordering are
needed at all**. A lost or late packet is covered by the next one. For the game
to really stop, **8 packets in a row** would have to fail, which on ordinary
wifi hardly ever happens.

Wifi, besides, does not lose so much as **delay**: it retries at the link
layer, so the packet arrives, late. The redundancy covers both cases equally,
because the next packet carries the frame that was missing.

---

## 5. The three layers of `net.c`

The file is split in three, bottom to top. It knows nothing about tanks: it
moves one byte per frame and compares a number somebody else works out.

### Layer 1 — talking to the IPX driver

**`ipx_detect()`** (line 266)
INT 2F is the DOS "is anybody there?" multiplex. `AX=7A00` asks specifically
for IPX: if it is loaded, AL comes back `0xFF` and **ES:DI** is the address to
call from then on.

It is the documented route and the one Doom used. It works with a real
`IPXODI`, with Novell's client, and with DOSBox, which answers this call
exactly like a real driver would.

**`ipx_call()`** (line 308)
The call into the driver: `BX` = what to do, `ES:SI` = the ECB.

It has to be assembler because IPX wants its arguments **in registers** and is
entered with a **far call**, and neither of those can be expressed in C.

The awkward part is calling an address held in a variable: there is no "far
call to this register pair" instruction. So the address is pushed onto the
stack and called from there, which is what this dance is doing:

```asm
    push  cx            ; the driver's segment
    push  dx            ; the driver's offset
    mov   bp, sp
    call  dword ptr [bp]
    add   sp, 4
```

`BP` is restored **before** anything BP-relative is touched again, because the
driver is free to return with `BP` pointing anywhere. And `DS` and `ES` are
saved because in the HUGE model the compiler assumes `DS` still points at this
module's data when the block ends, and the driver makes no such promise.

> **If TCC ever objects to `call dword ptr [bp]`**, that same instruction can
> be written by hand as `db 0FFh, 05Eh, 000h`. It is noted in the code itself.

**`ipx_socket_call()`** (line 358)
Opening and closing the socket do not use an ECB: the socket number goes
straight into `DX` and the result comes back in `AL`. The same far call trick,
except here the result has to be picked up **after** `BP` is restored, or the
`mov result_code, ax` would write into the middle of the stack.

**`net_post_listen()`** (line 430)
Hands one of the receive buffers back to the driver.

A buffer **is either ours or the driver's, never both**. Handing it over makes
it the driver's; it becomes ours again when `in_use` drops to 0.

There are **four** buffers in rotation (`NET_LISTEN_ECB_COUNT`) on purpose:
while we are dealing with a packet its buffer is ours, and a packet arriving
right then would be lost if it were the only one.

> **`esr_address` is always NULL.** IPX can call a routine of ours the moment a
> packet lands, but that routine would run **at interrupt time**, in the middle
> of whatever the game was doing, with all the reentrancy problems that brings.
> Looking at `in_use` once a frame is enough and cannot go wrong.

### Layer 2 — our packet on top of IPX

**`net_send_is_busy()`** (line 460)
This has to be asked **before building** the packet, not just before sending
it: while a send is in flight the buffer belongs to the driver, and building
the next payload on top of it would rewrite a packet already on its way out.

**`net_transmit()`** (line 483)
Fills in the IPX header and the ECB and releases the packet. **It never
waits.** If the previous send has not finished, this packet is simply dropped:
nothing that matters is lost, because the next one carries the last 8 frames
anyway.

The destination network is 0, which means "this one". Anything else would need
a router, and there is none between two machines on the same wifi.

**`net_packet_is_valid()`** (line 553)
Every packet of ours starts with `"CTRE"`. Anything that reaches the socket
without that mark is somebody else's traffic and is thrown away. So is anything
carrying **our own `instance_id`**: on real Ethernet a card does not hear
itself and that never happens, but checking costs nothing.

**`net_handle_packet()`** (line 654)
Sorts by type: `HELLO`, `HELLO_ACK` or `INPUT`.

**`net_poll()`** (line 728)
Picks up everything the driver has and gives the buffers back. It is the
**only** thing that moves data inwards, so **any loop that waits has to call
it**, or nothing ever arrives.

### Layer 3 — lockstep

**`net_set_local_input()`** (line 1046)
The whole input delay fits in one line: our keys go in at
`simulation_frame + NET_INPUT_DELAY`.

**`net_send_input()`** (line 1063)
Sends the last `NET_REDUNDANCY` frames, and the checksum when one is due.

**`net_has_remote_input()`** (line 1129)
Have their keys for the frame we are about to simulate arrived? While this is
0, **the game cannot advance**.

Each slot of the ring remembers **which frame it holds**, not just whether it
is full. Without that, a stale entry from a previous lap of the buffer would be
mistaken for the one we are waiting for.

**`net_set_local_checksum()`** / **`net_advance_frame()`** (lines 1175 and 1199)
They close the frame.

---

## 6. How the two machines find each other

`net_find_opponent()` (line 878) runs **in text mode, before switching to
VGA**. On purpose: in graphics mode there is nowhere to print, and this is
exactly the part that needs to be able to say what is going on.

Both copies do exactly the same thing:

1. Shout `HELLO` to everybody (`FF:FF:FF:FF:FF:FF`) every quarter of a second.
2. Answer any `HELLO` they hear with a `HELLO_ACK`.
3. Consider themselves paired when they **have heard a HELLO AND have had an
   answer to their own**.

**Nobody types an address anywhere.**

### Why the handshake goes both ways

Pairing on the first `HELLO` alone will not do: the faster machine would run
off and start the game while the slower one is still waiting for an answer that
is never coming, because the other one has stopped sending `HELLO`s. Requiring
both guarantees that when one starts, **the other already knows the game is
on**.

### Who is player 1

Settled **with nothing negotiated at all**. Each copy picks a random number at
startup (`instance_id`, seeded from the BIOS tick), both travel in the `HELLO`,
and **the lower one is player 1**, the tank at the bottom. Both machines
compare the same two numbers and reach the same conclusion by themselves.

The node address is deliberately not used for this: if the driver answered the
"give me my address" call (function 9) badly, both machines would think they
were player 1 and the game would be nonsense. The `instance_id` does not depend
on any call into the driver.

### Why there is no "press a key to start"

There was one, and **it was a mistake**. If a player is slow to press, their
machine is not servicing the network, its receive buffers fill up, and the
other one sits waiting for frame 0 until the connection times out.

In its place there is a **bounded two second pause** that **keeps calling
`net_poll()`**. Both machines start at frame 0; whichever gets there first
waits for the other, and lockstep handles that on its own as long as the wait
is short.

---

## 7. Desync detection

This is what saves the most time when something goes wrong, and that is why it
was there from the start and not bolted on afterwards.

> In lockstep, when the two machines stop agreeing, **nothing looks wrong**.
> Each screen carries on showing a perfectly coherent game. A different one,
> but coherent. You can chase that for days.

So every 30 frames (`NET_CHECKSUM_INTERVAL`) each machine sends a number that
sums up **all** of its state, and the other compares it with its own for that
frame. If they differ, it goes into the log:

```
NET DESYNC at frame 1830: mine 41234 theirs 41199
```

The checksum is worked out by `compute_state_checksum()` in `main.c`, because
`net.c` does not know what a tank is. Everything the simulation can change goes
in: positions, directions, bullets, score, explosions and the pause counter.
The multipliers are there so that swapping two values (say the two tanks' X)
still comes out to a different total.

Checksums for the last 64 frames are kept, so one arriving late still finds the
frame it belongs to.

---

## 8. Setting up the two machines in DOSBox

**DOSBox does not emulate a network card.** It emulates **IPX directly**: the
INT 2F, the far call, the ECBs. Which means **there is no driver to load**: no
`LSL.COM`, no ODI, no `IPXODI.COM`, no `NET.CFG`, no frame types.

### The easy way: the scripts

For two machines, the practical thing is not to write any `.conf` at all:

```
./launch_game_server.sh                        <- start this one FIRST
./launch_game_client.sh <server-ip>            <- and then this one
```

These scripts **work out where the game is on their own**, from where they
themselves are, and generate the `.conf` on every launch, so the project can
live in a different folder on each machine. Absolute paths written by hand
inside a `.conf` are the number one reason this fails when you carry it over to
the second machine.

Before launching anything they check that DOSBox is installed, that `bin/` and
`res/` exist (and tell you **the date of the executable**, so you can see if
you are running an old binary), that the port is neither privileged nor already
in use, and the client pings the server. The server prints the exact line to
run on the other machine, with the IP already filled in.

And "server" does not mean it does not play: both machines play the same. All
it does is bring up the tunnel so the other one can join. It has nothing to do
with who ends up being player 1 either, which the `instance_id` decides.

### By hand, if you prefer

On both machines, in `dosbox.conf`:

```ini
[ipx]
ipx=true
```

Machine A, the one acting as the hub of the tunnel (it plays too):

```ini
[autoexec]
ipxnet startserver 5213
mount c d:\msdos
c:
cd \game\bin
game.exe /net
```

Machine B:

```ini
[autoexec]
ipxnet connect 192.168.1.10 5213
mount c d:\msdos
c:
cd \game\bin
game.exe /net
```

Double click each one and the game starts already connected. The IP appears
**once**, in a configuration file, never in the code and never on a screen in
the game.

### The port: on Linux it CANNOT be 213

DOSBox tunnels IPX over UDP and defaults to **port 213**, which is the one IANA
assigns to IPX. On **Linux that will not do**: ports below 1024 are privileged
and a normal user cannot open them. `ipxnet startserver` fails, and the only
symptom you see is the other side saying `Timeout connecting to server`.

That is why a high port is used, **5213**, on both machines:

```
ipxnet startserver 5213
ipxnet connect <ip> 5213
```

On Windows 213 does work, but it is better to use the same port everywhere. And
if there is a firewall in the way, what has to be opened is **UDP 5213**.

### Checks before blaming the code

```
ipxnet ping      (inside DOSBox: does the other machine answer?)
ipxnet status
```

And from the operating system, before anything else:

```
ping -i 0.2 <the other machine's ip>
```

**Look at the maximum, not the average.** The average always looks lovely.

### The two wifi traps

1. **Client isolation (AP isolation).** Many routers have it switched on, and
   guest networks **always** do: it stops two devices on the same wifi from
   talking to each other. The internet works and `ipxnet connect` just hangs
   without saying why. If `ping` between the two machines does not work, this
   is it.
2. **Firewall, UDP 5213.** The machine running `startserver` has to accept
   inbound. That is the port we use, not DOSBox's default 213.

If either of the two can go on a cable, put it on a cable: every wifi hop adds
its own jitter.

### Use the same `cycles=`

Lockstep synchronises **by frame, not by time**, so you will not desync from
running at different speeds. But if one DOSBox runs twice as fast as the other,
it spends half its life waiting and both end up going at the slower one's pace.

---

## 9. On real hardware

The same `game.exe`, with no rebuild. The only difference is that there you do
have to load the driver first, in `autoexec.bat`, and in exactly this order:

```
LSL.COM          <- always first
RTSODI.COM       <- your card's ODI driver
IPXODI.COM       <- and IPX on top
```

And both machines have to use **the same frame type** in `NET.CFG`
(`ETHERNET_802.3`, `ETHERNET_II`...). If one is on one and the other on
another, both load with no errors, both look like they are working, and they
never hear each other. It is the number one failure.

> DOSBox's IPX tunnel is a closed world between DOSBoxes: **it does not talk to
> the real IPX on a LAN**. You cannot connect a DOSBox to a real DOS machine.

To check the hardware without depending on this code at all, the honest test is
to **run Doom with `-net 2`** between the two machines. It uses the driver the
same way this game does. If Doom works, this works.

---

## 10. What changed in `main.c`

Very little, and that is the good news. **Nothing was touched** in the
collisions, `update_bullet()`, `draw_to_buffer()`, `player_update_explosion()`,
`sound_update()` or `restart_game()`.

The underlying change is a single one:

> `process_player_input()` no longer reads `keys[]`. It receives **one byte
> with 5 bits** already worked out.

That way it cannot tell whether those keys came from this keyboard or from the
other machine, and it does not need to. That is what let the network be added
without touching the game.

| Added | What it does |
|---|---|
| `read_input_from_keys()` | Turns this keyboard into the 5 bit byte |
| `compute_state_checksum()` | The number that sums up the state, for desync detection |
| `set_text_mode()` | Back to text on the way out of a network game, so the message can be read |
| A block in the loop | Sends, waits, and hands out both tanks' keys |

One detail that is **not optional**: input is exchanged on **every** frame, the
explosion pause included. Both machines have to keep stepping through the same
frame numbers even when nothing is moving on screen, or one would end up
waiting for input the other never sent.

And the waiting loop calls `sound_update()` inside itself. The wait is usually a
fraction of a frame, but one bad moment on the wifi would make the card replay
the same half buffer and the sound would grind.

Over the network **both machines drive with the cursor keys and fire with
keypad 5**, whichever tank you were given.

---

## 11. Constants you can change

All in `header/net.h`.

| Constant | Value | What happens if you raise / lower it |
|---|---|---|
| `NET_INPUT_DELAY` | 5 | **The important one.** Higher = absorbs more jitter, responds later |
| `NET_REDUNDANCY` | 8 | Higher = survives more consecutive losses. Costs almost no bandwidth |
| `NET_INPUT_BUFFER_SIZE` | 64 | Frames kept. **Must be a power of two** |
| `NET_CHECKSUM_INTERVAL` | 30 | How often desync is checked |
| `NET_TIMEOUT_SECONDS` | 10 | Silence before the other machine is given up for dead |
| `NET_DISCOVERY_SECONDS` | 30 | How long it looks before giving up |
| `NET_SOCKET_NUMBER` | `0x869C` | In `net.c`. Has to be **the same on both** |

---

## 12. Diagnosis: what to look for in the log

| Line | What it means |
|---|---|
| `NET sizes: ecb=42 header=30 packet=60` | Correct. **Any other number** means the compiler padded the structures and IPX will read every field from the wrong place |
| `NET: no IPX driver found` | No driver. In DOSBox `ipx=true` is missing; on real DOS it has not been loaded |
| `NET: IPX driver entry at XXXX:YYYY` | The driver is there and we know where to call it |
| `NET: could not open the socket` | Another copy of the game has it open, or it was not closed properly last time |
| `NET: node 000000000000` | **Bad sign.** An all-zero node means no tunnel assigned us an address: this instance is not joined to one |
| `NET: node ... id ...` | Our address and our random number |
| `NET: paired. them id ... we are player N` | Paired, and which tank we got |
| `NET: discovery timed out` | The other machine never answered. Check the `ping` and client isolation |
| `NET DESYNC at frame N` | The two machines computed different states |
| `NET: connection lost at frame N` | 10 seconds with nothing received |
| `NET: sent X, received Y, waited Z frames` | Summary on the way out |

That last one is the most useful of the lot. **`waited`** counts how many
frames the game spent standing still waiting for the other machine. It has
**two different causes**, and telling them apart matters because the fix is not
the same:

- **Close to 0** → the link has plenty of room.
- **A few dozen, in bursts, over a long game** → network jitter. That is what
  `NET_INPUT_DELAY` is for. Raise it, or put one machine on a cable.
- **A large, steady fraction of the frames** (say half of them) → the two
  machines **are not running at the same speed**, and the faster one is being
  paced by the slower one. This is not jitter, and **raising
  `NET_INPUT_DELAY` does not fix it**: it only adds latency. A fixed buffer
  absorbs variation, not a permanent difference in speed. Lower `cycles` on
  **both** machines to a value the slower one can actually sustain.

To find out which one is the slow one, compare `waited` in the two logs: the
slow one will be close to zero, because it never has to wait for anybody.

---

## 13. The complete flow, from start to finish

### Startup

```
game.exe /net
   |
   +-- main()                        main.c:166
   |     reads the arguments, network_mode = 1
   |
   +-- net_init()                    net.c:775
   |     |
   |     +-- writes the structure sizes to the log
   |     |   (42/30/60: anything else means the compiler padded them
   |     |    and IPX would read every field from the wrong place)
   |     |
   |     +-- ipx_detect()            net.c:266
   |     |     INT 2F with AX=7A00. If AL comes back 0xFF, IPX is loaded
   |     |     and ES:DI is the way in. It is saved.
   |     |
   |     +-- ipx_socket_call(OPEN)   net.c:358
   |     |     opens socket 0x869C. From here on the driver will hand us
   |     |     whatever arrives on that socket and nothing else.
   |     |
   |     +-- ipx_get_local_address() net.c:411
   |     |     our node address. For the log only: if it comes back all
   |     |     zeros, this machine is NOT joined to any tunnel.
   |     |
   |     +-- instance_id = a random number from the BIOS tick
   |     |
   |     +-- net_post_listen() x4    net.c:430
   |           hands 4 buffers to the driver. From now on, anything that
   |           arrives lands in one of them. Four and not one because
   |           while we deal with a packet its buffer is ours.
   |
   +-- net_find_opponent()           net.c:878     <-- TEXT mode
   |     loops until paired (below)
   |
   +-- a two second pause, calling net_poll()
   |
   +-- install_kbd() / setup_screen() / init_players() / init_graphics()
   +-- sound_init()
   |
   +-- main loop
```

### The pairing

Both copies do literally the same thing. There is no "server" and "client" at
the game level: that is only DOSBox's tunnel.

```
      MACHINE A                                MACHINE B

  HELLO --> broadcast  ------------------------->  net_poll()
                                                   net_handle_packet()
                                                   stores its node and id
                                                   sets bit 0x01
                                    <-- HELLO_ACK  answers directly
  net_poll()
  sets bit 0x02
                                                   HELLO --> broadcast
  net_handle_packet()  <---------------------------
  stores its node and id
  sets bit 0x01
  HELLO_ACK -->  -------------------------------->  net_poll()
                                                    sets bit 0x02

  bits == 0x03  -> PAIRED               bits == 0x03  -> PAIRED
```

**Both bits** are required: having heard the other one (`0x01`) and having had
the other one answer us (`0x02`). With only one, the faster machine would run
off and play while the slower one waits for an answer nobody is going to send.

On leaving the loop, both do the same thing on their own and reach the same
conclusion:

```c
if (local_instance_id < remote_instance_id){ is_player1 = 1; }
```

And they set `simulation_frame = 0`, clear the rings, and fill the first
`NET_INPUT_DELAY` frames with "no keys pressed", because nobody ever sent them.

### Every frame of the main loop

This is the heart of it. `main.c:276` onwards.

```
 1. read_input_from_keys()      main.c:563
       cursor keys + keypad 5  ->  one byte with 5 bits

 2. net_set_local_input(byte)   net.c:1046
       stores it at frame  N + NET_INPUT_DELAY     <-- THE DELAY
                                                       LIVES HERE

 3. net_send_input()            net.c:1063
       one packet with frames  N-2 .. N+5  (8 frames, the redundancy)
       plus the state checksum if one is due

 4. while (net_has_remote_input() == 0){        <-- THE WAIT
        net_poll();          pick up whatever arrived
        sound_update();      the sound does NOT stop while we wait
        net_connection_lost() ?  -> leave
    }

 5. hand out according to who we are:
       if I am player 1:  p1 = my byte for frame N,  p2 = theirs
       otherwise:         p1 = theirs,               p2 = my byte

 6. process_player_input(&player1, &player2, p1)    main.c:915
    process_player_input(&player2, &player1, p2)
       <-- from here down, NOTHING knows a network exists

 7. update_bullet() x2, explosions, sound, score
 8. update_game() / draw_to_buffer() / wait_retrace() / dump to VGA

 9. net_set_local_checksum(compute_state_checksum())   main.c:611
       sums up ALL the state in one number and stores it

10. net_advance_frame()         net.c:1199
       N = N + 1
```

Steps 6, 7 and 8 are **exactly the code that was already there**. Not one line
of collisions, bullets, explosions or drawing was touched.

### What goes down the wire

A packet is **60 bytes**, of which 30 are the IPX header:

```
+--------------------------------+  30 bytes  IPX header
| dest network / node / socket   |            (we fill in the destination,
| src  network / node / socket   |             the driver fills in the source)
+--------------------------------+
| "CTRE"                         |   4        mark, so we ignore other traffic
| type (HELLO/ACK/INPUT)         |   1
| how many inputs                |   1
| instance_id                    |   4        who sent it / who is P1
| first frame of inputs[]        |   4
| inputs[8]                      |   8        <-- THE ACTUAL DATA
| is there a checksum?           |   1
| padding                        |   1
| frame of the checksum          |   4
| checksum                       |   2
+--------------------------------+  30 bytes  our payload
```

Look at the proportion: out of 60 bytes, **8 are the game**. The header weighs
four times what the data does. That is why the redundancy is free and why 8
frames are sent instead of 1.

### Shutting down

```
ESC
 |
 +-- sound_shutdown()    stops the DMA BEFORE releasing the memory
 +-- net_shutdown()      closes the socket, writes the summary to the log
 +-- player_free() / bmp_delete_buffers() / bmp_close_files()
 +-- uninstall_kbd()     puts the original INT 9 back
 +-- set_text_mode()     main.c:545, so the final message can be read
```

**Leaving by closing the window runs none of this.** In DOSBox it makes no
difference, but on real DOS the IPX socket stays open and the next game cannot
open the same one, and the Sound Blaster's DMA carries on reading memory that
is no longer yours.

---

## 14. Reference for every function

### `src/net.c` — layer 1, the IPX driver

| Function | Line | What it does |
|---|---|---|
| `net_swap16()` | 242 | Flips the two bytes of an integer. IPX writes sockets and lengths high byte first, and the 8086 stores them the other way round |
| `ipx_detect()` | 266 | `INT 2F` with `AX=7A00`. If `AL` comes back `0xFF`, IPX is loaded and `ES:DI` is the way in |
| `ipx_call()` | 308 | **The** call into the driver: `BX` = what to do, `ES:SI` = the ECB. In assembler, because IPX wants its arguments in registers and is entered with a `CALL FAR` |
| `ipx_socket_call()` | 358 | Opening and closing the socket. No ECB: the number goes in `DX` and the result comes back in `AL` |
| `ipx_get_local_address()` | 411 | Our node address. For the log only |
| `net_post_listen()` | 430 | Hands one receive buffer to the driver |

### `src/net.c` — layer 2, our packet

| Function | Line | What it does |
|---|---|---|
| `net_send_is_busy()` | 460 | Is the driver still busy with the last packet? Must be asked **before building**, not just before sending |
| `net_transmit()` | 483 | Fills in the IPX header and the ECB and releases the packet. Never waits |
| `net_build_header()` | 535 | The part of the payload every packet carries: mark, type, id |
| `net_packet_is_valid()` | 553 | Does it start with `"CTRE"`, and is it not our own echo? |
| `net_handle_packet()` | 654 | Sorts by type: HELLO, HELLO_ACK or INPUT |
| `net_poll()` | 728 | Picks up everything the driver has and gives the buffers back. **The only thing that moves data inwards** |

### `src/net.c` — layer 3, lockstep

| Function | Line | What it does |
|---|---|---|
| `net_store_remote_input()` | 594 | Files one frame of the other machine's keys. Drops what is already simulated and what is too far ahead |
| `net_check_remote_checksum()` | 622 | Compares their checksum against ours for that frame. If it does not match, `NET DESYNC` to the log |
| `net_set_local_input()` | 1046 | Our keys go in at `frame + NET_INPUT_DELAY`. **The whole input delay is this one line** |
| `net_send_input()` | 1063 | Sends the last 8 frames of keys, and the checksum when one is due |
| `net_has_remote_input()` | 1129 | Have their keys for the current frame arrived? While this is 0, the game does not advance |
| `net_get_remote_input()` | 1148 | Their keys for this frame |
| `net_get_local_input()` | 1159 | Ours for this frame. **From the ring, not from the keyboard**: that is why they are applied just as late as theirs |
| `net_set_local_checksum()` | 1175 | Stores the state summary and, every 30 frames, sets one aside to travel |
| `net_advance_frame()` | 1199 | Next frame |

### `src/net.c` — life cycle and warnings

| Function | Line | What it does |
|---|---|---|
| `net_init()` | 775 | Finds the driver, opens the socket, leaves the 4 buffers listening |
| `net_find_opponent()` | 878 | The HELLO / HELLO_ACK until paired. In text mode |
| `net_is_player1()` | 1024 | Which tank we got |
| `net_get_frame()` | 1031 | The frame being simulated |
| `net_connection_lost()` | 1213 | 1 when we have had 10 seconds with nothing received |
| `net_desync_detected()` | 1243 | 1 when the two machines have computed different states |
| `net_count_wait()` | 1254 | Counts one frame spent waiting, for the log summary |
| `net_shutdown()` | 848 | Closes the socket and writes the summary |

### `src/main.c` — what was added

| Function | Line | What it does |
|---|---|---|
| `read_input_from_keys()` | 563 | Turns this keyboard into the 5 bit byte. **The only one that still reads `keys[]`** |
| `compute_state_checksum()` | 611 | Sums up the whole game state in one number, for desync detection |
| `set_text_mode()` | 545 | Back to text mode on the way out, so the final message can be read |
| `process_player_input()` | 915 | **Modified.** It used to read `keys[]`; now it receives the byte already worked out. That is the only underlying change the whole game needed |

### And what was **not** touched

`is_blocked_by_wall()`, `is_blocked_by_tank()`, `is_move_blocked()`,
`bullet_has_hit_tank()`, `update_bullet()`, `move_sprite()`,
`update_player_animation()`, `draw_to_buffer()`, `draw_explosion()`,
`restart_game()`, `update_game()`, all of `players.c`, all of `sound.c` and all
of `bmp.c`.

That is not an accident or good luck: it is the consequence of having chosen
lockstep. Since both machines run the whole game and only exchange keys, there
is nothing in the game that needs to know a network exists.
