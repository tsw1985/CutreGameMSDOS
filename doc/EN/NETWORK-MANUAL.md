# Manual: how the game was made to work over the network

From nothing to two tanks fighting across two computers.

---

## Who this is for

For you in six months, when you open `net.c` and remember none of it.

I assume you are fluent in C, comfortable in DOS, and that you **understand
your own game**: the main loop, the collisions, how things are drawn, how a
tank moves.

I assume you know **nothing at all about networking**. Not what a socket is,
not what a packet is, not what a protocol is. All of that is explained here
from the beginning.

### How to read it

It is in **learning order**, not file order. Each part builds on the one
before:

| Part | What it covers |
|---|---|
| **1** | The problem. What "playing over the network" actually means |
| **2** | The idea that solves it: determinism and lockstep |
| **3** | How you send a byte in DOS: IPX |
| **4** | The code, read in execution order |
| **5** | **How all of it plugs into your game** |
| **6** | What happens when things go wrong |
| **7** | Experiments to play with, and a glossary |

**Do not skip part 2.** It is 80% of the understanding. Parts 3 and 4 are
mechanics; without part 2 they are noise.

When you are done, [`NETWORK.md`](NETWORK.md) is the reference to look
individual things up in.

---

# PART 1 — THE PROBLEM

## 1. Two computers, one game

Right now your game works like this:

```
   +-------------------------------------------+
   |             ONE COMPUTER                  |
   |                                           |
   |   keyboard ---> keys[] ---> game ---> VGA |
   |                                           |
   |   BOTH players share everything:          |
   |   the same keyboard, the same memory,     |
   |   the same screen, the same loop          |
   +-------------------------------------------+
```

Both tanks live in the same variables (`player1` and `player2`), move in the
same `while`, and are drawn into the same buffer. **There is nothing to
coordinate**, because there are not two of anything.

Playing over the network means this:

```
   +---------------------+          +---------------------+
   |    COMPUTER A       |          |    COMPUTER B       |
   |                     |          |                     |
   |  keyboard A         |          |         keyboard B  |
   |     |               |          |               |     |
   |     v               |          |               v     |
   |  player1, player2   | <------> |  player1, player2   |
   |     |               |   ???    |               |     |
   |     v               |          |               v     |
   |  screen A           |          |            screen B |
   +---------------------+          +---------------------+
```

Look carefully, because the whole problem is right there:

> **There are TWO copies of `player1` and `player2`.** One on each computer.
> They are different variables, in different memory, on different machines.

And both have to tell **the same story**. If on your screen your tank is at
(110, 80) and on his it is at (110, 84), you are playing two different games
that look like the same one. You shoot and hit; he sees the bullet miss.

**That is the entire problem.** Everything else in this manual is how it gets
solved.

---

## 2. The obvious idea, and why it does not work

The first thing anyone thinks of is this:

> "Easy: each computer sends where its tank is, and the other one draws it
> there."

Let us try it in our heads. Every frame, computer A sends:

```
my tank is at (110, 80), facing up, no bullet out
```

And B receives it and puts its copy of `player1` at (110, 80).

It looks like it works. **It does not**, for four reasons worth seeing now
because they explain every decision that follows.

### Problem 1: there is far more to send than it looks

It is not just the position. Think about everything that has to be sent for the
two screens to look the same:

- the tank's X and Y
- the direction it faces
- which track animation frame it is on
- the bullet's position
- the bullet's direction
- whether the bullet is flying
- whether the tank is exploding
- how far into the explosion it is
- the score

And all of that **every frame, 70 times a second**. That is 20-30 bytes per
machine per frame instead of 1. Not catastrophic, but 30 times more, and this
is a 1994 network.

### Problem 2: you are always late

Packets take time. Even 1 millisecond, they take time. So when B receives "A's
tank is at (110, 80)", **A is not there any more**. B is drawing the past.

With 5 ms of delay and a tank moving 2 pixels a frame, B draws A's tank about 5
pixels behind where it really is. You see your tank in one place and he sees it
in another. Who is right when you shoot?

### Problem 3: who decides?

Imagine the two tanks driving into each other and colliding.

- Computer A works out the collision and says: "tank 2 stops here."
- Computer B works out the same collision with slightly older data and says:
  "tank 2 stops two pixels further on."

Who wins? There is no answer. You would have to appoint one of them "the boss"
(an **authoritative server**), and then the other player has to swallow
whatever the first one says, even about his own tank. That is what produces
that rubber-band feeling in online games: your character moves forward and is
suddenly yanked back because the server disagreed.

### Problem 4: you have to touch the whole game

And this is the one that really hurts. To send the state you have to:

- write code that packs `struct player` into bytes
- write code that unpacks it
- remember to update it **every time** you add a field to the struct
- decide what to do when a packet arrives late or out of order
- write interpolation so the other tank does not move in jumps

**Your game would stop being your game.** Every function that touches `player1`
would have to know a network exists.

### So what?

Keep those four problems in mind. Chapter 5 solves **all four at once**, with
an idea that looks like a magic trick.

---

## 3. The two questions of every networked game

Everything above boils down to two questions. Every networked game in the
world, from 1994 or from today, answers these two:

> **Question 1: WHAT do I send?**
>
> **Question 2: How do I get both machines to agree?**

The obvious idea answers:

1. Send **the state** (where everything is).
2. Appoint one the boss and let the other obey.

We are going to answer **completely differently**:

1. Send **the keys being pressed**. Nothing else.
2. No boss needed: both machines work out the same thing on their own.

It sounds impossible. The next chapter explains why it is not.

---

# PART 2 — THE IDEA THAT SOLVES IT

## 4. Determinism: why your game can cheat

This is the central idea of the whole manual. If you take one thing away, take
this.

**Deterministic** means: *given the same inputs, it always produces exactly the
same result*.

Look at it in your own code. Take `move_sprite()`:

```c
if (direction == MOVE_UP){
    if (_player->position_y >= PIXEL_TO_MOVE){
        _player->position_y = _player->position_y - PIXEL_TO_MOVE;
    }else{
        _player->position_y = 0;
    }
}
```

If `position_y` is 80 and `direction` is `MOVE_UP`, the result is **78**.
Always 78. On your Pentium III, on a 486, on a Core i9, in DOSBox, today and in
twenty years. There is no way for it to come out as anything else.

Now look at your whole game with those eyes:

| | Is it in your game? | Why it would matter |
|---|---|---|
| Floating point (`float`) | **No** | Two CPUs can round differently in the last bit. That bit propagates and a thousand frames later the tanks are in different places |
| `rand()` | **No** | Each machine would draw different numbers |
| Reading the clock | **No** | Two machines' clocks never agree |
| Pointers used as values | **No** | `farmalloc()` can return different addresses on each machine |
| Uninitialised memory | **No** | Different garbage on each machine |

Your game is **pure integer**: `unsigned int` for everything, additions,
subtractions and comparisons. And that, which you did because it was the
natural thing in Turbo C, turns out to hand you the single most valuable
property a game can have for playing it over a network.

> **If both machines start in the same state and receive the same sequence of
> keys, they compute exactly the same pixels until the end of the game.**
>
> Not "similar". **The same.** Bit for bit.

Read it again, because it is counterintuitive and everything else comes from
it.

### A thought experiment

Imagine recording into a file every key that was pressed during a game, frame
by frame:

```
frame 0: nothing
frame 1: nothing
frame 2: player1=UP, player2=nothing
frame 3: player1=UP, player2=LEFT
...
frame 4210: player1=FIRE, player2=UP
```

If tomorrow you start the game and, instead of reading the keyboard, you read
that file, you will see **exactly the same game**. The same moves, the same
shots, the same explosion on the same pixel, the same winner.

That is what being deterministic means. And look at the size of that file: one
byte per player per frame. A five minute game is 21,000 frames, that is
**42 KB**. A whole game fits on a floppy.

---

## 5. Lockstep: do not send positions, send keys

Now put the two ideas together:

1. The game is deterministic.
2. A file of keys reproduces the whole game.

> **What if, instead of saving the keys to a file, I send them to the other
> machine in real time?**

That is **lockstep**. And this is what changes:

```
   +---------------------+          +---------------------+
   |    COMPUTER A       |          |    COMPUTER B       |
   |                     |          |                     |
   |  keyboard A         |          |         keyboard B  |
   |     |               |          |               |     |
   |     +---> 1 byte -------------------> (receives)|    |
   |     |               |          |               |     |
   |     |  (receives) <------------------- 1 byte --+    |
   |     v               |          |               v     |
   |  SIMULATES BOTH     |          |  SIMULATES BOTH     |
   |  TANKS COMPLETELY   |          |  TANKS COMPLETELY   |
   |     |               |          |               |     |
   |     v               |          |               v     |
   |  screen A           |          |            screen B |
   +---------------------+          +---------------------+
```

Both machines run **the complete game**. Both tanks. Both bullets. Both sets of
collisions. Both explosions. The score. Everything.

Neither machine is a client of the other. **Both are the whole game.**

And the only thing crossing the wire is:

```
one byte per player per frame
```

That byte is five bits:

```c
#define NET_INPUT_UP		0x01     // bit 0
#define NET_INPUT_DOWN		0x02     // bit 1
#define NET_INPUT_LEFT		0x04     // bit 2
#define NET_INPUT_RIGHT		0x08     // bit 3
#define NET_INPUT_FIRE		0x10     // bit 4
```

If the player is holding UP and FIRE at once, the byte is `0x11`.

### Watch the four problems from chapter 2 disappear

This is the beautiful part:

| Problem with the obvious idea | How lockstep kills it |
|---|---|
| **1. Far too much to send** | 1 byte instead of 30. And it does not matter how much you add to the game: **it stays 1 byte**. You could add ten tanks, mines and power-ups, and what travels does not grow |
| **2. You are always late** | You are not drawing the past: you draw exactly what the other one draws, on the same frame. The two screens are always identical |
| **3. Who decides?** | Nobody. There is no boss. Both machines work out the collision from **the same data** and reach the same answer. There is nothing to negotiate |
| **4. You have to touch the whole game** | **Nothing is touched.** Not collisions, not bullets, not explosions, not drawing, not sound. Not one line |

That last one is the important one and part 5 covers it in detail.

### The loop, in three lines

```
frame N:  1. send my keys
          2. wait for the other machine's keys for frame N
          3. simulate frame N with BOTH
```

And that is it. That is the whole networked game, conceptually. Everything else
is detail about making it work for real.

### The price

Lockstep has a cost and you should know it:

> **If a packet does not arrive, BOTH machines stop.**

It is not that one runs worse. The one waiting cannot advance, because it does
not know which keys to apply, and if it advanced by making them up it would no
longer be computing the same game. So it waits. And the other one, on the next
frame, waits too.

Chapters 6 and 7 are the two things we do to make sure that almost never
happens.

---

## 6. The mistake that desyncs you: the input delay

This chapter explains **the mistake everybody makes the first time**. Including
me, if I were not careful.

### The obvious attempt

The natural way to write it would be:

```
frame N:  my keys -> apply NOW to my tank            <-- feels great
          whatever of his keys have arrived -> apply to his tank
```

Your tank responds instantly. Perfect. **And you desync in under a second.**

### Why it fails

Let us trace it. You press UP on frame 100. Your packet takes 3 frames to reach
the other machine.

```
                MACHINE A (you)             MACHINE B (the other one)
frame 100   your tank moves up 2 px     your tank does NOT move
                                        (the packet is on its way)
frame 101   your tank moves up 2 px     your tank does NOT move
frame 102   your tank moves up 2 px     your tank does NOT move
frame 103   your tank moves up 2 px     the packet arrives: up 2 px
```

By frame 103 your tank is at `y = 80 - 8` on your screen and at `y = 80 - 2` on
his. **Six pixels apart.**

And this does not fix itself, it grows. Worse: the moment there is a collision,
the two machines will compute different things, and from there the games
diverge beyond repair. One sees the bullet hit, the other sees it miss.

The root of the bug is treating your keys and his **differently**. One is
applied on frame 100 and the other on frame 103.

### The fix: delay yours too

```
frame N:  my keys -> store them for frame N + 5
          simulate frame N with the keys stored for frame N
                            (mine AND his)
```

In other words: **your own keys wait too.** Exactly as long as the other
player's.

```
                MACHINE A (you)             MACHINE B (the other one)
frame 100   store UP for frame 105       (the packet is on its way)
frame 101   tank still                   arrives: store UP for frame 105
frame 102   tank still                   tank still
frame 103   tank still                   tank still
frame 104   tank still                   tank still
frame 105   UP: moves up 2 px            UP: moves up 2 px      <-- IDENTICAL
```

On frame 105 **both machines do the same thing**. And on 106, and on 4210. They
cannot drift apart.

That is `NET_INPUT_DELAY`, and it is 5:

```c
#define NET_INPUT_DELAY 	5
```

### How much do you notice?

The loop runs at 70 fps, so one frame is **14.3 ms**. Five frames are **71 ms**
between pressing and your tank moving.

Is that a lot? It depends on the game:

- In a fighting game it would be unacceptable.
- Here, with `PIXEL_TO_MOVE 2`, those five frames are **10 pixels** of delay
  before you start moving, on a 320 wide screen with 18 pixel tanks and the
  inertia a tank has. **You do not notice it.**

### Why 5 and not 1

Because the delay is the **cushion** that absorbs the network's mood swings. If
a packet takes 4 frames to arrive and the delay is 5, it arrives in time and
you notice nothing. If it takes 6, the game stops for one frame.

| `NET_INPUT_DELAY` | Cushion | When to use it |
|---|---|---|
| 3 | ~43 ms | A cable, or two DOSBoxes on one PC |
| **5** | **~71 ms** | **What is set. Ordinary wifi** |
| 7 | ~100 ms | Bad wifi, or playing over the internet |

The higher it is, the more network trouble it survives and the later your tank
responds. It is a trade, and there is no "correct" value: it depends on your
network.

> **The golden rule of lockstep, and the one you cannot break:**
>
> Both inputs are applied on the same frame on both machines. Always. The
> moment you make an exception "just so it feels better", it is over.

---

## 7. What if a packet is lost: redundancy

We know what to send and when to apply it. One problem left: **packets get
lost**.

On a network there are no guarantees. A packet can be lost, can arrive late, or
two can arrive out of order. That is normal and has to be planned for.

### What a normal program would do

A serious program would ask for the packet again:

```
B: "hey, I never got frame 105, send it again"
A: "here"
```

That is called **retransmission**, and it is what TCP does for you. And here it
would be **terrible**: by the time the request has gone and the answer come
back, two full network round trips have passed. The game has been frozen the
whole time.

### What we do: send it before it is asked for

Look at the real size of one of our packets:

```
+--------------------------------+
|      IPX header: 30 bytes      |   <-- mandatory, the protocol puts it there
+--------------------------------+
|      our data: 30 bytes        |   <-- of which 8 are keys
+--------------------------------+
   total: 60 bytes
```

Here is the key observation:

> **Sending 1 byte of keys or sending 8 costs the same packet.**

The header weighs 30 bytes whatever you do. The 7 extra bytes are noise next to
that. **Redundancy is free.**

So every packet carries the **last 8 frames** of keys, not just the current
one:

```c
#define NET_REDUNDANCY 		8
```

### What it looks like

If you are on frame 100 and the delay is 5, the packet going out carries the
keys for frames **98 to 105**:

```
packet from frame 100:  [98][99][100][101][102][103][104][105]
packet from frame 101:  [99][100][101][102][103][104][105][106]
packet from frame 102:  [100][101][102][103][104][105][106][107]
```

Every frame travels **eight times**, in eight different packets.

If the packet from frame 100 is lost, absolutely nothing happens: frame 105 was
also in the packet from 101, and in the one from 102, and in six more. The next
one covers it.

**For the game to actually stop, 8 packets in a row would have to be lost.** On
a normal network that hardly ever happens.

### The lovely detail

Look at what we have achieved:

- **No retransmissions.** Nobody asks for anything.
- **No acknowledgements.** Nobody confirms anything.
- **No ordering.** If packets arrive out of order it does not matter: each one
  says which frames its contents belong to, and they get filed in the right
  place.
- **No connection.** There is nothing to "establish" and nothing to keep alive.

All of that is complexity that **does not exist** in this code, and it does not
exist because it is unnecessary. It is the reason `net.c` fits in 1200
commented lines.

And it is also the reason for choosing a dumb protocol over a clever one: TCP
would do all the things we just decided not to do, on its own, and give us its
hitches in return. Part 3 covers which protocol we use and why.

---

# PART 3 — HOW YOU SEND A BYTE IN DOS

We know **what** to send (a byte of keys) and **when** to apply it (five frames
later). What is left is the down-to-earth part: how you actually send it.

## 8. What "the network" is in DOS

If you come from programming on any modern system, you have an idea of
networking that **does not exist here**. On Linux or Windows you write:

```c
socket();  connect();  send();
```

and the operating system handles everything. In DOS that is not there. And it
is not there for a fundamental reason:

> **DOS is not a multitasking operating system. It is a program loader with a
> filesystem.** There are no drivers, no network stack, nothing running in the
> background. When your program starts, your program **is** the machine.

Turbo C++ 3.0 has nothing either. Its library is pure DOS: `fopen`, `int86`,
`inp`, `outp`. There is no `<sys/socket.h>`. It does not exist.

### So how did DOS programs talk over a network?

With a **TSR**: *Terminate and Stay Resident*. A program you run once, which
stays in memory and exits. Like your own INT 9 handler, but in a separate
`.COM`.

```
autoexec.bat:
    LSL.COM          <- stays in memory
    RTSODI.COM       <- stays in memory
    IPXODI.COM       <- stays in memory
    ...
    game.exe         <- your game, which talks to them
```

That TSR is "the network". It knows how to talk to the card, how to send and
receive, and it leaves **a way in** so other programs can ask it for things.

### And how do you talk to something already in memory?

The same way you talk to the BIOS: **through an interrupt**. You load some
registers, do an `int` with a particular number, and the TSR does its job.

It is exactly the same mechanism you already use in the game.
`set_vga_320_200_mode()` does `int 0x10` to talk to the video BIOS. Here we
will do something very similar to talk to the network driver.

**That is the whole magic.** There is nothing more mysterious to it: the
network in DOS is a program already in memory that you make calls into.

---

## 9. What IPX is

**IPX** stands for *Internetwork Packet eXchange*. It is the network protocol
of **Novell NetWare**, from the mid 80s.

A bit of history, because it helps: in the 80s and early 90s, "office network"
meant NetWare. TCP/IP was a university and Unix thing. If you had PCs connected
in a company, they almost certainly spoke IPX. And that is why **every** DOS
multiplayer game used it: Doom, Duke Nukem 3D, Warcraft II, Command & Conquer,
Descent...

### Compared with what you know today

| | TCP/IP (today) | IPX |
|---|---|---|
| Machine address | IP: `192.168.1.45` | **Node**: `00:1A:2B:3C:4D:5E` (the MAC!) |
| Port / channel | Port: `8080` | **Socket**: `0x869C` |
| Network | Netmask, gateway, DHCP | A 4 byte **network number**, normally 0 |
| Names | DNS | None. None needed |
| "No guarantees" version | UDP | **IPX** ← the one we use |
| "With guarantees" version | TCP | SPX ← we do not use it |

### The three things that make it right here

**1. The address is the card's MAC.**

This matters more than it sounds. In TCP/IP an IP is a number *somebody* has to
assign: either you configure it by hand or you set up a DHCP. In IPX the
address **is already burned into the card at the factory**. There is nothing to
configure. Plug two machines in and they can talk.

And there is a special address:

```
FF:FF:FF:FF:FF:FF   =  "everyone on this network"
```

That is called **broadcast**, and it is what lets the two copies of the game
find each other without anyone typing an address. We come back to it in chapter
13.

**2. There is no stack to link.**

This is the decisive one. If we wanted TCP/IP we would have to put a library
(WATTCP) inside the executable, with its ARP, its IP, its UDP and its buffer
management. And **rebuild it for the HUGE model**, because the prebuilt
versions of the era came for LARGE.

With IPX nothing is linked. Not one `.LIB`. The driver is already in memory and
all you have to do is call it.

**3. It is dumb, and that is what we want.**

IPX does not retry, does not order, does not guarantee delivery, does no
congestion control. You send a packet and off it goes. If it arrives, it
arrives.

That sounds like a flaw and it is **exactly what chapter 7 needs**. We already
decided we do not want retransmissions. TCP would give them to us whether we
liked it or not.

### What IPX does not give you, and we supply

| Missing | Where we solve it |
|---|---|
| Reliability | 8 frame redundancy (ch. 7) |
| Knowing who is on the other end | The broadcast HELLO (ch. 13) |
| Making both games agree | Lockstep (ch. 5) |

---

## 10. The ECB: the form you fill in for the driver

Now the practical part: how do you tell the driver "send this"?

You do not pass it arguments like a C function. You fill in **a structure in
memory** and give it the address. That structure is called an **ECB**: *Event
Control Block*.

Think of it as a shipping form:

```
   +---------------------------------------------+
   |  ECB - "shipping form"                      |
   +---------------------------------------------+
   |  Which socket?           0x869C             |
   |  Which MAC?              FF:FF:FF:FF:FF:FF  |
   |  Where is the data?      -----> [packet]    |
   |  How many bytes?         60                 |
   |                                             |
   |  in_use:  [ ] <-- the driver marks this     |
   +---------------------------------------------+
```

You fill in the form, hand it over, and the driver does the work. The structure
in `net.c` is literally that:

```c
struct ipx_ecb {
    void far      *link_address;         // the driver's own bookkeeping
    void far      (*esr_address)();      // ALWAYS NULL here, see below
    unsigned char  in_use;               // the driver marks it while working
    unsigned char  completion_code;      // 0 = it went fine
    unsigned int   socket_number;
    unsigned char  ipx_workspace[4];     // the driver's own bookkeeping
    unsigned char  driver_workspace[12]; // the driver's own bookkeeping
    unsigned char  immediate_address[6]; // the destination MAC
    unsigned int   fragment_count;       // always 1 here
    void far      *fragment_address;     // where the packet is
    unsigned int   fragment_size;        // how big it is
};
```

### `in_use`: who owns the buffer

This field is the heart of the matter and is worth stopping on.

> A buffer **is either yours or the driver's, never both at once.**

- You hand over the ECB → the buffer becomes **the driver's**. `in_use` goes
  non-zero.
- The driver finishes → it sets `in_use` to **0**. The buffer is **yours**
  again.

If you write into the buffer while `in_use` is not zero, you are modifying a
packet the driver may be sending at that very moment. That is exactly the bug
that was in the first version of `net_transmit()`, and it is why
`net_send_is_busy()` exists.

### `esr_address`: the trap we did not fall into

IPX offers you something tempting: you can give it the address of a function of
yours, and it calls you **the moment a packet arrives**. Automatic, no asking
required.

**Here it is always NULL, on purpose.**

Because that function would run **at interrupt time**: in the middle of
whatever the game was doing. It could interrupt `draw_to_buffer()` halfway
through, or the sound mixer, or your own keyboard handler. Everything it
touched would have to be reentrant.

You already know what that means, because your sound ISR takes the same care:
`sound_isr()` just raises a flag and leaves.

Here we do the same but even simpler: **there is no network ISR**. Once a frame
we look at `in_use` and that is all. It is called **polling**, it is the most
boring thing there is, and it cannot go wrong.

### The IPX header

Besides the ECB, the packet itself carries a 30 byte header in front of it that
the protocol defines:

```c
struct ipx_header {
    unsigned int   checksum;                // 0xFFFF (IPX never really used it)
    unsigned int   length;                  // total size
    unsigned char  transport_control;       // router hops. 0 here
    unsigned char  packet_type;             // 4 = ordinary datagram
    unsigned char  destination_network[4];  // 0 = "this same network"
    unsigned char  destination_node[6];     // the destination MAC
    unsigned char  destination_socket[2];
    unsigned char  source_network[4];       // \
    unsigned char  source_node[6];          //  > the driver fills these in
    unsigned char  source_socket[2];        // /
};
```

Notice the last part: **the sender is filled in by the driver**, not by us. So
when we receive a packet we automatically know who it came from, without anyone
having to tell us. That is what makes the discovery in chapter 13 possible.

### One confusing detail: byte order

IPX writes numbers **with the big byte first** (*big endian*):

```
socket 0x869C  is written in memory as:   86 9C
```

But the 8086 stores `unsigned int` **the other way round** (*little endian*):

```
unsigned int x = 0x869C;  is stored in memory as:   9C 86
```

So to get `86 9C` into memory you have to put the value `0x9C86` into the
variable. Hence this function:

```c
static unsigned int net_swap16(unsigned int value){
    unsigned int high_byte;
    unsigned int low_byte;

    high_byte = (value >> 8) & 0x00FF;
    low_byte  = value & 0x00FF;

    return (low_byte << 8) | high_byte;
}
```

`net_swap16(0x869C)` returns `0x9C86`, which in memory comes out as `86 9C`,
which is what IPX wants to read. It sounds like a muddle and it is only this:
**swapping two bytes**.

---

## 11. Calling the driver, and why it is assembler

We have the form filled in. How do we hand it to the driver?

### Finding the door

First we have to know whether the driver is there and where its entry point is.
You ask with interrupt `2F`, which is the DOS "is anybody there?" multiplex:

```c
static int ipx_detect(void){
    union  REGS  regs;
    struct SREGS sregs;

    segread(&sregs);

    regs.x.ax = 0x7A00;          // 7A00 = "are you there, IPX?"
    int86x(0x2F, &regs, &regs, &sregs);

    if (regs.h.al != 0xFF){      // 0xFF = "yes, here I am"
        return 0;                // anything else: no driver
    }

    ipx_entry_segment = sregs.es;   // and it leaves its address in ES:DI
    ipx_entry_offset  = regs.x.di;

    return 1;
}
```

This is identical in spirit to what you already do with the video BIOS. You
load `AX`, fire the interrupt, and look at what comes back.

If `AL` comes back `0xFF`, IPX is there, and `ES:DI` is **the address to call**
from then on. It is saved in two globals.

This is the route Novell documented and the one Doom used. It works with a real
`IPXODI`, with Novell's client, and with DOSBox, which answers this call
exactly like a real driver does. **That is why the same `.EXE` serves both.**

### Knocking on the door

And here comes the only ugly part of the whole module. IPX is not called like a
C function. It wants:

- The operation number in register **BX**
- The ECB's address in **ES:SI**
- And it is entered with a **far call** (`CALL FAR`), not an interrupt

**None of those three can be expressed in C.** C does not let you say "put this
in BX". Hence the assembler.

The hard one is the third. Think about it: the address to call is **in a
variable** (we saved it earlier). And on the 8086:

> **There is no "far call to the address in these two registers" instruction.**
> It does not exist. You can far call a constant address, or one held in
> memory, but not one held in registers.

The way round it is to push the address onto the stack and call **from the
stack**:

```asm
    push    cx              ; the driver's segment
    push    dx              ; the driver's offset
    mov     bp, sp          ; BP now points at those 4 bytes
    call    dword ptr [bp]  ; far call to the address held at [BP]
    add     sp, 4           ; and clean up what we pushed
```

`call dword ptr [bp]` means: "read 4 bytes from where BP points, treat them as
segment:offset, and call there". Since we just left the address exactly there,
it works.

### What gets saved, and why

The full block saves five registers:

```asm
    push si
    push di
    push ds
    push es
    push bp
    ...
    pop  bp
    pop  es
    pop  ds
    pop  di
    pop  si
```

Two deserve explanation:

**`BP`** — the compiler uses it to reach local variables and parameters. We
trample it with `mov bp, sp` to make the call, and on top of that the driver
may return it pointing anywhere. So it is restored **before** anything
BP-relative is touched again. If you look at `ipx_socket_call()`, the
`mov result_code, ax` comes **after** the `pop bp`, and that is not an
accident: before it, it would write into the middle of the stack.

**`DS`** — in the HUGE model the compiler assumes DS still points at this
module's data when the assembler block ends. The driver promises nothing of the
sort. If it changed DS and we did not restore it, **every global in `net.c`
would point somewhere else** from then on. That is a bug that would show up in
incomprehensible ways a long way from its cause.

> **Practical note:** if TCC ever objects to `call dword ptr [bp]`, that
> instruction can be written out in bytes by hand: `db 0FFh, 05Eh, 000h`. It is
> noted in `net.c` itself.

---

# PART 4 — THE CODE, IN EXECUTION ORDER

You have all the pieces now. Let us read `net.c` **in the order it runs**, not
the order it is written in.

## 12. Startup: `net_init()`

Called once, from `main()`, when you start with `/net`. It does six things:

### 1. Check the structures are the size they should be

```c
sprintf(net_log_text, "NET sizes: ecb=%u header=%u packet=%u (want 42/30/60)", ...);
tanks_log(net_log_text);
```

This looks trivial and it is one of the most useful things in the file.

The ECB has a size **fixed by Novell byte by byte**. If the compiler decided to
insert padding between fields to align them (which many compilers do), the
driver would read **every field from the wrong place** and nothing would work,
with no error message to explain it.

Turbo C aligns on bytes by default (`-a-`), so it comes out right. But writing
it in the log turns a baffling failure into an obvious line. If you ever see a
different number there, you know exactly what is going on.

### 2. Find the driver

`ipx_detect()`, from chapter 11.

### 3. Open the socket

```c
ipx_socket_call(IPX_FUNCTION_OPEN_SOCKET, net_swap16(NET_SOCKET_NUMBER));
```

A **socket** in IPX is like a port in TCP/IP: a number that identifies "this
conversation" within the machine. We tell the driver "anything arriving on
socket `0x869C`, give it to me".

Both machines use the same number, fixed in the code. It is what separates our
packets from anything else on the network.

### 4. Ask for our own address

```c
ipx_get_local_address();
```

Gives us our card's MAC. **It is only used for the log**, but it is a gold
plated diagnostic: if it comes back all zeros, it means no tunnel has assigned
us an address, which means we are not connected to anything. That is exactly
the symptom that showed up the first time this was tested.

### 5. Pick a random number

```c
srand((unsigned int)biostime(0, 0L));
local_instance_id = ((unsigned long)rand() << 16) | (unsigned long)rand();
```

A different number on each machine, drawn from the BIOS tick (which never
matches between two computers). It does two jobs:

- Telling our own packets apart from the other machine's.
- **Deciding who is player 1**, in chapter 13.

### 6. Put out four mailboxes

```c
index = 0;
while (index < NET_LISTEN_ECB_COUNT){
    listen_is_posted[index] = 0;
    net_post_listen(index);
    index = index + 1;
}
```

Here is something that surprises people at first:

> **To receive a packet you must have given the driver somewhere to put it,
> beforehand.**

It is not like reading a file, where you ask for data when you feel like it.
The driver needs a free buffer **in advance**. If a packet arrives and none is
waiting, it is lost.

That is why it gets **four** (`NET_LISTEN_ECB_COUNT`), not one. Think about
why: while we are processing a packet, that buffer is ours, not the driver's.
If there were only one, any packet arriving at that instant would be lost. With
four in rotation there is always a free one.

---

## 13. Finding each other: `net_find_opponent()`

Now we have to find the other machine. And here there is a design decision you
feel a lot when using it:

> **Nobody types an address anywhere.**

Not an IP, not a MAC, not a name. The two copies find each other on their own.

### How

With **broadcast**. Remember: the address `FF:FF:FF:FF:FF:FF` means "everyone
on this network". So both copies do the same thing:

1. Shout `HELLO` to everybody, every quarter of a second.
2. Answer `HELLO_ACK` to any `HELLO` they hear.
3. Consider themselves paired when they **have heard a HELLO AND have had their
   own answered**.

```
      MACHINE A                              MACHINE B

  HELLO --> to everyone  ----------------->  hears it
                                             notes its address and id
                                             sets "I heard him"  (bit 0x01)
                              <-- HELLO_ACK  answers him directly
  receives it
  sets "he answered me" (bit 0x02)
                                             HELLO --> to everyone
  hears it  <---------------------------------
  notes its address and id
  sets "I heard him"  (bit 0x01)
  HELLO_ACK -->  --------------------------> receives it
                                             sets "he answered me" (0x02)

  has both bits -> PAIRED                    has both bits -> PAIRED
```

### Why BOTH bits are needed

This part is subtle and it was a bug in the code before it was fixed.

The obvious thing would be to pair as soon as you hear a `HELLO`. **That will
not do.** Look at what would happen:

- A hears B's HELLO, considers itself paired, and **goes off to play**.
- On leaving, A stops sending HELLOs.
- B never heard one from A, and now never will.
- B keeps looking until it times out.

Requiring both bits guarantees that **when one starts playing, the other
already knows the game is on**. It is a two-way handshake, and it is the
minimum that works.

### Who is player 1

Settled **with absolutely nothing negotiated**:

```c
if (local_instance_id < remote_instance_id){
    is_player1 = 1;
}else{
    is_player1 = 0;
}
```

Each machine has both numbers (its own, and the other's which came in the
HELLO). It compares. **The lower one is player 1**, the tank at the bottom.

Both machines make the same comparison with the same two numbers, so they reach
the same conclusion on their own. Nobody has to decide and nobody has to be
told.

> **Why not use the MAC address, which is also unique?**
>
> Because the MAC comes from the driver, and if the driver answered that call
> badly (returning zeros, say), **both machines would think they were player 1**
> and the game would be nonsense. The `instance_id` does not depend on any call
> into the driver: we make it up ourselves. It is more robust exactly where it
> matters.

### Resetting the counter

Before leaving, both machines set `simulation_frame = 0` and clear the rings.
And there is a detail:

```c
index = 0;
while (index < NET_INPUT_DELAY){
    local_input_value[index]  = 0;
    remote_input_frame[index] = (unsigned long)index;
    remote_input_value[index] = 0;
    remote_input_valid[index] = 1;      // <-- marked as ALREADY received
    index = index + 1;
}
```

The first 5 frames are filled by hand with "no keys" and marked as received.
Why?

Because with a delay of 5, on frame 0 you send the keys for frame 5. **Nobody
ever sends the keys for frames 0 to 4**, because they are before the start.
Without this filling in, both machines would sit waiting forever for an input
for frame 0 that does not exist.

### Why there is no "press a key to start"

There was one, and **it was a mistake**. If a player is slow to press, their
machine is not servicing the network: its four mailboxes fill up, it stops
collecting packets, and the other machine sits waiting for frame 0 until it
times out.

In its place there is a **bounded two second pause** that keeps calling
`net_poll()`. Both start at frame 0; whichever gets there first waits for the
other, and lockstep handles it on its own as long as the wait is short.

---

## 14. The input ring

Before looking at sending and receiving, you need to know where the keys are
kept. Three arrays:

```c
static unsigned char local_input_value[NET_INPUT_BUFFER_SIZE];    // mine

static unsigned long remote_input_frame[NET_INPUT_BUFFER_SIZE];   // theirs
static unsigned char remote_input_value[NET_INPUT_BUFFER_SIZE];
static unsigned char remote_input_valid[NET_INPUT_BUFFER_SIZE];
```

`NET_INPUT_BUFFER_SIZE` is **64**. And here is the trick: the frame number
grows without end (0, 1, 2... 21000...), but the array has only 64 slots. So it
wraps around:

```c
index = (unsigned int)(frame & (NET_INPUT_BUFFER_SIZE - 1));
```

`frame & 63` is the same as `frame % 64`, but with an AND instead of a
division, which matters on a 486. That is why the size **has to be a power of
two**.

```
  frame:   0   1   2  ...  63  64  65  ... 127 128
  index:   0   1   2  ...  63   0   1  ...  63   0
                                 ^
                                 wraps here and overwrites frame 0's slot
```

It is a **ring buffer**: it holds the last 64 frames and overwrites the old
ones. With 64 slots and a delay of 5, there is masses of room.

### Why the remote one also stores the frame number

Notice that the other player's keys need **three** arrays and mine only one.
The reason is this:

If slot 10 holds `0x01`, is that from frame 10, from 74, or from 138? All three
land in the same slot. If we took whatever is there without checking, an old
entry from a previous lap would be mistaken for the one we are waiting for, and
**you would desync without noticing**.

So each slot stores which frame it belongs to, and reading it checks:

```c
if (remote_input_valid[index] == 0){ return 0; }
if (remote_input_frame[index] != simulation_frame){ return 0; }
return 1;
```

Mine do not need it because I write them myself 5 frames before reading them,
and 5 is much less than 64: there can never be any confusion.

---

## 15. Sending: `net_send_input()`

Called once a frame. It builds a packet with the last 8 frames of my keys.

```c
newest_frame = simulation_frame + NET_INPUT_DELAY;
```

If I am simulating frame 100, the newest one I have stored is 105 (because I
just put the current keys there).

```c
if (newest_frame + 1 < NET_REDUNDANCY){
    count = (unsigned int)(newest_frame + 1);
}else{
    count = NET_REDUNDANCY;
}

send_packet.payload.base_frame = newest_frame - (unsigned long)count + 1;
```

Normally `count` is 8 and `base_frame` is `105 - 8 + 1 = 98`. So the packet
carries frames **98 to 105**.

The `if` is for the very first frames of a game, when there are not yet 8
frames of history to look back on. On frame 0, `newest_frame` is 5, so it sends
6 frames (0 to 5) instead of 8.

Then it copies them out of the ring:

```c
entry = 0;
while (entry < count){
    frame = send_packet.payload.base_frame + (unsigned long)entry;
    index = (unsigned int)(frame & (NET_INPUT_BUFFER_SIZE - 1));
    send_packet.payload.inputs[entry] = local_input_value[index];
    entry = entry + 1;
}
```

### It never waits

```c
if (net_send_is_busy() == 1){
    return;
}
```

If the driver is still busy with the previous packet, **this frame sends
nothing**. It does not wait and it does not queue.

Is information lost? No: next frame's packet will carry frames 99 to 106, which
includes everything that was in the one that was not sent. **The redundancy
covers it.** It is the same idea as chapter 7, applied to our own side.

And notice the check comes **before building the packet**, not just before
sending it. That matters: while a send is in flight the buffer belongs to the
driver, and writing the next payload on top would rewrite a packet already on
its way.

---

## 16. Receiving: `net_poll()`

Collects whatever has arrived. It is **the only thing that moves data inwards**,
so any loop that waits has to call it or nothing will ever arrive.

```c
index = 0;
while (index < NET_LISTEN_ECB_COUNT){

    if (listen_is_posted[index] == 1){

        if (listen_ecb[index].in_use == 0){       // <-- the driver is done

            listen_is_posted[index] = 0;

            if (listen_ecb[index].completion_code == 0){
                if (net_packet_is_valid(&listen_packet[index]) == 1){
                    net_handle_packet(&listen_packet[index]);
                }
            }

            net_post_listen(index);                // <-- give the mailbox back
        }
    }

    index = index + 1;
}
```

It walks the four mailboxes. For each one:

1. Is it posted? (`listen_is_posted`)
2. Has the driver finished with it? (`in_use == 0`)
3. If so: process what it holds and **give it straight back to the driver**.

That last step is easy to forget and catastrophic: if you do not give the
mailboxes back, you run out of them and stop receiving forever.

### `net_packet_is_valid()`: the filter

```c
if (packet->payload.magic[0] != 'C'){ return 0; }
...
if (packet->payload.instance_id == local_instance_id){ return 0; }
```

Two filters:

**The `"CTRE"` mark.** All our packets start with those four bytes. Anything
reaching our socket without that mark is another program's traffic and is
thrown away. It is cheap and avoids surprises.

**Our own `instance_id`.** If our own broadcast came back to us, we ignore it.
On real Ethernet a card does not hear itself, so this hardly ever matters, but
checking costs nothing and it would prevent a very confusing bug: pairing with
yourself.

### `net_handle_packet()`: filing

For an `INPUT` packet, it stores each frame it carries:

```c
entry = 0;
while (entry < packet->payload.count){
    if (entry >= NET_REDUNDANCY){ break; }
    frame = packet->payload.base_frame + (unsigned long)entry;
    net_store_remote_input(frame, packet->payload.inputs[entry]);
    entry = entry + 1;
}
```

And `net_store_remote_input()` throws away what is useless:

```c
if (frame < simulation_frame){ return; }                           // already gone
if (frame >= simulation_frame + NET_INPUT_BUFFER_SIZE){ return; }  // too far ahead
```

- A frame **already simulated** is no use: we are not going back.
- A frame **too far ahead** would overwrite the slot of one we still need,
  because of the ring buffer.

Most of the time it will be filing duplicate data (because of the redundancy),
and that is fine: writing the same value again does no harm.

---

## 17. Waiting: `net_has_remote_input()`

```c
int net_has_remote_input(void){
    unsigned int index;
    index = (unsigned int)(simulation_frame & (NET_INPUT_BUFFER_SIZE - 1));

    if (remote_input_valid[index] == 0){ return 0; }
    if (remote_input_frame[index] != simulation_frame){ return 0; }
    return 1;
}
```

"Do I already have the other player's keys for the frame I am about to
simulate?"

While this returns 0, **the game cannot advance**. That is the "lock" in
"lockstep": both machines are chained to the same frame number.

In the main loop it is used like this:

```c
while (net_has_remote_input() == 0){
    net_poll();
    sound_update();

    if (net_connection_lost() == 1){
        connection_was_lost = 1;
        break;
    }
}
```

Three things about this loop:

**`net_poll()` inside.** Obvious but essential: if you do not call it, nothing
will ever arrive and you will wait forever.

**`sound_update()` inside.** Less obvious. If the game stops here for more than
a frame, the sound card runs out of fresh data and replays the last half of the
buffer, which comes out as a stutter. It is exactly the same reason
`sound_update()` sits outside the explosion-pause `if`: **the sound has to keep
running whatever happens**.

**The emergency exit.** If the other machine has died, this loop would have no
way out. `net_connection_lost()` checks whether 10 seconds have passed with
nothing received and breaks.

---

## 18. Closing the frame

At the end of the main loop:

```c
if (network_mode == 1){
    net_set_local_checksum(compute_state_checksum());
    net_advance_frame();
}
```

`net_advance_frame()` is one line: `simulation_frame + 1`. The checksum is
explained in chapter 24.

What matters is **where** it is: at the very end, after simulating and after
drawing. Both machines reach this line having computed exactly the same thing,
and only then do they move on to the next frame together.

---

# PART 5 — HOW IT PLUGS INTO THE GAME

This is the part that matters. You know how the network works inside; now let
us see how it connects to the game you already had.

## 19. The one real change

Out of the whole game, **one single thing really changed**. This:

```c
// BEFORE
void process_player_input(struct player *_player,
                          struct player *_other,
                          unsigned char key_up_code,
                          unsigned char key_down_code,
                          unsigned char key_left_code,
                          unsigned char key_right_code,
                          unsigned char key_fire_code){

    if (keys[key_up_code]){            // <-- reads the keyboard directly
        ...
```

```c
// NOW
void process_player_input(struct player *_player,
                          struct player *_other,
                          unsigned char input_bits){

    if (input_bits & NET_INPUT_UP){    // <-- reads a byte it is handed
        ...
```

That is all. The function no longer reads the keyboard: **it is handed the keys
already worked out**.

And that tiny difference changes everything, because now the function **cannot
know where those keys came from**. It genuinely does not care:

```
                      +---------------------------+
   your keyboard --->  |                           |
                      |  process_player_input()   |  ---> the tank moves
   the network   --->  |                           |
                      +---------------------------+
                         it cannot tell which one
                         it was, and does not need to
```

In jargon this is called **decoupling**: the function used to depend on the
keyboard and now depends on a byte. It is a small change in the code and an
enormous one in what it makes possible.

So that local mode keeps working, a function was added that does what
`process_player_input()` used to do itself:

```c
unsigned char read_input_from_keys(unsigned char key_up_code, ...){

    unsigned char input_bits;
    input_bits = 0;

    if (keys[key_up_code]){    input_bits = input_bits | NET_INPUT_UP;    }
    if (keys[key_down_code]){  input_bits = input_bits | NET_INPUT_DOWN;  }
    if (keys[key_left_code]){  input_bits = input_bits | NET_INPUT_LEFT;  }
    if (keys[key_right_code]){ input_bits = input_bits | NET_INPUT_RIGHT; }
    if (keys[key_fire_code]){  input_bits = input_bits | NET_INPUT_FIRE;  }

    return input_bits;
}
```

It is the **only** function in the game that still touches `keys[]`. Everything
else works on the byte.

And the main loop becomes this:

```c
if (network_mode == 1){
    ... get the two bytes: one mine, one from the network ...
}else{
    player1_input = read_input_from_keys(KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_NUMPAD_5);
    player2_input = read_input_from_keys(KEY_W,  KEY_S,    KEY_A,    KEY_D,     KEY_G);
}

process_player_input(&player1, &player2, player1_input);
process_player_input(&player2, &player1, player2_input);
```

The last two lines are **identical** in both modes. From there on, the game is
the game it always was.

---

## 20. The journey of one keypress, step by step

This chapter is the heart of the manual. We are going to follow **one single
keypress** from the moment you press it to the moment the tank moves **on both
screens**.

**Scenario:** you are at machine A, you got player 1 (the tank at the bottom),
and you press the **UP arrow** just as the game reaches **frame 100**.

### Step 1 — The hardware

You press the key. The keyboard sends scan code `0x48` and fires interrupt 9.
Your usual handler:

```c
void interrupt far new_kbd_handler() {
    ...
    keys[scancode] = 1;      // keys[0x48] = 1
    ...
}
```

Up to here **nothing has changed**. This is exactly what your game already did.

### Step 2 — The loop reads the keyboard (frame 100, machine A)

```c
local_input = read_input_from_keys(KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_NUMPAD_5);
```

`keys[0x48]` is 1, so `local_input` comes out as **`0x01`** (`NET_INPUT_UP`).

### Step 3 — It is filed FOR THE FUTURE

```c
net_set_local_input(local_input);
```

And inside:

```c
target_frame = simulation_frame + NET_INPUT_DELAY;    //  100 + 5 = 105
index = (unsigned int)(target_frame & 63);            //  105 & 63 = 41
local_input_value[index] = input_bits;                //  local_input_value[41] = 0x01
```

> **Look carefully: your key is NOT applied now.** It is stored in frame
> **105**'s slot. Your tank is not going to move yet.
>
> This is chapter 6. If it were applied now, you would desync.

### Step 4 — Out on the wire

```c
net_send_input();
```

It builds a packet carrying frames **98 to 105**:

```
   frame:  98    99   100   101   102   103   104   105
   value: 0x00  0x00  0x00  0x00  0x00  0x00  0x00  0x01
                                                      ^
                                            your keypress is here
```

The other seven are zero because you had not pressed anything. The whole packet
is 60 bytes and it heads off to the other machine.

### Step 5 — It arrives at machine B

A millisecond later (over a cable), B calls `net_poll()` in its loop, finds the
packet in one of its four mailboxes, and files it:

```c
net_store_remote_input(105, 0x01);
```

Which does:

```c
index = (unsigned int)(105 & 63);         //  41   <-- the SAME slot as on A
remote_input_frame[41] = 105;
remote_input_value[41] = 0x01;
remote_input_valid[41] = 1;
```

**Both machines now have the same byte in slot 41.** One in its "local" array,
the other in its "remote" array. But the same byte, for the same frame.

### Step 6 — Five frames of nothing happening

Frames 100, 101, 102, 103, 104. On **both** machines:

```c
player1_input = ... the byte for the current frame ...   // it is 0x00
process_player_input(&player1, &player2, 0x00);          // no keys: no movement
```

Your tank sits still on your screen. You pressed and nothing happens.

**Those are the 71 ms.** Five frames × 14.3 ms. It is the price, and chapter 6
showed why it has to be paid.

### Step 7 — Frame 105: both machines, together

Here is the lovely bit. Both reach frame 105 and do this:

```
MACHINE A (you, player 1)              MACHINE B (the other, player 2)
-------------------------              -------------------------------
index = 105 & 63 = 41                  index = 105 & 63 = 41

net_get_local_input()                  net_get_remote_input()
  -> local_input_value[41]               -> remote_input_value[41]
  -> 0x01                                -> 0x01

local_player_is_1 == 1, so:            local_player_is_1 == 0, so:
  player1_input = 0x01   <-- mine        player1_input = 0x01   <-- from the wire
  player2_input = (his)                  player2_input = (his)
```

**Both arrived at the same `player1_input = 0x01`.** One took it off its
keyboard five frames ago; the other received it over the cable. It makes no
difference: it is the same byte, on the same frame.

### Step 8 — The game as it always was

And now, on both machines, exactly the same code runs:

```c
process_player_input(&player1, &player2, 0x01);
```

Inside:

```c
if (input_bits & NET_INPUT_UP){                              // 0x01 & 0x01 -> yes

    is_driving = 1;
    player_update_future_collision_points(_player, MOVE_UP); // where would it land?

    if (is_move_blocked(_player, _other) == 0){              // wall or tank there?
        _player->is_moving = 1;
        move_sprite(_player, MOVE_UP);                       // position_y -= 2
    }
}
```

And here is the key to the whole design:

> **`is_move_blocked()` reads the same collision map, with the tank in the same
> position, on both machines. So it returns the same thing. And `move_sprite()`
> subtracts the same 2 pixels.**

If `position_y` was 164, on both machines it becomes **162**. Not
"approximately 162". **162.**

### The whole journey at a glance

```
   YOU                                                   THE OTHER
   ===                                                   =========

   UP key
      |
      v
   INT 9 -> keys[0x48] = 1
      |
      v
   frame 100: read_input_from_keys() -> 0x01
      |
      +--> local_input_value[41] = 0x01      (for frame 105)
      |
      +--> packet with frames 98..105 ---------> (1 ms) ---> net_poll()
                                                                 |
                                                                 v
                                                  remote_input_value[41] = 0x01
   frames 100-104: still                         frames 100-104: still
      |                                                          |
      v                                                          v
   frame 105                                     frame 105
   net_get_local_input() -> 0x01                 net_get_remote_input() -> 0x01
      |                                                          |
      +---------------> player1_input = 0x01 <-------------------+
                                |
                                v
              process_player_input(&player1, &player2, 0x01)
                                |
                                v
                   move_sprite(&player1, MOVE_UP)
                                |
                                v
                    player1.position_y: 164 -> 162
                         ON BOTH MACHINES
```

If you have understood that diagram, you have understood networked play.
Everything else is plumbing.

---

## 21. Who controls which tank

There is an easy confusion to fall into here, so let us go slowly.

There are **two pairs of concepts** that are not the same thing:

| | |
|---|---|
| **`player1` / `player2`** | The two tanks **in the simulation**. They exist identically on both machines. `player1` is always the bottom one and `player2` always the top one, on both computers |
| **local / remote** | Which **keyboard** a byte came from. "Local" is the one in front of me; "remote" is the one that arrived over the wire |

The question is: which tank does my keyboard move? And the answer comes from
`is_player1`, decided during pairing:

```c
if (local_player_is_1 == 1){
    player1_input = net_get_local_input();     // my keyboard moves the bottom one
    player2_input = net_get_remote_input();
}else{
    player1_input = net_get_remote_input();
    player2_input = net_get_local_input();     // my keyboard moves the top one
}
```

As a picture:

```
        MACHINE A (got player 1)            MACHINE B (got player 2)

  my keyboard  --------> player1        my keyboard  --------> player2
                       (the bottom one)                      (the top one)

  the network  --------> player2        the network  --------> player1


        BOTH simulate player1 AND player2. The only thing that differs
        is which of the two each machine feeds from its own keyboard.
```

**The crossover is the whole trick.** What is "mine" on one machine is "his" on
the other, and vice versa. But both tanks are fully simulated on both.

### Three practical consequences

**1. On the network, you both use the arrow keys.**

In local mode player 1 uses the arrows and player 2 uses WASD, because they
share a keyboard. On the network each has a whole keyboard, so both use the
same keys:

```c
local_input = read_input_from_keys(KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_NUMPAD_5);
```

The WASD keys **are not read in network mode**. It makes no difference which
tank you got: you always use the arrows and keypad 5.

**2. You might get the top tank.**

It is decided at random (by the `instance_id`), so there is no way to choose.
The game tells you on the text screen before starting:

```
You are PLAYER 1, the tank at the bottom.
```

**3. "Server" has nothing to do with this.**

Whoever runs `launch_game_server.sh` may perfectly well end up as player 2. The
server/client thing is only for bringing up DOSBox's tunnel; once connected,
both machines are identical.

---

## 22. Everything that did NOT have to be touched

It is worth seeing written down, because it is the reward for having chosen
lockstep:

```
is_blocked_by_wall()          update_bullet()           draw_to_buffer()
is_blocked_by_tank()          bullet_has_hit_tank()     draw_explosion()
is_move_blocked()             move_sprite()             update_game()
restart_game()                update_player_animation()

...and ALL of players.c, ALL of sound.c, ALL of bmp.c
```

Not one line.

Why? Because **those functions never knew where the keys came from**. They just
moved a tank, checked a collision or painted a sprite. And they still do
exactly that.

Compared with the obvious idea in chapter 2, where the whole `struct player`
would have to be serialised and the packer updated every time a field is added,
the difference is enormous.

> **The moral, which applies to any project:** the architecture did the work,
> not the network code. `net.c` moves one byte; what makes it work is that the
> game was deterministic and that input could be decoupled from the keyboard
> with a one line change.

---

## 23. Two details that are not optional

Two things about the wiring that look minor and are not.

### Input is exchanged ALWAYS, including during the explosion

Look at where the network block sits in the loop:

```c
do {
    // 0. THE NETWORK, ALWAYS
    if (network_mode == 1){
        ... send, wait, hand out ...
    }else{
        ... read the keyboard ...
    }

    // 1. and NOW the two states of the round
    if (explosion_pause_counter == 0){
        ... playing: keys, bullets, collisions ...
    }else{
        ... burning: only the explosion advances ...
    }
    ...
} while(!keys[KEY_ESC]);
```

The network block is **outside and before** the two-state `if`. It has to be.

During the half second the explosion lasts, nothing moves on screen except the
fire. But **the frame number keeps advancing**. If we stopped sending and
receiving during that time, the other machine would sit waiting for an input
for a frame nobody sent it, and would stall until it timed out.

Put another way: **lockstep cannot be paused.** Even when the game is
"stopped", both machines have to keep stepping through the same frame numbers,
even if what they send is "I pressed nothing".

### The sound needs no network (and this is lovely)

Not one byte about sound is sent. Not "I fired", not "play the explosion".
Nothing.

And yet it sounds right on both machines, at the same time. Why?

Because **the sound is triggered by the simulation, and both machines simulate
the same thing**:

```c
if (player_fire_bullet(_player) == 1){
    sound_play(_player->sound_fire_voice, SOUND_SAMPLE_FIRE, SOUND_VOLUME_FIRE);
}
```

On frame 105, both machines run `player_fire_bullet()` with the same input byte
and the same state. Both return 1. Both call `sound_play()`. **On the same
frame, on their own, without having discussed it.**

The same goes for the explosion, the engine and the hit.

This is a free consequence of determinism, and it is a good example of why this
architecture is so comfortable: **anything that depends only on the game state
synchronises itself.** If you add a power-up tomorrow that makes a sound,
nothing will need sending for that either.

The only thing that did have to be added is the `sound_update()` call inside the
waiting loop (chapter 17), and not to synchronise anything: just to avoid
leaving the card without data while the game is stopped.

---

# PART 6 — WHEN THINGS GO WRONG

## 24. Desync: the invisible failure

This is **the** characteristic failure of lockstep, and it is worth
understanding well because it is treacherous.

### What it is

The two machines stop computing the same thing. From that moment on, each one
carries on playing **its own game**.

### Why it is so bad

Because **you cannot see it**. Think about it: each screen carries on showing a
perfectly coherent game. The tanks move properly, the bullets fly properly, the
collisions work. They are simply **different games**.

You see that you hit him. He sees that you missed. And there is no error
message, no crash, nothing. You could chase that for days without knowing where
to start.

### What would cause it

Anything that breaks the determinism of chapter 4:

- putting a `float` into the simulation
- calling `rand()`
- anything depending on the clock or on how fast the machine is
- reading uninitialised memory
- **applying an input on different frames** on each machine (chapter 6's
  mistake)
- a bug in the ring buffer that confuses an old frame for a new one

### How we detect it

Every 30 frames, each machine works out a number that sums up **all** of its
state:

```c
unsigned int compute_state_checksum(){

    unsigned int checksum;
    checksum = 0;

    checksum = checksum + (player1.position_x * 3);
    checksum = checksum + (player1.position_y * 5);
    checksum = checksum + (player1.current_direction * 7);
    checksum = checksum + (player1.bullet_position_x * 11);
    ...
    checksum = checksum + (explosion_pause_counter * 71);

    return checksum;
}
```

It is called a **checksum**: a short number standing for a lot of data. If two
checksums differ, the data differs, guaranteed.

That number travels in the next packet, along with the frame it belongs to. The
other machine compares it with its own for that frame, and if they do not
match:

```
NET DESYNC at frame 1830: mine 41234 theirs 41199
```

**And there you have the exact frame it broke on.** That turns a days-long
mystery into a bounded problem.

### The details of the checksum

**Why the multipliers 3, 5, 7, 11...?** So things cannot cancel out. If we
added the values raw, swapping the two tanks' X values would give **the same
total** and we would not catch the fault. With different multipliers, each
field contributes differently.

**Does it not overflow?** Yes, `unsigned int` wraps constantly. **And it does
not matter**: it wraps exactly the same way on both machines, because unsigned
overflow is defined in C and is deterministic. All we need is that two
different states almost always give different numbers.

**Why is it in `main.c` and not `net.c`?** Because `net.c` does not know what a
tank is, and should not. `main.c` works the number out and hands it over.

**Why keep 64 of them?** Because the checksum travels in a packet that can be
delayed, and when it arrives it has to be compared against **ours for that same
frame**, not the current one. The last 64 are kept in a ring, like the inputs.

---

## 25. Disconnection

The other possible failure: the other machine vanishes. DOSBox is closed, the
wifi drops, the cable is pulled.

Unprotected, chapter 17's loop would wait **forever**, with the game frozen and
no explanation.

```c
int net_connection_lost(void){

    long now_tick;
    ...
    now_tick = biostime(0, 0L);

    if (now_tick - last_packet_tick > NET_TIMEOUT_SECONDS * NET_TICKS_PER_SECOND){
        sprintf(net_log_text, "NET: connection lost at frame %lu", simulation_frame);
        tanks_log(net_log_text);
        connection_lost = 1;
        return 1;
    }

    return 0;
}
```

`last_packet_tick` is updated every time **any** valid packet arrives. If 10
seconds pass without one, the connection is given up for dead and the game
exits cleanly: back to text mode, telling you what happened.

Ten seconds is deliberately generous. A two or three second network hiccup is
annoying but survivable; we do not want to end a game over that.

---

## 26. What to look for in the log

All of this goes to `k:\game.log`, because in graphics mode nothing can be
printed.

| Line | What it means |
|---|---|
| `NET sizes: ecb=42 header=30 packet=60` | Correct. **Any other number** = the compiler padded the structures and IPX reads everything wrong |
| `NET: no IPX driver found` | No driver. In DOSBox, `ipx=true` is missing |
| `NET: IPX driver entry at F000:1680` | The driver is there and we know where to call it |
| `NET: node 000000000000` | **Bad sign.** An all-zero node = no tunnel gave us an address: we are not connected |
| `NET: node 7F000001AF6E id 304509028` | Good. There is our address and our random number |
| `NET: paired. them id ... we are player 1` | Paired, and which tank we got |
| `NET: discovery timed out` | Nobody answered in 30 seconds |
| `NET DESYNC at frame N` | Chapter 24 |
| `NET: connection lost at frame N` | Chapter 25 |
| `NET: sent 21007, received 20984, waited 11403 frames` | Summary on the way out |

### How to read the final summary

It is the most useful line of all.

**`sent` / `received`.** A small difference (under 1%) is normal: those are the
ones still in the air when you quit, plus the odd lost one. The redundancy
covers them. If `received` were **0**, nothing is arriving: check the all-zero
node line.

**`waited`.** The frames the game spent **stopped**, waiting for the other
machine. It has **two different causes** and confusing them makes you reach for
the wrong knob:

| What you see | What it is | What to do |
|---|---|---|
| Close to 0 | Everything has plenty of room | Nothing |
| A few dozen, in bursts | Network **jitter**: packets arriving at irregular times | Raise `NET_INPUT_DELAY` |
| A large, constant fraction (half of them, say) | The two machines **are not running at the same speed**, and the faster is being paced by the slower | Lower `cycles` on **both** |

The second case is important and counterintuitive: **raising
`NET_INPUT_DELAY` does not fix it**. The delay is a cushion that absorbs
*variation*; it cannot absorb a permanent speed difference, any more than a
tank absorbs a tap that delivers less than you draw. It would only add latency.

To find out which one is the slow one, compare `waited` in the two logs: **the
slow one will be close to zero**, because it never has to wait for anybody.

---

# PART 7 — LEARNING BY POKING AT IT

## 27. Experiments

The best way to understand this is to break it on purpose. All of these are
safe: they undo by changing the number back.

### Experiment 1: feel the input delay

In `header/net.h`, set:

```c
#define NET_INPUT_DELAY 	30
```

Rebuild and play. **Half a second between pressing and the tank moving.** It is
grotesque, but it makes you *feel* what input delay is and why 5 is a chosen
number and not an arbitrary one.

Now try `1`. It feels perfect... until the network has a bad moment and the
game lurches. `waited` goes up in the log.

### Experiment 2: break determinism on purpose

**This is the most instructive of all.** In `move_sprite()`, add this:

```c
if (rand() % 200 == 0){
    _player->position_x = _player->position_x + 1;
}
```

One extra pixel, very occasionally, at random.

Play over the network and watch the log. Within seconds:

```
NET DESYNC at frame 312: mine 28471 theirs 28399
```

The two machines draw different numbers from `rand()`, so a tank shifts one
pixel on one machine and not on the other. **And from there the two games
diverge beyond repair.**

The interesting part: watch both screens while it happens. **They both still
look correct.** Nothing breaks, nothing crashes. They are just different games.
That is exactly what chapter 24 wanted you to understand, and seeing it with
your own eyes is worth more than reading it.

Then remove the `rand()` and check that the `DESYNC` goes away.

### Experiment 3: remove the redundancy

```c
#define NET_REDUNDANCY 		1
```

Now each packet carries only the current frame, with no spare. On a cable you
will notice nothing (almost nothing gets lost). Over wifi you will see hitches
and `waited` will climb a lot. It is the demonstration of why the redundancy
was money well spent.

### Experiment 4: change the socket on one machine only

In `net.c`, on one of the two:

```c
#define NET_SOCKET_NUMBER 		0x869D      // one higher
```

Both start fine, the tunnel comes up fine, `ipxnet ping` works... and **they
never find each other**. `NET: discovery timed out`.

It shows what a socket is: the driver only hands each packet to whoever is
listening on that exact number. It is the equivalent of a port.

### Experiment 5: see lockstep with your own eyes

Start both instances and **slow one right down**: in DOSBox, press `Ctrl+F11`
several times to drop its cycles to almost nothing.

You will see **the other one slow down too**. It does not run ahead, it does
not carry on alone: it waits. That is literally the "lock" in lockstep, and
watching it happen live explains it better than any paragraph.

`Ctrl+F12` puts the cycles back and both recover.

### Experiment 6: watch the log live

In another terminal, while you play:

```
tail -f net-test/log-server/GAME.LOG | grep --line-buffered "NET\|Tank hit"
```

You will see the pairing and the hits as they happen.

---

## 28. Glossary

Terms that appear in the manual and in the code, in the order you need them.

**Packet** — A block of bytes sent over the network in one go. Ours is 60
bytes: 30 of IPX header and 30 of ours.

**Protocol** — The agreement about what those bytes mean. IPX is a protocol;
"the first four bytes are `CTRE`" is part of ours.

**Datagram** — A standalone packet, with no connection and no guarantees. You
send it and that is that. The opposite would be a stream (like TCP), where the
protocol makes sure everything arrives and in order.

**Node** — A machine's address in IPX: 6 bytes, which are the card's MAC.

**MAC** — The unique number every network card carries from the factory.

**Broadcast** — Sending to every machine on the network at once, using the
address `FF:FF:FF:FF:FF:FF`.

**Socket** — A number identifying one conversation within a machine. The
equivalent of a port in TCP/IP. We use `0x869C`.

**ECB** (*Event Control Block*) — The structure you fill in for the IPX driver
to ask it to send or receive. Chapter 10's "form".

**TSR** (*Terminate and Stay Resident*) — A DOS program that stays in memory
after exiting, so others can call it. The IPX driver is one.

**Latency** — How long a packet takes to arrive. Over a cable, tenths of a
millisecond; over wifi, a few milliseconds.

**Jitter** — The *variation* in latency. It is worse than latency itself: high
but constant latency is compensated by the input delay; latency jumping from
2 ms to 90 ms is not.

**Deterministic** — Given the same inputs, always produces exactly the same
result. The property of your game that makes all of this possible.

**Lockstep** — This game's architecture: both machines simulate everything and
only exchange inputs, advancing frame by frame together.

**Input delay** — The frames your own keys are held back before being applied,
so they are applied at the same time as the other player's. Here, 5.

**Redundancy** — Sending the last N frames of keys in every packet, so a loss
does not stop the game. Here, 8.

**Ring buffer** — An array reused by wrapping around, using `frame & 63` as the
index. Holds the last 64 frames.

**Polling** — Asking "is there anything?" from time to time, rather than being
told by an interrupt. It is what `net_poll()` does, and it is deliberate.

**Checksum** — A short number summarising a lot of data. If two checksums
differ, the data differs.

**Desync** — When the two machines stop computing the same thing. The
characteristic failure of lockstep.

**Big endian / little endian** — Which order the bytes of a number are stored
in. IPX uses big endian (big byte first), the 8086 little endian. Hence
`net_swap16()`.

---

## And that is it

If you got this far, you now know:

- why sending positions is a bad idea (ch. 2)
- why your game can do lockstep and many cannot (ch. 4)
- why your own keys are delayed (ch. 6)
- why the redundancy is free (ch. 7)
- what IPX is and why not TCP/IP (ch. 9)
- how you talk to the driver (ch. 10-11)
- what each function does and in what order (ch. 12-18)
- **how a keypress travels from your finger to both screens (ch. 20)**
- how anything that goes wrong is detected and diagnosed (ch. 24-26)

To look individual things up, [`NETWORK.md`](NETWORK.md) has the reference for
every function with its line number. To set up the two machines,
[`NETWORK-TESTING.md`](NETWORK-TESTING.md).
