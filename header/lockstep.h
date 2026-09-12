#ifndef LOCKSTEP
#define LOCKSTEP

#include "header\net.h"

//===========================================================
// Two player game kept in step, on top of net.c.
//
// THIS IS NOT THE NETWORK LIBRARY. net.c is: it moves bytes and knows
// nothing about frames or players. This file is one particular way of USING
// it, the way a game that has to stay identical on two machines needs.
//
// If what you want is a chat, a file transfer or a program that just sends
// three bytes now and again, you do not want this file at all. Copy net.c
// and header\net.h and read doc\EN\NETWORK-TUTORIAL.md.
//
// HOW THE GAME IS KEPT IN SYNC (lockstep): no positions are ever sent. BOTH
// machines run the WHOLE game, both tanks included, and all that travels is
// one byte of pressed keys per player per frame. The game is integer only,
// has no rand() and never reads the clock, so feeding both machines the same
// keys makes them work out exactly the same pixels.
//
//   frame N:  send my keys for frame N + NET_INPUT_DELAY
//             wait for the other machine's keys for frame N
//             simulate frame N with BOTH sets of keys
//
// That also means the sound needs no network at all: both machines work out
// the same shot on the same frame, so both play it by themselves.
//===========================================================


// ---- The one byte that travels ----
// Which keys are held this frame. Same 5 bits whether they come from our own
// keyboard or from the other machine, so process_player_input() cannot tell
// the difference and does not need to.
#define NET_INPUT_UP		0x01
#define NET_INPUT_DOWN		0x02
#define NET_INPUT_LEFT		0x04
#define NET_INPUT_RIGHT		0x08
#define NET_INPUT_FIRE		0x10


// Frames your own keys are held back before they are applied.
//
// This is THE tuning knob. Your keys are delayed exactly as long as the
// other player's, so both machines apply both inputs on the same frame.
// Applying your own straight away would feel better and desync in the first
// second, because then the two machines would be simulating different games.
//
// The loop runs at about 70 Hz, so one frame is ~14 ms and 5 frames is
// ~71 ms of network jitter absorbed before the game hitches. Over wifi 5 is
// a sensible floor; over a cable 3 is plenty. Raise it if the game stutters.
#define NET_INPUT_DELAY 		5

// How many past frames of keys ride along in EVERY packet.
//
// The IPX header alone is 30 bytes, so 1 byte of payload or 8 costs
// practically the same: redundancy is free. It means a lost or late packet
// is covered by the next one, with no retransmission and no acknowledgement.
// The game only hitches if 8 packets in a row fail to arrive in time.
#define NET_REDUNDANCY 			8

// Ring buffer of frames kept in memory, for our own keys and the other
// machine's. MUST be a power of two, the index is worked out with an AND.
#define NET_INPUT_BUFFER_SIZE 	64

// Frames between two state checksums.
//
// In lockstep a desync is invisible: each machine keeps showing a game that
// makes perfect sense, just a different one. So every so often each side
// sends a checksum of its whole game state and compares it with its own.
// Without this you chase the bug for days; with it, game.log tells you the
// exact frame it broke on.
#define NET_CHECKSUM_INTERVAL 	30

// Seconds net_find_opponent() keeps looking before giving up
#define NET_DISCOVERY_SECONDS 	30


// Starts the network up and gets ready to play. Returns 1 if there is
// network, 0 if there is not. This is net_start() plus the game's own state.
int net_init(void);

// Gives the socket back and writes the run's figures to the log. MUST be
// called before leaving the program.
void net_shutdown(void);

// Finds the other machine and pairs with it, in TEXT mode, before the game
// switches to VGA. Nobody types an address: the two copies find each other
// on their own. Returns 1 when paired, 0 if it timed out or a key was hit.
int net_find_opponent(void);

// Settles which LEVEL both machines are going to load, and has to be called
// after net_find_opponent() and before the map is loaded.
//
// The theme does NOT need settling: sky, war and neon are the same world
// repainted and share their collision map byte for byte, so the two machines
// can wear different ones. The level does: each one is a different set of
// walls, and loading different ones is playing different games.
//
// PLAYER 1 DECIDES. my_level is only a request; on player 2 it is ignored and
// whatever player 1 offers comes back instead.
//
// Returns the agreed level (0 = the original big.bmp, 1..5 = the levels), or
// -1 if they could not agree, which means DO NOT START.
int net_agree_level(int my_level);


//===========================================================
// THE INTRO, OVER THE NETWORK
//
// The intro runs for over two minutes. On one machine that is fine; with
// two of them it is a problem with three separate edges, and these four
// functions are the answer to all three:
//
//   WHO PLAYS IT. The game has no idea which machine started the server:
//   discovery is a symmetric broadcast and player 1 is a coin toss between
//   two random ids. So the machines have to ASK each other, and that is
//   net_agree_demo().
//
//   THE OTHER ONE MUST NOT GIVE UP. net_connection_lost() fires after ten
//   seconds of silence and the intro lasts a hundred and thirty four. Both
//   ends keep a heartbeat going the whole time.
//
//   THEY MUST LEAVE TOGETHER. Same reason as the level handshake: the music
//   is streamed and nobody ever resynchronises it, so a machine that starts
//   the match a second early stays a second early until the last note.
//===========================================================

// Tells the other machine that this one is about to show the intro, or
// listens for it being told. NOTHING is negotiated: the caller has already
// decided from the role it was started with, /server or /client.
//
//   i_play_it   1 if this machine shows it
//
//   returns     1  play it
//               0  wait for the other machine to play it
//              -1  no intro, straight to the match
//
// It is one directional on purpose, and the comment on the function says
// why: the version that negotiated could put BOTH machines into the waiting
// state, because two ids that come out equal make both ends believe they are
// player 2. A machine only waits here if it actually received an
// announcement, so both waiting is not a state that exists.
int net_agree_demo(int i_play_it);

// The heartbeat, for the machine that IS playing. Hand it to
// demo_set_idle() and the intro calls it once a frame on its own.
//
// Returns 1 when the other machine has asked to cut the intro short, which
// demo_run() then treats exactly like a local ESC. So ESC works from either
// keyboard, which matters: the one watching a black screen is the one most
// likely to want out.
int net_demo_idle(void);

// The other side of it: sit here until the intro on the other machine is
// over. Answers the heartbeat so neither end times out, and sends the stop
// request if ESC is pressed here.
//
// Returns 1 when the intro ended normally, 0 if the connection was lost.
int net_demo_wait(void);

// Called by the machine that played it, once the intro is over. Tells the
// other one and holds both of them here for the same few ticks, so the two
// matches start on the same moment.
void net_demo_finished(void);

// A barrier: neither machine leaves until both have arrived.
//
// It exists for the music, and only for the music. The tanks do not need it:
// lockstep makes whoever gets to frame 0 first wait for the other. The song
// does, because it is streamed straight off the disk and nothing ever
// resynchronises it, so two machines that start it a second apart stay a
// second apart until the last note.
//
// And a second apart is easy. Between the moment the two machines last
// agreed on anything and the moment the music starts, each one reads 256000
// bytes of map, another 256000 of collision bitmap, 39 sprites and four WAV
// files. Whatever difference there is between two disks lands straight in
// the song. So instead of hoping they take the same time, they meet here,
// with all the loading behind them and play_song() the very next thing.
//
// Does nothing at all when there is no network.
void net_wait_together(void);

// Which tank is ours: 1 = player 1 (the one at the bottom), 0 = player 2.
//
// Decided with no negotiation at all: each copy picks a random id at
// startup, both ids travel in the pairing, and the lower one is player 1.
// Both machines work out the same answer on their own.
int net_is_player1(void);

// Picks up whatever has arrived and files it. Cheap, and safe to call as
// often as you like. Call it in any loop that waits, or nothing arrives.
void net_poll(void);

// Our own keys for the frame NET_INPUT_DELAY ahead of the one being
// simulated. Call once per frame, before net_send_input().
void net_set_local_input(unsigned char input_bits);

// Sends our last NET_REDUNDANCY frames of keys, and the state checksum when
// one is due. Never blocks: if the previous send has not finished, this
// frame's send is simply skipped and the redundancy covers it.
void net_send_input(void);

// Has the other machine's input for the frame we are about to simulate
// arrived yet? While this is 0 the game MUST NOT advance: keep calling
// net_poll() (and sound_update(), so the sound does not stutter) and ask
// again.
int net_has_remote_input(void);

// The keys for the frame being simulated now. The local one comes out of
// the ring buffer, NOT straight from the keyboard: that is what makes both
// machines apply it on the same frame.
unsigned char net_get_remote_input(void);
unsigned char net_get_local_input(void);

// Hands in the checksum of the game state, after the frame has been
// simulated. Compared against the other machine's a few frames later.
void net_set_local_checksum(unsigned int checksum);

// Done with this frame, move on to the next one. Call once per frame, last.
void net_advance_frame(void);

// 1 when the two machines have computed different states. The game carries
// on, but game.log has the frame it happened on.
int net_desync_detected(void);

// The frame being simulated, for the log
unsigned long net_get_frame(void);

// Counts one frame spent standing still waiting for the other machine. A big
// number in the log at the end means NET_INPUT_DELAY is too small for the link.
void net_count_wait(void);

// net_connection_lost() is NOT here: it belongs to the network and lives in
// net.h, which this file has already included.

#endif
