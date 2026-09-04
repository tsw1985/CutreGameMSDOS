#ifndef NET
#define NET

//===========================================================
// Two player game over the network, on top of IPX.
//
// WHY IPX AND NOT SOCKETS: Turbo C++ 3.0 has no networking at all, and DOS
// has no TCP/IP either. In DOS the network is a driver you load before the
// game, and IPX is the one that needs no stack on top: you fill a structure
// in memory, tell the driver "send this", and it goes. It is what Doom used.
//
// Nothing here is DOSBox specific. DOSBox implements the same IPX calls a
// real driver does, so the very same game.exe runs on two DOSBoxes talking
// over UDP, or on two real machines with real cards and IPXODI loaded. No
// recompiling, no #ifdef.
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
// Without this you chase the bug for days; with it, tanks.log tells you the
// exact frame it broke on.
#define NET_CHECKSUM_INTERVAL 	30

// Seconds without a single packet before the game gives up and quits
#define NET_TIMEOUT_SECONDS 	10

// Seconds net_find_opponent() keeps looking before giving up
#define NET_DISCOVERY_SECONDS 	30


// Finds the IPX driver and opens our socket. Returns 1 if there is network,
// 0 if there is not (no driver loaded, or the socket was busy). Everything
// else here must only be called after this has returned 1.
int net_init(void);

// Closes the socket and gives the driver back what is ours. MUST be called
// before leaving the program.
void net_shutdown(void);

// Looks for the other machine and pairs with it. Runs in TEXT mode, before
// the game switches to VGA, so it can print what is going on: it broadcasts
// a HELLO every quarter of a second and waits for one back.
//
// Nobody types an IP anywhere. IPX has broadcast, so the two copies find
// each other on their own and each ends up knowing the other's node address.
//
// Returns 1 when paired, 0 if it timed out or the user pressed a key.
int net_find_opponent(void);

// Which tank is ours: 1 = player 1 (the one at the bottom), 0 = player 2.
//
// Decided with no negotiation at all: each copy picks a random id at
// startup, both ids travel in the HELLO, and the lower one is player 1.
// Both machines work out the same answer on their own.
int net_is_player1(void);

// Picks up whatever has arrived from the driver. Cheap, and safe to call as
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

// 1 when nothing has arrived for NET_TIMEOUT_SECONDS: the other machine is
// gone. The game quits instead of hanging for ever.
int net_connection_lost(void);

// 1 when the two machines have computed different states. The game carries
// on, but tanks.log has the frame it happened on.
int net_desync_detected(void);

// The frame being simulated, for the log
unsigned long net_get_frame(void);

// Counts one frame spent standing still waiting for the other machine. A big
// number in the log at the end means NET_INPUT_DELAY is too small for the link.
void net_count_wait(void);

#endif
