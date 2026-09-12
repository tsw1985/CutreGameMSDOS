#include <stdio.h>
#include <string.h>
#include <bios.h>		// biostime(), for the level handshake timeout
#include "header\util.h"
#include "header\net.h"
#include "header\lockstep.h"

//===========================================================
// Keeping two machines simulating the very same game. See lockstep.h for
// the why.
//
// Everything here sits ON TOP of net.c and only ever touches it through
// net_send() and net_receive(). It does not know that the network is IPX,
// and net.c does not know that what it is carrying is frames of keys: swap
// net.c for a serial cable one day and this file would not change a line.
//
// This is the GAME's layer, not the library's, and it is the reason it is a
// separate file. Copying net.c into a chat program should not drag frames,
// checksums and ring buffers along with it.
//===========================================================


//===========================================================
// What one packet of ours carries.
//
// It goes into net_send() as a lump of bytes, and comes out of
// net_receive() as the same lump: net.c never looks inside it.
//
// The two longs are first on purpose. Turbo C aligns structures on bytes by
// default, so it would not matter here, but putting the wide fields at the
// front costs nothing and keeps the layout obvious.
//===========================================================
struct lockstep_message {

	unsigned long  base_frame;				// the frame inputs[0] belongs to
	unsigned long  checksum_frame;			// which frame the checksum was taken on
	unsigned int   checksum_value;
	unsigned char  count;					// how many entries of inputs[] are real
	unsigned char  has_checksum;			// 1 if the two checksum fields mean anything
	unsigned char  inputs[NET_REDUNDANCY];	// keys for base_frame .. base_frame+count-1

};


// Who drives which tank
static int is_player1 = 0;

// The frame being simulated right now. Both machines are always on the same
// one: that is the whole point.
static unsigned long simulation_frame;

// Our own keys, indexed by frame. Written NET_INPUT_DELAY frames ahead of
// where they are read, so our input is applied as late as the other one.
static unsigned char local_input_value[NET_INPUT_BUFFER_SIZE];

// Their keys. Each slot remembers which frame it holds, so a stale entry
// from a lap ago can never be mistaken for the one we are waiting for.
static unsigned long remote_input_frame[NET_INPUT_BUFFER_SIZE];
static unsigned char remote_input_value[NET_INPUT_BUFFER_SIZE];
static unsigned char remote_input_valid[NET_INPUT_BUFFER_SIZE];

// Our own checksums, kept so one arriving from the other machine a few
// frames late still finds the frame it belongs to
static unsigned long local_checksum_frame[NET_INPUT_BUFFER_SIZE];
static unsigned int  local_checksum_value[NET_INPUT_BUFFER_SIZE];
static unsigned char local_checksum_valid[NET_INPUT_BUFFER_SIZE];

// The checksum waiting for a packet to ride along on
static unsigned long pending_checksum_frame;
static unsigned int  pending_checksum_value;
static unsigned char pending_checksum_ready;
static unsigned int  checksum_countdown;

static int desync_detected = 0;

static unsigned long total_waits;

static char lockstep_log_text[100];


//===========================================================
// Files one frame of the other machine's keys.
//
// Frames already simulated are dropped: they are no use, we have moved on.
// Frames further ahead than the ring is long are dropped too, or they would
// land on top of a frame we still need.
//===========================================================
static void net_store_remote_input(unsigned long frame, unsigned char input_bits){

	unsigned int index;

	if (frame < simulation_frame){
		return;
	}

	if (frame >= simulation_frame + NET_INPUT_BUFFER_SIZE){
		return;
	}

	index = (unsigned int)(frame & (NET_INPUT_BUFFER_SIZE - 1));

	remote_input_frame[index] = frame;
	remote_input_value[index] = input_bits;
	remote_input_valid[index] = 1;

}


//===========================================================
// Compares a checksum from the other machine against our own for that frame.
//
// If they differ the two machines are simulating different games. It cannot
// be repaired from here, but it is written down: without this line a desync
// looks like nothing at all, because each screen carries on making sense.
//===========================================================
static void net_check_remote_checksum(unsigned long frame, unsigned int value){

	unsigned int index;

	index = (unsigned int)(frame & (NET_INPUT_BUFFER_SIZE - 1));

	if (local_checksum_valid[index] == 0){
		return;
	}

	if (local_checksum_frame[index] != frame){
		return;
	}

	if (local_checksum_value[index] == value){
		return;
	}

	if (desync_detected == 0){
		sprintf(lockstep_log_text, "NET DESYNC at frame %lu: mine %u theirs %u",
		        frame, local_checksum_value[index], value);
		tanks_log(lockstep_log_text);
	}

	desync_detected = 1;

}


//===========================================================
// Deals with one message that has arrived
//===========================================================
static void net_handle_message(struct lockstep_message *message){

	unsigned int  entry;
	unsigned long frame;

	// The last NET_REDUNDANCY frames of their keys, so one lost packet
	// is covered by the next without anybody asking for it again
	entry = 0;
	while (entry < message->count){

		if (entry >= NET_REDUNDANCY){
			break;
		}

		frame = message->base_frame + (unsigned long)entry;
		net_store_remote_input(frame, message->inputs[entry]);

		entry = entry + 1;

	}

	if (message->has_checksum == 1){
		net_check_remote_checksum(message->checksum_frame, message->checksum_value);
	}

}


//===========================================================
// Picks up everything the network has for us.
//
// net_update() is what actually talks to the driver; this drains the
// messages it left behind. There can be more than one, so it reads until
// net_receive() says there is nothing left.
//===========================================================
void net_poll(void){

	struct lockstep_message message;
	int length;

	net_update();

	length = net_receive(&message, sizeof(struct lockstep_message));

	while (length > 0){

		// Anything that is not exactly one of our messages is somebody
		// else's idea of a packet. Ignore it rather than read fields out of
		// something that was never ours.
		if (length == sizeof(struct lockstep_message)){
			net_handle_message(&message);
		}

		length = net_receive(&message, sizeof(struct lockstep_message));

	}

}


//===========================================================
// Starts the network and clears the game's own state.
//===========================================================
int net_init(void){

	int index;

	desync_detected = 0;
	total_waits     = 0;

	simulation_frame       = 0;
	pending_checksum_ready = 0;
	checksum_countdown     = NET_CHECKSUM_INTERVAL;

	index = 0;
	while (index < NET_INPUT_BUFFER_SIZE){
		local_input_value[index]    = 0;
		remote_input_frame[index]   = 0;
		remote_input_value[index]   = 0;
		remote_input_valid[index]   = 0;
		local_checksum_valid[index] = 0;
		index = index + 1;
	}

	// net.c reports through whatever it is given and stays quiet otherwise.
	// This is where the game hands it its own log, and the only line in the
	// whole thing that ties the network to this particular program.
	net_set_log(tanks_log);

	return net_start();

}


void net_shutdown(void){

	sprintf(lockstep_log_text, "NET: waited %lu frames for the other machine", total_waits);
	tanks_log(lockstep_log_text);

	net_end();

}


//===========================================================
// Finds the other machine and works out who is who.
//
// Both copies are the same program and neither has any reason to be in
// charge, so net_find_peer() is the right one of the three: whoever hears
// the other first does the answering.
//===========================================================
//===========================================================
// Agreeing on the level, before a single frame is simulated.
//
// The THEME does not need agreeing: sky, war and neon are the same world
// repainted, so the collision map is byte for byte identical between them.
// Two machines can happily wear different themes.
//
// The LEVEL is the opposite. Each level is a different set of walls, so if
// the two machines load different ones they are playing different games: the
// tanks walk through each other's walls and the checksum screams on the
// first check. It has to be settled BEFORE the game starts.
//
// How it is settled: PLAYER 1 DECIDES. Not a negotiation, not a vote. Player
// 1 was already picked without exchanging a word (the lower id), so it is
// free to be the one that chooses, and player 2 simply adopts it.
//
// Player 1 offers, player 2 acknowledges. Both retry, because IPX loses
// packets and a lost offer would leave player 2 waiting for ever.
//
// The message is 4 bytes and lockstep_message is 20, so the two can never be
// mistaken for each other: net_poll() drops anything that is not exactly its
// own size, and this function drops anything that is not exactly its own.
//===========================================================

#define LEVEL_OFFER 	1
#define LEVEL_ACK 		2

struct level_message {
	unsigned char magic[2];		// "LV", so a stray packet cannot pass for one
	unsigned char type;			// LEVEL_OFFER or LEVEL_ACK
	unsigned char level;		// 0 = the original big.bmp, 1..5 = the levels
};

// A quarter of a second between retries, like the HELLO of net.c
#define LEVEL_RETRY_TICKS 	5
#define LEVEL_TIMEOUT_TICKS 	(18 * 10)


//===========================================================
// Returns the level BOTH machines are going to load, or -1 if they could not
// agree (which means: do not start, you would desync immediately).
//
// my_level is only a request. Player 1 gets its way; player 2's is ignored,
// and it is told so on screen rather than left wondering.
//===========================================================
int net_agree_level(int my_level){

	struct level_message message;
	struct level_message incoming;
	long start_tick;
	long now_tick;
	long next_send_tick;
	int  length;
	int  agreed;

	agreed = -1;

	start_tick     = biostime(0, 0L);
	next_send_tick = start_tick;

	message.magic[0] = 'L';
	message.magic[1] = 'V';

	while (1){

		now_tick = biostime(0, 0L);

		if (now_tick - start_tick > LEVEL_TIMEOUT_TICKS){
			tanks_log("NET: timed out agreeing the level");
			return -1;
		}

		if (net_connection_lost() == 1){
			tanks_log("NET: connection lost while agreeing the level");
			return -1;
		}

		//-----------------------------------------------
		// Player 1 keeps offering until it hears an ACK.
		// Player 2 keeps quiet until it hears an offer.
		//-----------------------------------------------
		if (is_player1 == 1 && agreed < 0){

			if (now_tick >= next_send_tick){

				next_send_tick = now_tick + LEVEL_RETRY_TICKS;

				message.type  = LEVEL_OFFER;
				message.level = (unsigned char)my_level;

				net_send(&message, sizeof(struct level_message));

			}

		}

		net_update();

		length = net_receive(&incoming, sizeof(struct level_message));

		while (length > 0){

			// Anything that is not exactly one of ours is ignored. It costs
			// three comparisons and it means a stray packet on the socket
			// can never be read as a level.
			if (length == (int)sizeof(struct level_message) &&
			    incoming.magic[0] == 'L' && incoming.magic[1] == 'V'){

				if (is_player1 == 0 && incoming.type == LEVEL_OFFER){

					// Player 2: take what it is given and say so. The ACK is
					// sent EVERY time an offer arrives, not just the first,
					// because player 1 will keep offering until one of them
					// gets through.
					agreed = (int)incoming.level;

					message.type  = LEVEL_ACK;
					message.level = incoming.level;

					net_send(&message, sizeof(struct level_message));

				}

				if (is_player1 == 1 && incoming.type == LEVEL_ACK){

					// Player 1: they have it. Note it and stop offering, but
					// do NOT leave yet: see the wait at the bottom of the loop.
					if ((int)incoming.level == my_level){
						agreed = my_level;
					}

				}

			}

			length = net_receive(&incoming, sizeof(struct level_message));

		}

		//-----------------------------------------------
		// NEITHER machine returns the moment it has the number. Both stay
		// here until the same deadline, counted from the same start.
		//
		// Two different reasons, and both matter:
		//
		//   Player 2 has to keep answering for a while, so player 1's last
		//   offers keep getting their ACK. Leaving at once would often make
		//   player 1 wait out the full timeout for an ACK that was never
		//   sent again.
		//
		//   Player 1 has to wait for the SAME length of time, or it walks out
		//   of here a second ahead of player 2 and stays a second ahead for
		//   the rest of the game. The tanks would not care, because lockstep
		//   makes whoever is early wait at frame 0 anyway. The music would:
		//   it is streamed, nobody synchronises it, and the two songs would
		//   play a second apart from the first note to the last.
		//
		// Both machines got here within a packet of each other (net_pair()
		// only lets go when both ends have heard a HELLO and had their own
		// answered), so counting the same number of ticks from their own
		// start_tick lands them on the same moment.
		//-----------------------------------------------
		if (agreed >= 0){

			if (now_tick - start_tick > LEVEL_RETRY_TICKS * 4){
				return agreed;
			}

		}

	}

}


//===========================================================
// THE INTRO, OVER THE NETWORK
//
// Read the block in header\lockstep.h first: it says what the three
// problems are. This is the same shape as net_agree_level() above, message
// for message, because it is the same kind of job.
//===========================================================

#define DEMO_START 		1		// I am about to show the intro
#define DEMO_START_ACK 	2		// understood, I will wait
#define DEMO_ALIVE 		3		// still going, do not give up on me
#define DEMO_STOP 		4		// cut it short (ESC on the watching machine)
#define DEMO_OVER 		5		// the intro has finished
#define DEMO_OVER_ACK 	6		// heard you
#define DEMO_READY 		7		// loaded and standing at the start line

struct demo_message {
	unsigned char magic[2];		// "DM", so a stray packet cannot pass for one
	unsigned char type;
	unsigned char spare;		// keeps it 4 bytes, like the level message
};

// How often the two ends shout "still here", in ticks. Two seconds against a
// ten second timeout: four missed heartbeats in a row before anybody worries.
#define DEMO_BEAT_TICKS 	36

// Giving up on agreeing who plays. Short on purpose: if this does not settle
// in ten seconds something is wrong with the link, and the match matters more
// than the intro.
#define DEMO_AGREE_TIMEOUT 	(18 * 10)

// How long the machine waiting for an intro puts up with hearing nothing at
// all. The one showing it beats every two seconds, so ten of silence means
// there is no intro and nobody is coming to say so.
#define DEMO_SILENCE_TICKS 	(18 * 10)

// Set by net_agree_demo() and read by the two functions below, so the caller
// does not have to carry it around.
static int  demo_stop_requested = 0;
static long demo_next_beat_tick = 0;


//-----------------------------------------------------------
// Fills in a message of ours. Four lines in one place instead of four lines
// in five places.
//-----------------------------------------------------------
static void demo_fill(struct demo_message *message, int type){

	message->magic[0] = 'D';
	message->magic[1] = 'M';
	message->type     = (unsigned char)type;
	message->spare    = 0;

}


//-----------------------------------------------------------
// Is this one of ours? Same three comparisons as the level handshake, and
// for the same reason: a stray packet on the socket must never be read as
// an instruction.
//-----------------------------------------------------------
static int demo_is_ours(struct demo_message *message, int length){

	if (length != (int)sizeof(struct demo_message)){
		return 0;
	}
	if (message->magic[0] != 'D' || message->magic[1] != 'M'){
		return 0;
	}

	return 1;

}


//===========================================================
// WHO PLAYS THE INTRO
//
// Nothing is negotiated here, and that is the point. The caller has already
// decided, from the role it was started with, and this only tells the other
// machine so it knows to wait.
//
// It used to be a negotiation: both ends said whether they had -demo and, if
// both did, player 1 won the tie. That deadlocked, and the reason is worth
// keeping:
//
//   net.c seeds its instance id with srand(biostime()), and the BIOS tick
//   moves 18.2 times a second. Two machines whose clocks agree and that
//   start within a eighteenth of a second of each other get the SAME id.
//   Then "local < remote" is false on BOTH of them, both believe they are
//   player 2, and both sat waiting for an intro nobody was showing.
//
// One direction cannot deadlock. Only a machine that was told to play sends
// DEMO_START, and only a machine that has RECEIVED one waits.
//
//   i_play_it   1 if this machine is the one that shows the intro
//
//   returns     1  play it
//               0  wait for the other machine to play it
//              -1  no intro, go straight to the match
//===========================================================
int net_agree_demo(int i_play_it){

	struct demo_message message;
	struct demo_message incoming;
	long start_tick;
	long now_tick;
	long next_send_tick;
	int  length;
	int  acked;
	int  heard_start;

	demo_stop_requested = 0;
	demo_next_beat_tick = biostime(0, 0L);

	acked       = 0;
	heard_start = 0;

	start_tick     = biostime(0, 0L);
	next_send_tick = start_tick;

	while (1){

		now_tick = biostime(0, 0L);

		if (now_tick - start_tick > DEMO_AGREE_TIMEOUT){
			break;
		}

		if (net_connection_lost() == 1){
			tanks_log("NET: connection lost settling the intro");
			return -1;
		}

		//-----------------------------------------------
		// The one that plays keeps announcing until it is answered. There is
		// no state to agree, only a fact to deliver, so repeating it is the
		// whole of the reliability.
		//-----------------------------------------------
		if (i_play_it == 1 && acked == 0){

			if (now_tick >= next_send_tick){

				next_send_tick = now_tick + LEVEL_RETRY_TICKS;

				demo_fill(&message, DEMO_START);
				net_send(&message, sizeof(struct demo_message));

			}

		}

		net_update();

		length = net_receive(&incoming, sizeof(struct demo_message));

		while (length > 0){

			if (demo_is_ours(&incoming, length) == 1){

				// Answer EVERY announcement, not just the first: they keep
				// sending until one of our acks gets through.
				if (incoming.type == DEMO_START){

					heard_start = 1;

					demo_fill(&message, DEMO_START_ACK);
					net_send(&message, sizeof(struct demo_message));

				}

				if (incoming.type == DEMO_START_ACK){
					acked = 1;
				}

			}

			length = net_receive(&incoming, sizeof(struct demo_message));

		}

		//-----------------------------------------------
		// Both sides hold for the same few ticks after they know, the same
		// way the level handshake does, so the last acks keep flowing.
		//-----------------------------------------------
		if (i_play_it == 1 && acked == 1){
			if (now_tick - start_tick > LEVEL_RETRY_TICKS * 4){
				break;
			}
		}

		if (i_play_it == 0 && heard_start == 1){
			if (now_tick - start_tick > LEVEL_RETRY_TICKS * 4){
				break;
			}
		}

	}

	demo_next_beat_tick = biostime(0, 0L);

	//---------------------------------------------------
	// And the answer, which has no room for both ends agreeing on the same
	// thing by accident.
	//---------------------------------------------------
	if (i_play_it == 1){

		if (acked == 1){
			tanks_log("NET: showing the intro, the other machine is waiting");
			return 1;
		}

		// Nobody answered. Playing anyway would leave them in the game loop
		// with nothing arriving, and ten seconds of that is a lost
		// connection. An intro is not worth a broken match.
		tanks_log("NET: nobody acked the intro, skipping it");
		return -1;

	}

	if (heard_start == 1){
		tanks_log("NET: the other machine is showing the intro, waiting");
		return 0;
	}

	tanks_log("NET: no intro announced, straight to the match");

	return -1;

}


//===========================================================
// THE HEARTBEAT, on the machine that is playing.
//
// Handed to demo_set_idle(), so the intro calls it once a frame without
// knowing what it is. It has to be CHEAP: it runs 9000 times over the two
// minutes and it is competing with a rotozoom for the frame.
//
// Returns 1 when they have asked us to stop.
//===========================================================
int net_demo_idle(void){

	struct demo_message message;
	struct demo_message incoming;
	long now_tick;
	int  length;

	if (net_is_connected() == 0){
		return 0;
	}

	now_tick = biostime(0, 0L);

	if (now_tick >= demo_next_beat_tick){

		demo_next_beat_tick = now_tick + DEMO_BEAT_TICKS;

		demo_fill(&message, DEMO_ALIVE);
		net_send(&message, sizeof(struct demo_message));

	}

	net_update();

	length = net_receive(&incoming, sizeof(struct demo_message));

	while (length > 0){

		if (demo_is_ours(&incoming, length) == 1){

			if (incoming.type == DEMO_STOP){
				demo_stop_requested = 1;
			}

		}

		length = net_receive(&incoming, sizeof(struct demo_message));

	}

	if (demo_stop_requested == 1){
		return 1;
	}

	return 0;

}


//===========================================================
// THE OTHER SIDE: waiting for an intro that is playing somewhere else.
//
// Returns 1 when it is over, 0 if the connection went.
//===========================================================
int net_demo_wait(void){

	struct demo_message message;
	struct demo_message incoming;
	long now_tick;
	long start_tick;
	long last_heard_tick;
	int  length;
	int  finished;

	finished = 0;

	demo_next_beat_tick = biostime(0, 0L);
	last_heard_tick     = biostime(0, 0L);

	while (finished == 0){

		if (net_connection_lost() == 1){
			tanks_log("NET: connection lost while waiting for the intro");
			return 0;
		}

		now_tick = biostime(0, 0L);

		//-----------------------------------------------
		// A way out that does not depend on anybody telling us.
		//
		// The machine showing the intro beats every two seconds. Ten of
		// silence means it is not showing one: most likely it heard nothing
		// back from us, gave up, and went to the match while we sat here.
		// Walking in a few seconds late beats waiting for good.
		//-----------------------------------------------
		if (now_tick - last_heard_tick > DEMO_SILENCE_TICKS){
			tanks_log("NET: ten seconds without a word about the intro, giving up on it");
			return 1;
		}

		//-----------------------------------------------
		// We answer with our own heartbeat, and that is not politeness: the
		// machine playing the intro is watching ITS timeout too, and a
		// silent partner for two minutes looks exactly like a dead one.
		//-----------------------------------------------
		if (now_tick >= demo_next_beat_tick){

			demo_next_beat_tick = now_tick + DEMO_BEAT_TICKS;

			if (demo_stop_requested == 1){
				demo_fill(&message, DEMO_STOP);
			}else{
				demo_fill(&message, DEMO_ALIVE);
			}

			net_send(&message, sizeof(struct demo_message));

		}

		//-----------------------------------------------
		// ESC here stops the intro over there. The person staring at a
		// screen that says "waiting" is the one most likely to want out,
		// so it would be daft to make them walk to the other machine.
		//
		// It is sent at once and then on every heartbeat, because this is
		// the one message in the whole exchange that has no reply to tell
		// us it arrived.
		//-----------------------------------------------
		if (demo_stop_requested == 0){

			if (bioskey(1) != 0){

				if ((bioskey(0) & 0x00FF) == 27){

					demo_stop_requested = 1;

					demo_fill(&message, DEMO_STOP);
					net_send(&message, sizeof(struct demo_message));

					printf("Skipping the intro on the other machine...\n");

				}

			}

		}

		net_update();

		length = net_receive(&incoming, sizeof(struct demo_message));

		while (length > 0){

			if (demo_is_ours(&incoming, length) == 1){

				last_heard_tick = now_tick;

				if (incoming.type == DEMO_OVER){

					// Answer EVERY one, not just the first: they keep
					// sending until one of our acks gets through.
					demo_fill(&message, DEMO_OVER_ACK);
					net_send(&message, sizeof(struct demo_message));

					finished = 1;

				}

			}

			length = net_receive(&incoming, sizeof(struct demo_message));

		}

	}

	//---------------------------------------------------
	// Hold for the same few ticks the other machine holds, counted from the
	// moment we learned it was over. Both walk out together, and the two
	// songs of the match start on the same beat.
	//
	// And keep answering while we wait: they carry on sending DEMO_OVER
	// until one of our acks arrives, and an unanswered one would leave them
	// counting out their whole timeout.
	//---------------------------------------------------
	start_tick = biostime(0, 0L);

	while (biostime(0, 0L) - start_tick <= LEVEL_RETRY_TICKS * 4){

		net_update();

		length = net_receive(&incoming, sizeof(struct demo_message));

		while (length > 0){

			if (demo_is_ours(&incoming, length) == 1){

				if (incoming.type == DEMO_OVER){
					demo_fill(&message, DEMO_OVER_ACK);
					net_send(&message, sizeof(struct demo_message));
				}

			}

			length = net_receive(&incoming, sizeof(struct demo_message));

		}

	}

	return 1;

}


//===========================================================
// The intro is over. Say so until they answer, then leave together.
//===========================================================
void net_demo_finished(void){

	struct demo_message message;
	struct demo_message incoming;
	long start_tick;
	long now_tick;
	long next_send_tick;
	long ack_tick;
	int  length;
	int  acked;

	if (net_is_connected() == 0){
		return;
	}

	acked    = 0;
	ack_tick = 0;

	start_tick     = biostime(0, 0L);
	next_send_tick = start_tick;

	while (1){

		now_tick = biostime(0, 0L);

		// Not getting an ack is not a reason to refuse to play. Worst case
		// the other machine is a couple of seconds behind, and lockstep
		// makes whoever is early wait at frame 0 anyway.
		if (now_tick - start_tick > DEMO_AGREE_TIMEOUT){
			tanks_log("NET: nobody acked the end of the intro");
			return;
		}

		if (net_connection_lost() == 1){
			return;
		}

		if (acked == 0 && now_tick >= next_send_tick){

			next_send_tick = now_tick + LEVEL_RETRY_TICKS;

			demo_fill(&message, DEMO_OVER);
			net_send(&message, sizeof(struct demo_message));

		}

		net_update();

		length = net_receive(&incoming, sizeof(struct demo_message));

		while (length > 0){

			if (demo_is_ours(&incoming, length) == 1){

				if (incoming.type == DEMO_OVER_ACK && acked == 0){

					// The one moment BOTH machines witness: they sent it,
					// we received it. See the hold below.
					acked    = 1;
					ack_tick = now_tick;

				}

			}

			length = net_receive(&incoming, sizeof(struct demo_message));

		}

		//-----------------------------------------------
		// THE HOLD, counted from ACK_TICK and not from start_tick.
		//
		// That distinction is a whole bug. The first version counted from
		// the moment this machine STARTED announcing the end, while the
		// other counted from the moment it HEARD it. Those are the same
		// instant only if the first announcement got through.
		//
		// Drop three of them and the announcement lands fifteen ticks late:
		// this machine has already run its hold out and leaves at once, the
		// other starts its fifteen ticks later, and the match begins nearly
		// a second apart on the two screens. The tanks would not care,
		// lockstep makes whoever is early wait at frame 0. The music would:
		// it is streamed, nobody resynchronises it, and the two songs stay
		// that second apart from the first note to the last.
		//
		// The ack is the event both machines witness. Counting from there
		// puts them a network latency apart instead of a lost packet apart.
		//-----------------------------------------------
		if (acked == 1){

			if (now_tick - ack_tick > LEVEL_RETRY_TICKS * 4){
				return;
			}

		}

	}

}


//===========================================================
// THE BARRIER
//
// See header\lockstep.h for why this exists at all. Short version: the
// music is streamed and never resynchronised, so the two machines have to
// start it on the same moment, and everything they do between the last
// handshake and that moment is several seconds of disk work that no two
// machines do in the same time.
//
// It is symmetric and there is no leader: both shout READY until they hear
// the other one, and leave the instant they do.
//
// READY goes out IMMEDIATELY on arrival and then once a tick, not once
// every LEVEL_RETRY_TICKS. It matters: the machine that gets here first is
// already shouting, so the late one hears it on arrival and leaves at once,
// and the early one leaves one network latency later. Throttling to five
// ticks would put a quarter of a second of slack in exactly the place this
// function exists to remove.
//===========================================================
void net_wait_together(void){

	struct demo_message message;
	struct demo_message incoming;
	long start_tick;
	long now_tick;
	long next_send_tick;
	int  length;
	int  heard;

	if (net_is_connected() == 0){
		return;
	}

	heard = 0;

	start_tick     = biostime(0, 0L);
	next_send_tick = start_tick;

	while (heard == 0){

		now_tick = biostime(0, 0L);

		// Waiting for ever for a machine that has died would be worse than
		// a song out of step.
		if (now_tick - start_tick > DEMO_SILENCE_TICKS){
			tanks_log("NET: the other machine never reached the start, going anyway");
			return;
		}

		if (net_connection_lost() == 1){
			tanks_log("NET: connection lost at the start line");
			return;
		}

		if (now_tick >= next_send_tick){

			next_send_tick = now_tick + 1;

			demo_fill(&message, DEMO_READY);
			net_send(&message, sizeof(struct demo_message));

		}

		net_update();

		length = net_receive(&incoming, sizeof(struct demo_message));

		while (length > 0){

			if (demo_is_ours(&incoming, length) == 1){

				if (incoming.type == DEMO_READY){
					heard = 1;
				}

			}

			length = net_receive(&incoming, sizeof(struct demo_message));

		}

	}

	//---------------------------------------------------
	// One last READY on the way out.
	//
	// The other machine may still be waiting to hear from us: if it arrived
	// first, it has been shouting into the void and our first READY is the
	// one that releases it. Sending one more here means it does not have to
	// wait for our next tick.
	//---------------------------------------------------
	demo_fill(&message, DEMO_READY);
	net_send(&message, sizeof(struct demo_message));

	tanks_log("NET: both machines at the start line");

}


int net_find_opponent(void){

	int index;

	if (net_find_peer(NET_DISCOVERY_SECONDS) == 0){
		return 0;
	}

	// Who drives which tank, settled without a word being exchanged about
	// it: both machines compare the same two numbers and reach the same
	// answer. The lower id is player 1, the tank at the bottom.
	if (net_get_local_id() < net_get_remote_id()){
		is_player1 = 1;
	}else{
		is_player1 = 0;
	}

	// Everything starts at frame 0, on both machines.
	//
	// The first NET_INPUT_DELAY frames have no keys behind them, on either
	// side, so they are filled in as "nothing pressed" and marked as already
	// received. Without this both machines would sit waiting for an input
	// for frame 0 that neither of them ever sent.
	simulation_frame = 0;

	index = 0;
	while (index < NET_INPUT_BUFFER_SIZE){
		local_input_value[index]    = 0;
		remote_input_frame[index]   = 0;
		remote_input_value[index]   = 0;
		remote_input_valid[index]   = 0;
		local_checksum_valid[index] = 0;
		index = index + 1;
	}

	index = 0;
	while (index < NET_INPUT_DELAY){
		local_input_value[index]  = 0;
		remote_input_frame[index] = (unsigned long)index;
		remote_input_value[index] = 0;
		remote_input_valid[index] = 1;
		index = index + 1;
	}

	pending_checksum_ready = 0;
	checksum_countdown     = NET_CHECKSUM_INTERVAL;

	if (is_player1 == 1){
		printf("You are PLAYER 1, the tank at the bottom.\n");
	}else{
		printf("You are PLAYER 2, the tank at the top.\n");
	}

	printf("Both machines drive with the cursor keys and fire with keypad 5.\n");

	sprintf(lockstep_log_text, "NET: we are player %d", 2 - is_player1);
	tanks_log(lockstep_log_text);

	return 1;

}


int net_is_player1(void){

	return is_player1;

}


unsigned long net_get_frame(void){

	return simulation_frame;

}


//===========================================================
// Files our own keys for the frame NET_INPUT_DELAY ahead of the one being
// simulated.
//
// This is the whole input delay in one line. Our keys go in at
// simulation_frame + NET_INPUT_DELAY and are read back out at
// simulation_frame, which is exactly as late as the other machine's arrive.
//===========================================================
void net_set_local_input(unsigned char input_bits){

	unsigned long target_frame;
	unsigned int  index;

	target_frame = simulation_frame + NET_INPUT_DELAY;
	index = (unsigned int)(target_frame & (NET_INPUT_BUFFER_SIZE - 1));

	local_input_value[index] = input_bits;

}


//===========================================================
// Sends our last NET_REDUNDANCY frames of keys, plus a checksum when one is
// due to go out.
//===========================================================
void net_send_input(void){

	struct lockstep_message message;
	unsigned long newest_frame;
	unsigned long frame;
	unsigned int  count;
	unsigned int  entry;
	unsigned int  index;

	if (net_is_connected() == 0){
		return;
	}

	newest_frame = simulation_frame + NET_INPUT_DELAY;

	// At the very start there are not NET_REDUNDANCY frames to look back on
	// yet, so send only the ones that exist
	if (newest_frame + 1 < NET_REDUNDANCY){
		count = (unsigned int)(newest_frame + 1);
	}else{
		count = NET_REDUNDANCY;
	}

	memset(&message, 0, sizeof(struct lockstep_message));

	message.count      = (unsigned char)count;
	message.base_frame = newest_frame - (unsigned long)count + 1;

	entry = 0;
	while (entry < count){

		frame = message.base_frame + (unsigned long)entry;
		index = (unsigned int)(frame & (NET_INPUT_BUFFER_SIZE - 1));

		message.inputs[entry] = local_input_value[index];

		entry = entry + 1;

	}

	if (pending_checksum_ready == 1){

		message.has_checksum   = 1;
		message.checksum_frame = pending_checksum_frame;
		message.checksum_value = pending_checksum_value;

	}

	// A 0 means the driver was still busy with the last packet and this one
	// was dropped. That is fine and it is what the redundancy is for: the
	// next packet carries this frame too. The checksum is only marked as
	// sent if the packet really went, so it waits for the next one instead
	// of being lost.
	if (net_send(&message, sizeof(struct lockstep_message)) == 1){
		pending_checksum_ready = 0;
	}

}


//===========================================================
// Have they sent us the keys for the frame we are about to simulate?
//
// While this is 0 the game must stand still. That is the price of lockstep,
// and the input delay is what keeps it from being paid very often.
//===========================================================
int net_has_remote_input(void){

	unsigned int index;

	index = (unsigned int)(simulation_frame & (NET_INPUT_BUFFER_SIZE - 1));

	if (remote_input_valid[index] == 0){
		return 0;
	}

	if (remote_input_frame[index] != simulation_frame){
		return 0;
	}

	return 1;

}


unsigned char net_get_remote_input(void){

	unsigned int index;

	index = (unsigned int)(simulation_frame & (NET_INPUT_BUFFER_SIZE - 1));

	return remote_input_value[index];

}


unsigned char net_get_local_input(void){

	unsigned int index;

	index = (unsigned int)(simulation_frame & (NET_INPUT_BUFFER_SIZE - 1));

	return local_input_value[index];

}


//===========================================================
// Takes the checksum of the state after this frame has been simulated. Kept
// for comparing, and every NET_CHECKSUM_INTERVAL frames one is put aside to
// travel on the next packet.
//===========================================================
void net_set_local_checksum(unsigned int checksum){

	unsigned int index;

	index = (unsigned int)(simulation_frame & (NET_INPUT_BUFFER_SIZE - 1));

	local_checksum_frame[index] = simulation_frame;
	local_checksum_value[index] = checksum;
	local_checksum_valid[index] = 1;

	checksum_countdown = checksum_countdown - 1;

	if (checksum_countdown == 0){

		checksum_countdown     = NET_CHECKSUM_INTERVAL;
		pending_checksum_frame = simulation_frame;
		pending_checksum_value = checksum;
		pending_checksum_ready = 1;

	}

}


void net_advance_frame(void){

	simulation_frame = simulation_frame + 1;

}


int net_desync_detected(void){

	return desync_detected;

}


//===========================================================
// Counts one frame spent waiting for the other machine, for the log. A big
// number here means the input delay is too small for the link.
//===========================================================
void net_count_wait(void){

	total_waits = total_waits + 1;

}
