# Testing the game over the network

DOSBox configuration files for trying out `game.exe /net`.

**First of all:** the `bin\GAME.EXE` you run has to be the **freshly built**
one, the one that already has the network code in it. Copy it over from
whichever machine you compile on.

---

## Step 1 - one PC, two DOSBoxes

This is the test that separates "my code is right" from "my network is right".
If this works, the code is correct and anything that goes wrong afterwards is
a network problem.

```
dosbox -conf net-test/dosbox-local-a.conf     <- this one first
dosbox -conf net-test/dosbox-local-b.conf     <- and then this one
```

Nothing to configure.

---

## Step 2 - two machines: use the scripts

The `.conf` files in this folder have absolute paths written inside them, and
that is exactly what breaks when you carry them over to the second machine.
For two machines use the scripts in the project root: they **work out where
the game is on their own**, from where they themselves are, and generate the
`.conf` on every launch. There is no path to adjust anywhere.

On the machine acting as the server (start this one **first**):

```
./launch_game_server.sh
```

It prints the IP the other player needs. On the other machine:

```
./launch_game_client.sh <that-ip>
```

Each machine needs its own copy of `bin\` and `res\`, and the project can live
in a different folder on each one. It makes no difference.

Options, on both scripts:

| | |
|---|---|
| `-p 6000` | a different port, the same on both |
| `-y 'max'` | different CPU cycles, the same on both |
| `-c file.conf` | use your own `.conf` instead of generating one |
| `-h` | help |

Before launching anything they check that DOSBox is installed, that `bin/` and
`res/` are there, that the port is neither privileged nor already in use, and
the client pings the server. Every failure says what is wrong and how to fix
it.

> "Server" does not mean it does not play. Both machines play exactly the same
> game; all the server does is bring up the tunnel so the other one can join.
> It has nothing to do with who ends up being player 1 either.

---

## Where the logs are

Each instance writes its log to `k:\game.log`, and every conf mounts a
**different** K: drive on purpose: with a shared one the two copies would
overwrite each other's log, right where you need it most.

| How you launched | Server / A | Client / B |
|---|---|---|
| The scripts | `net-test/log-server/GAME.LOG` | `net-test/log-client/GAME.LOG` |
| `dosbox-local-*.conf` | `net-test/log-a/GAME.LOG` | `net-test/log-b/GAME.LOG` |

With two machines, each log stays **on its own machine**.

```
tail -f net-test/log-server/GAME.LOG | grep --line-buffered "NET\|Tank hit"
```

| Line | What it means |
|---|---|
| `NET sizes: ecb=42 header=30 packet=60` | Good. Any other number means the compiler padded the structures and IPX would read every field from the wrong place |
| `NET: IPX driver entry at XXXX:YYYY` | The IPX driver is there |
| `NET: node 000000000000` | **Bad sign.** An all-zero node means no tunnel assigned us an address: this instance is not joined to one |
| `NET: paired. ... we are player N` | Paired, and which tank we got |
| `NET DESYNC at frame N` | The two machines computed different states |
| `NET: sent X, received Y, waited Z frames` | Summary on the way out |

Exit with **ESC**, not by closing the window, or the summary line never gets
written.

### Reading `waited`

`waited` counts the frames the game spent standing still waiting for the other
machine. It has **two different causes**, and telling them apart matters
because the fix is not the same:

- **A few, in bursts** — network jitter. This is what `NET_INPUT_DELAY` in
  `header/net.h` is for. Raise it.
- **A steady, large fraction of the frames** (say half of them) — the two
  machines are **not running at the same speed**, and the faster one is being
  paced by the slower one. Raising `NET_INPUT_DELAY` does **not** help here and
  only adds latency: a fixed buffer absorbs variation, not a permanent
  throughput gap. Lower `cycles` on **both** machines to a value the slower one
  can actually sustain (`-y 'fixed 20000'`).

To tell which machine is the slow one, compare `waited` in the two logs: the
slow one will be close to zero.

Inside DOSBox, to check the tunnel without involving the game at all:

```
ipxnet ping
ipxnet status
```

---

## The port: why not 213

DOSBox defaults to **UDP port 213** for the IPX tunnel. On **Linux that does
not work**: anything below 1024 is privileged and a normal user **cannot open
it**. `ipxnet startserver` fails, and the only symptom you see is the other
side saying `Timeout connecting to server`.

That is why these confs use **5213**, which is high and needs no privileges:

```
ipxnet startserver 5213
ipxnet connect <ip> 5213
```

The port has to be **the same on both machines**. On Windows 213 does work,
but there is no reason to use it: leave 5213 on both and forget about it. And
if you open a firewall, open **UDP 5213**, not 213.

---

Full documentation: [`NETWORK.md`](NETWORK.md)
