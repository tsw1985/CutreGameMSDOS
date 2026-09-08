#include <stdio.h>
#include <string.h>
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
