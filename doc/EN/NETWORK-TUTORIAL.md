# Tutorial: using the network module in any DOS project

`src/net.c` is a **standalone module**. It knows nothing about tanks, about
games, or about what you are sending: you hand it a few bytes, they come out
the other side.

This document explains how to use it in a new program: a chat, a file
transfer, two programs talking to each other, or another game. If what you
want is to understand how it works inside (IPX, the ECBs, lockstep), that is
in [NETWORK-MANUAL.md](NETWORK-MANUAL.md).

> **Would you rather see it running first?**
> The [course](../../tutorial/README-EN.md) has three runnable chapters on this
> API:
> [**ch15**](../../tutorial/ch15/doc/README-EN.md) finding IPX ·
> [**ch16**](../../tutorial/ch16/doc/README-EN.md) pairing up ·
> [**ch17**](../../tutorial/ch17/doc/README-EN.md) a complete chat.

---

## 1. What do I copy into my project

**Two files, and nothing else:**

```
src/net.c
header/net.h
```

They depend on no other file in this repository. Their only dependencies are
standard Turbo C headers:

```c
#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <bios.h>
#include <string.h>
#include <stdlib.h>
```

To build it, one line in your Makefile like any other:

```
tcc -c -O2 -mh -Iheader -obin\net.obj src\net.c
```

And add `bin\net.obj` to the link list.

> **`src/lockstep.c` is NOT part of the library.** It is this game's way of
> using it, and it is a separate file precisely so that copying `net.c` into a
> chat does not drag frames, checksums and ring buffers along with it. Ignore
> it unless you are writing a two player action game; section 10 explains what
> it is for.

---

## 2. The five steps

It is always these five, in this order. There is nothing more to it.

```c
#include "header\net.h"

int main()
{
    char message[100];
    int  length;

    /* ---- STEP 1: switch the network on ---- */
    if (net_start() == 1){

        /* ---- STEP 2: find the other machine ---- */
        net_connect_to_server(30);

    }

    while (running){

        /* ---- STEP 3: once per loop, ALWAYS ---- */
        net_update();

        /* ---- STEP 4: say something, and listen ---- */
        if (something_happened){
            net_send("hello", 5);
        }

        length = net_receive(message, sizeof(message));

        while (length > 0){
            /* ...do something with the first `length` bytes... */
            length = net_receive(message, sizeof(message));
        }

    }

    /* ---- STEP 5: switch it off ---- */
    net_end();

    return 0;
}
```

And that is it. That is a program that talks to another machine.

### The five steps, one at a time

**`net_start()`** finds the IPX driver and opens our socket. It returns **1 if
there is network and 0 if there is not** (no driver loaded, or another copy of
your program already has the socket).

The important part: if it returns 0, **you do not have to do anything
special**. Every other function checks for itself and quietly does nothing.
Your program runs exactly the same, on its own, without a single extra `if`
scattered through your code.

It connects to nobody. Who talks to whom is step 2.

**`net_update()` IS NOT OPTIONAL.** It is the one of the five you can forget
and break everything with. It is the **only** thing that ever moves data in:
while it is not being called, nothing arrives, however much the other side
sends.

Call it inside **any loop that waits** for something too. It is cheap: almost
every time it looks, sees the driver has nothing, and returns.

**`net_send()`** hands your bytes to the driver and returns straight away. It
**never waits**, so it can return 0 meaning "not sent". See section 5.

**`net_receive()`** gives you **one** message and returns how long it was, or
**0** when there is nothing waiting. Call it in a loop until it returns 0:
several messages can arrive between two turns round yours.

**`net_end()`** is required before leaving. Skip it and the driver keeps our
socket and our buffers, and the next run cannot open the same socket.

---

## 3. Connecting: who is the server?

Here is the honest bit, and it is worth thirty seconds of your time.

**IPX has no such thing as a server or a client.** There is no connection to
open and none to accept. It is closer to shouting across a room than to
picking up a telephone.

But you almost always want to *think* in those terms, so the library gives you
them as a **convention**. The only difference between the two is **who shouts
and who listens**:

| | What it actually does |
|---|---|
| `net_wait_for_client(30)` | Stays quiet and waits. Answers whoever turns up |
| `net_connect_to_server(30)` | Shouts four times a second until somebody answers |
| `net_find_peer(30)` | Both sides shout AND answer. Nobody is in charge |

The number is **how many seconds** to keep trying before giving up. All three
return **1 when they have found the other machine** and 0 on timeout or if the
user pressed a key.

**Nobody types an address anywhere.** They find each other by broadcast. That
is genuinely nicer than TCP/IP, where somebody has to know an IP.

Once they have found each other **the two sides are completely identical**.
Either can send whenever it likes. "Server" and "client" only ever described
the first two seconds.

### Which one do I use?

- Writing a **tool**, where one machine is clearly the one being connected
  *to* (a file receiver, a print server, a chat host): use
  `net_wait_for_client()` on that side and `net_connect_to_server()` on the
  other.
- Writing a **game**, where both copies are the same program and neither has
  any reason to be in charge: use `net_find_peer()` on both. Neither player
  has to remember to start first.

All three print what they are doing, so **call them before you switch to
graphics mode**.

### And who gets to decide things?

Sooner or later one side has to be "the first one" for something: who is
player 1, who sends first, who picks the level. There is a trick for this that
costs no packets at all:

```c
if (net_get_local_id() < net_get_remote_id()){
    /* I go first */
}else{
    /* they go first */
}
```

Each copy picks a random number at `net_start()`, and both numbers are
exchanged while pairing. **Both machines compare the same two numbers and
reach the same answer on their own**, with nothing negotiated and nothing
sent. That is how this game settles who drives which tank.

---

## 4. What travels: messages, not a stream

This is the single most important idea in the whole library.

**One `net_send()` is one MESSAGE.** If you send 3 bytes, the other side gets
exactly 3 bytes in one `net_receive()`.

```c
    net_send("abc", 3);
    net_send("de", 2);
```

arrives as **`abc`** and then **`de`**. Never as `abcde`, and never as `ab`
plus `cde`.

If you have used TCP sockets, this is the opposite of what you are used to,
and it is **much easier**: there is no stream to cut back up, no length prefix
to write, no leftover half message to remember until next time.

So the natural way to use it is to send a `struct`:

```c
struct my_message {
    unsigned char  what_happened;
    unsigned int   x;
    unsigned int   y;
};

    struct my_message out;
    struct my_message in;

    out.what_happened = 7;
    out.x = 100;
    out.y = 50;

    net_send(&out, sizeof(struct my_message));

    /* ...and on the other machine... */

    if (net_receive(&in, sizeof(struct my_message)) == sizeof(struct my_message)){
        /* in.x and in.y are ready to use */
    }
```

Both programs are compiled by the same Turbo C with the same options, so the
structure has the same layout on both sides and there is nothing to convert.

**Check what `net_receive()` returned.** If it is not the size you expected,
somebody else is using the socket, or you sent a different kind of message.
Reading the fields of something that was never yours is how you get a bug that
takes a week.

### If you send more than one kind of message

Put a type byte at the front and look at it first. Exactly what the library
does internally:

```c
#define MSG_CHAT     1
#define MSG_FILE     2
#define MSG_GOODBYE  3

struct my_message {
    unsigned char type;
    unsigned char data[100];
};
```

### The limits

| | |
|---|---|
| Most in one message | **`NET_MAX_DATA`, 480 bytes** |
| Messages waiting to be collected | **`NET_QUEUE_SIZE`, 8** |
| What it costs in memory | about **6 KB**, whatever you do |

480 is not arbitrary: IPX guarantees 546 bytes for the whole packet, its own
header takes 30, and our envelope takes 12.

Ask to send more and `net_send()` **refuses and returns 0** rather than
quietly chopping your data in half. Split it yourself, as section 6 does with
a file.

---

## 5. What you do NOT get

The library gives you no more than IPX does, and being straight about that is
worth more than a comfortable lie:

| | |
|---|---|
| Does it arrive? | **Not guaranteed.** A message can be lost and nobody notices |
| In order? | **Not guaranteed.** Two can arrive the other way round |
| Is there a connection? | **No.** "Connected" only means they have found each other |

On a LAN, and on DOSBox, losses are rare. Whether that matters depends
entirely on what you are sending:

**It does not matter** for anything you send over and over: a position, a
score, keys held this frame. The next one along fixes it. Waiting for a
retransmission would be worse than the loss.

**It matters a great deal** for anything sent once: a file, a "the game is
over" message, a chat line. Losing it loses it for good.

There are also **two more ways a message can go missing**, and these are yours
to deal with:

**`net_send()` returned 0.** The driver was still busy with the previous
packet. On a fast loop this happens now and again and is perfectly normal:

```c
    if (net_send(&message, sizeof(message)) == 0){
        /* not sent. Try again next loop if it matters */
    }
```

**You did not read fast enough.** Only `NET_QUEUE_SIZE` messages are kept
waiting. If more pile up, the **oldest** is thrown away, because on a link
falling behind the newest news is the news worth keeping. Read your messages
in a loop, every loop, and this never happens.

### How to fix it when it does matter

The cure is the same one TCP uses, and here it is about twenty lines. **Number
your messages and have the other side say what it got:**

```c
struct my_message {
    unsigned int  number;              /* 0, 1, 2, ... */
    unsigned char data[400];
};

struct my_ack {
    unsigned int  number;              /* "I have got this one" */
};
```

**The sender** keeps a copy of the message it last sent, resends it every so
often, and only moves on to the next when the acknowledgement for that number
arrives:

```c
    while (there_is_more_to_send){

        net_update();

        /* resend about four times a second until they say they got it */
        if (biostime(0, 0L) >= next_resend){
            next_resend = biostime(0, 0L) + 5L;
            net_send(&out, sizeof(out));
        }

        if (net_receive(&ack, sizeof(ack)) == sizeof(ack)){
            if (ack.number == out.number){
                /* got it. Fill in the next message and move on */
                out.number = out.number + 1;
                ...
            }
        }

        if (net_connection_lost() == 1){
            break;
        }

    }
```

**The receiver** acknowledges everything and ignores anything it has already
seen:

```c
    if (net_receive(&in, sizeof(in)) == sizeof(in)){

        ack.number = in.number;
        net_send(&ack, sizeof(ack));       /* always, even for a repeat */

        if (in.number == expected){
            /* new. Use it */
            expected = expected + 1;
        }

        /* if it is not the one expected it is a repeat: acknowledged and
           thrown away. That is what makes a lost acknowledgement harmless */

    }
```

That handles a lost message (it is resent), a lost acknowledgement (the repeat
is acknowledged again), and messages arriving out of order (only the expected
number is used). It is slow — one message in the air at a time — but for
sending a file over a LAN it is perfectly fine, and it is *correct*, which
matters more.

---

## 6. A complete program: a two machine chat

This compiles and works as it is. Run it on two machines: on one type
`chat s`, on the other `chat c`.

```c
#include <stdio.h>
#include <conio.h>
#include <string.h>
#include "header\net.h"

#define CHAT_LINE_LENGTH 	80

int main(int argc, char *argv[])
{
    char sent_line[CHAT_LINE_LENGTH];
    char received_line[CHAT_LINE_LENGTH];
    int  typed_length;
    int  received_length;
    int  key;
    int  connected;

    if (net_start() == 0){
        printf("No IPX driver. Load one and try again.\n");
        return 1;
    }

    if (argc > 1 && argv[1][0] == 's'){
        connected = net_wait_for_client(60);
    }else{
        connected = net_connect_to_server(60);
    }

    if (connected == 0){
        net_end();
        return 1;
    }

    printf("Type and press ENTER. ESC to leave.\n\n");

    typed_length = 0;

    while (1){

        /* ALWAYS, on every pass */
        net_update();

        /* ---- anything they said? ---- */

        received_length = net_receive(received_line, CHAT_LINE_LENGTH - 1);

        while (received_length > 0){

            received_line[received_length] = '\0';
            printf("\nTHEM: %s\n", received_line);

            received_length = net_receive(received_line, CHAT_LINE_LENGTH - 1);

        }

        /* ---- anything we are typing? ---- */

        if (kbhit()){

            key = getch();

            if (key == 27){                              /* ESC */
                break;
            }

            if (key == 13){                              /* ENTER */

                if (typed_length > 0){

                    if (net_send(sent_line, typed_length) == 0){
                        printf("\n(busy, not sent)\n");
                    }

                    typed_length = 0;
                    printf("\n");

                }

            }else{

                if (typed_length < CHAT_LINE_LENGTH - 1){
                    sent_line[typed_length] = (char)key;
                    typed_length = typed_length + 1;
                    putch(key);
                }

            }

        }

        /* ---- have they gone? ---- */

        if (net_connection_lost() == 1){
            printf("\n\nThe other machine has gone.\n");
            break;
        }

    }

    net_end();

    return 0;
}
```

Notice three things:

1. **`net_update()` is outside every `if`.** It has to run on every pass.
2. **`net_receive()` is in a `while`, not an `if`.** Two lines can arrive
   between two keystrokes.
3. **`net_connection_lost()` is what gets you out.** Without it the program
   would sit there for ever if the other machine crashed.

A chat is the one case where you should think about section 5: a lost line is
lost silently. On a LAN it will practically never happen, and if it worries
you, the numbering from section 5 is the answer.

---

## 7. Sending a file

Same library, nothing new, just the pieces put together. **Cut the file into
messages, number them, and say when it ends.**

```c
#define FILE_CHUNK 		400

#define MSG_NAME 		1
#define MSG_CHUNK 		2
#define MSG_END 		3

struct file_message {
    unsigned char  type;
    unsigned char  padding;
    unsigned int   number;
    unsigned int   length;
    unsigned char  data[FILE_CHUNK];
};
```

**Sending**, in outline:

```c
    file = fopen("picture.bmp", "rb");

    /* the name first, so the other side knows what it is writing */
    message.type = MSG_NAME;
    strcpy((char *)message.data, "picture.bmp");
    message.length = strlen("picture.bmp");
    send_and_wait_for_ack(&message);

    /* then the file, 400 bytes at a time */
    message.number = 0;

    read_bytes = fread(message.data, 1, FILE_CHUNK, file);

    while (read_bytes > 0){

        message.type   = MSG_CHUNK;
        message.length = read_bytes;

        send_and_wait_for_ack(&message);

        message.number = message.number + 1;

        read_bytes = fread(message.data, 1, FILE_CHUNK, file);

    }

    message.type = MSG_END;
    send_and_wait_for_ack(&message);

    fclose(file);
```

where `send_and_wait_for_ack()` is the resend loop from section 5. **Use it
here.** A file with a hole in the middle is not a file, and this is exactly
the case where "a message can be lost" stops being acceptable.

**Receiving** is the mirror image: acknowledge everything, write to disk only
the numbers you have not seen before, and stop on `MSG_END`.

At 400 bytes a message and one message in the air at a time, a floppy sized
file takes a while. If that bothers you, send several and acknowledge the
highest number received in order — that is a sliding window, and it is how
TCP gets its speed. But get the simple one working first.

---

## 8. Full reference

| Function | What it does |
| --- | --- |
| `net_start()` | Switches the network on. **1 = there is network, 0 = there is not** |
| `net_end()` | Gives the socket back. **Required before leaving** |
| `net_update()` | **Once per loop, always.** Nothing arrives without it |
| `net_wait_for_client(seconds)` | SERVER: waits for somebody. **1 = they came** |
| `net_connect_to_server(seconds)` | CLIENT: looks for a server. **1 = found** |
| `net_find_peer(seconds)` | Both do this one, nobody is in charge |
| `net_is_connected()` | 1 once the other machine is known |
| `net_send(data, length)` | Sends one message. **1 = handed over, 0 = not sent** |
| `net_receive(buffer, max)` | Takes one message. **Returns its length, 0 = nothing** |
| `net_get_local_id()` | Our random number for this run |
| `net_get_remote_id()` | Theirs. Compare the two to decide things for free |
| `net_connection_lost()` | 1 when nothing has arrived for 10 seconds |
| `net_set_log(function)` | Where to report problems. Optional |

Constants you may change in `net.h`:

| Constant | Value | What it is |
| --- | --- | --- |
| `NET_MAX_DATA` | 480 | Most bytes in one message |
| `NET_QUEUE_SIZE` | 8 | Messages kept waiting for `net_receive()` |
| `NET_TIMEOUT_SECONDS` | 10 | Silence before `net_connection_lost()` gives up |

And inside `net.c`, if you ever need it:

| Constant | Value | What it is |
| --- | --- | --- |
| `NET_SOCKET_NUMBER` | 0x869C | **Change this** so two programs of yours do not hear each other |
| `NET_LISTEN_ECB_COUNT` | 4 | Receive buffers left with the driver |

Raising `NET_MAX_DATA` past 480 is the one change that can bite you: the
packet would go over the 546 bytes IPX guarantees, and whether it still works
depends on the driver.

---

## 9. Finding out what is going on (optional)

The library is **silent by default**, apart from the pairing functions, which
print on purpose and say so.

That is deliberate: a program in graphics mode cannot write to the screen. So
the library does not decide for you where the text goes; you tell it:

```c
void my_log(char *message)
{
    FILE *f;

    f = fopen("debug.log", "a");

    if (f != NULL){
        fprintf(f, "%s\n", message);
        fclose(f);
    }
}

    /* before net_start() */
    net_set_log(my_log);
```

From then on it tells you things like:

```
NET sizes: ecb=42 header=30 overhead=42 (want 42/30/42)
NET: IPX driver entry at 0300:0010
NET: node 000000000001 id 1839472
NET: paired with id 993822 node 000000000002
NET: sent 4211, received 4198, dropped 0
```

**That first line is worth reading.** If those three numbers are not 42, 30
and 42, the compiler has padded the structures and IPX is reading every field
from the wrong place. Compile with `-a-` (byte alignment), which is Turbo C's
default and what the Makefile here uses.

If you never call `net_set_log()`, nothing bad happens: it carries on working,
quietly. And this is also what makes `net.c` copyable — it does not have to
include any file of yours in order to write to your log.

---

## 10. Common mistakes

| Symptom | Almost certainly |
| --- | --- |
| Nothing ever arrives | You are not calling `net_update()` in some loop that waits |
| Only the first message arrives | `net_receive()` is in an `if` and should be in a `while` |
| `net_start()` returns 0 | No IPX driver, or another copy has the socket |
| Pairing times out | The other machine is not running, or DOSBox is not on the same IPX network |
| Messages go missing now and then | Normal. Check what `net_send()` returned, and read section 5 |
| The sizes line does not say 42/30/42 | The structures are being padded. Compile with `-a-` |
| Garbage in the fields you read | Check the length `net_receive()` returned before using it |
| It hangs for ever waiting | You are not checking `net_connection_lost()` |
| Two of your programs interfere | Give each one a different `NET_SOCKET_NUMBER` |

### Setting it up in DOSBox

On one machine:

```
ipxnet startserver
```

On the other:

```
ipxnet connect 192.168.1.50
```

That is DOSBox's own IPX-over-UDP tunnel and it happens **before** your
program runs. It has nothing to do with `net_wait_for_client()` and
`net_connect_to_server()`, which are your program's own idea of who is in
charge — you can perfectly well run the DOSBox server on the machine that
becomes your client. There is more on this in
[NETWORK-TESTING.md](NETWORK-TESTING.md).

On real machines you load a real driver (`LSL` + `IPXODI`, or the packet
driver shim) and there is nothing to connect: same `.exe`, no recompiling.

---

## 11. And how this repository's game uses it

The game does **not** use `net.c` directly. It has a layer of its own,
`src/lockstep.c`, sitting on top, and that is worth understanding because it
is a good example of what the library is for.

A two player action game cannot just send positions: they would disagree. So
this one sends **one byte of pressed keys per player per frame**, both
machines run the whole game, and they work out the same pixels. Everything
`lockstep.c` adds on top of `net.c` is in service of that:

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

Twenty bytes, sent seventy times a second with `net_send()`, picked up with
`net_receive()`. **`net.c` never looks inside it.**

Two ideas in there are worth stealing:

**Redundancy instead of retransmission.** Every packet carries the **last 8
frames** of keys, not just this one. A packet costs 42 bytes of overhead, so 1
byte of payload or 8 costs practically the same — redundancy is *free*. A lost
packet is covered by the next one, with nothing asked for again and nothing to
wait for. When you are sending something over and over anyway, this beats
acknowledgements every time.

**Checksums to catch what you cannot see.** Every 30 frames each side sends a
checksum of its whole game state. If the two ever disagree, the machines are
quietly simulating different games — and without this you would never know,
because both screens carry on making perfect sense. It costs 6 bytes on a
packet that was going out anyway.

The whole of `lockstep.c` talks to the network through exactly two functions,
`net_send()` and `net_receive()`. It does not know the network is IPX, and
`net.c` does not know it is carrying frames of keys.

That is the point of splitting them.
