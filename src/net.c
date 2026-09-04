#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <bios.h>
#include <string.h>
#include <stdlib.h>
#include "header\util.h"
#include "header\net.h"

//===========================================================
// IPX transport and lockstep synchronisation. See net.h for the why.
//
// This file is split in three layers, bottom to top:
//
//   1. Talking to the IPX driver at all  (ipx_* functions)
//   2. Our packet on top of IPX          (net_transmit / net_receive)
//   3. Lockstep: the input ring buffers  (net_set_local_input and friends)
//
// It knows NOTHING about tanks. It moves one byte of keys per frame and
// compares a checksum somebody else works out.
//===========================================================


//===========================================================
// LAYER 1 - THE IPX DRIVER
//===========================================================

// IPX functions, the number that goes in BX
#define IPX_FUNCTION_OPEN_SOCKET		0x0000
#define IPX_FUNCTION_CLOSE_SOCKET		0x0001
#define IPX_FUNCTION_SEND				0x0003
#define IPX_FUNCTION_LISTEN				0x0004
#define IPX_FUNCTION_RELINQUISH			0x000A
#define IPX_FUNCTION_GET_ADDRESS		0x0009

// Our socket number. Any value from 0x8000 up is fair game for an
// application; it just has to be the SAME on both machines, since it is what
// tells our packets apart from anything else on the wire.
//
// It travels big endian (high byte first), which is the opposite of how the
// 8086 stores an int, hence net_swap16() everywhere a socket is written.
#define NET_SOCKET_NUMBER 				0x869C

// Packet type 4 is "PEP", an ordinary unsequenced datagram: fire and forget,
// no acknowledgement, no ordering. Exactly what a game wants.
#define IPX_PACKET_TYPE 				4

// How many receive buffers are left posted with the driver at once.
//
// More than one on purpose: while we are dealing with a packet its buffer
// belongs to us, not to the driver, and a packet arriving right then would
// be dropped if it were the only one. Four is far more than 70 packets a
// second ever needs.
#define NET_LISTEN_ECB_COUNT 			4

// What kind of packet this is
#define NET_TYPE_HELLO 					1
#define NET_TYPE_HELLO_ACK 				2
#define NET_TYPE_INPUT 					3

// First 4 bytes of every packet of ours. Anything on our socket that does
// not start with this is somebody else's traffic and is thrown away.
#define NET_MAGIC_0 					'C'
#define NET_MAGIC_1 					'T'
#define NET_MAGIC_2 					'R'
#define NET_MAGIC_3 					'E'

// The BIOS ticks at 18.2 Hz. Close enough for timeouts.
#define NET_TICKS_PER_SECOND 			18L

// A quarter of a second between HELLO broadcasts while looking for the other
// machine. Short, because the gap between the two copies pairing is what
// they will have to catch up on once the game starts.
#define NET_HELLO_INTERVAL_TICKS 		5L


//===========================================================
// The Event Control Block: the form you fill in for the driver.
//
// You hand it one of these, it does the job, and it sets in_use back to 0
// when it has finished. That is the whole conversation.
//
// The layout is fixed by Novell down to the byte, so nothing here may be
// reordered, resized or padded. Turbo C aligns structures on bytes by
// default (-a-), which is what we need; net_init() logs the sizes so a
// wrong one shows up straight away instead of as mysterious garbage.
//===========================================================
struct ipx_ecb {

	void far      *link_address;			// the driver's own list, never touched by us
	void far      (*esr_address)();			// callback on completion. ALWAYS NULL here, see below
	unsigned char  in_use;					// non zero while the driver owns this ECB
	unsigned char  completion_code;			// 0 = it worked
	unsigned int   socket_number;			// our socket, big endian
	unsigned char  ipx_workspace[4];		// scratch for the driver
	unsigned char  driver_workspace[12];	// scratch for the driver
	unsigned char  immediate_address[6];	// MAC of the next hop. On send, the destination
	unsigned int   fragment_count;			// how many pieces the packet is in. Always 1 here
	void far      *fragment_address;		// where the packet is
	unsigned int   fragment_size;			// how big it is

};

//===========================================================
// The 30 byte IPX header, at the front of every packet.
//
// On send we fill in the destination and the driver fills in the source. On
// receive the driver fills in the lot, which is how we learn who the other
// machine is without anybody typing an address.
//===========================================================
struct ipx_header {

	unsigned int   checksum;				// 0xFFFF = none. IPX has never really used this
	unsigned int   length;					// whole packet, big endian
	unsigned char  transport_control;		// routers count hops here. 0 on a flat LAN
	unsigned char  packet_type;				// IPX_PACKET_TYPE
	unsigned char  destination_network[4];	// 0 = "this network", no routing
	unsigned char  destination_node[6];		// the MAC, or FF FF FF FF FF FF for everybody
	unsigned char  destination_socket[2];
	unsigned char  source_network[4];		// from here down, filled in by the driver
	unsigned char  source_node[6];
	unsigned char  source_socket[2];

};

//===========================================================
// What we actually send. 30 bytes, and the IPX header in front of it is 30
// bytes on its own: the header costs as much as the payload.
//
// That is the reason NET_REDUNDANCY exists. Sending 1 byte of keys or 8
// costs the same packet, so we may as well send the last 8 frames every
// time and never need a retransmission.
//===========================================================
struct net_payload {

	unsigned char  magic[4];				// "CTRE", so we ignore other traffic
	unsigned char  type;					// NET_TYPE_*
	unsigned char  count;					// INPUT: how many entries of inputs[] are real
	unsigned long  instance_id;				// who sent it, and who gets to be player 1
	unsigned long  base_frame;				// INPUT: the frame inputs[0] belongs to
	unsigned char  inputs[NET_REDUNDANCY];	// keys for base_frame .. base_frame+count-1
	unsigned char  has_checksum;			// 1 if the two fields below mean anything
	unsigned char  padding;					// keeps the long below on an even offset
	unsigned long  checksum_frame;			// which frame the checksum was taken on
	unsigned int   checksum_value;

};

struct net_packet {

	struct ipx_header  header;
	struct net_payload payload;

};


// Where the driver lives. Found once by ipx_detect() and far called from
// then on. NOT an interrupt: IPX is entered with a far call.
static unsigned int ipx_entry_segment;
static unsigned int ipx_entry_offset;

// 1 once net_init() has found the driver and opened the socket
static int net_is_running = 0;

// One ECB and one buffer for sending, several for receiving
static struct ipx_ecb    send_ecb;
static struct net_packet send_packet;

static struct ipx_ecb    listen_ecb[NET_LISTEN_ECB_COUNT];
static struct net_packet listen_packet[NET_LISTEN_ECB_COUNT];

// Whether each receive buffer is currently in the driver's hands. Needed
// because in_use is also 0 on a buffer that was never handed over, and we
// must not read a packet that never arrived.
static unsigned char listen_is_posted[NET_LISTEN_ECB_COUNT];

// Broadcast: "every node on this network"
static unsigned char broadcast_node[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Us, and them
static unsigned char local_node[6];
static unsigned char remote_node[6];

// A number picked at random when the program starts. It does two jobs: it
// tells our own packets from theirs, and the lower of the two decides who
// gets to be player 1, with no negotiation of any kind.
static unsigned long local_instance_id;
static unsigned long remote_instance_id;

static int paired_with_opponent = 0;
static int is_player1 = 0;

// ---- Lockstep state ----

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

// BIOS tick when the last packet arrived, for the timeout
static long last_packet_tick;

static int connection_lost = 0;
static int desync_detected = 0;

// Counters for the log, so a bad run can be read afterwards
static unsigned long total_packets_sent;
static unsigned long total_packets_received;
static unsigned long total_waits;

static char net_log_text[80];


//===========================================================
// Swaps the two bytes of a 16 bit value.
//
// IPX writes sockets and lengths big endian, high byte first, and the 8086
// stores an int the other way round. So anything that goes into one of those
// fields has to be turned round first.
//===========================================================
static unsigned int net_swap16(unsigned int value){

	unsigned int high_byte;
	unsigned int low_byte;

	high_byte = (value >> 8) & 0x00FF;
	low_byte  = value & 0x00FF;

	return (low_byte << 8) | high_byte;

}


//===========================================================
// Is the IPX driver there, and where is its front door?
//
// INT 2F is the DOS "is anybody there" multiplex. AX=7A00 asks specifically
// for IPX: AL comes back 0xFF if it is loaded, and ES:DI is the address to
// call from then on.
//
// This is the documented way in, and the one Doom used. It works with a real
// IPXODI, with Novell's client, and with DOSBox, which answers this call
// exactly like a real driver would.
//===========================================================
static int ipx_detect(void){

	union  REGS  regs;
	struct SREGS sregs;

	segread(&sregs);

	regs.x.ax = 0x7A00;
	int86x(0x2F, &regs, &regs, &sregs);

	if (regs.h.al != 0xFF){
		return 0;
	}

	ipx_entry_segment = sregs.es;
	ipx_entry_offset  = regs.x.di;

	return 1;

}


//===========================================================
// The call into the driver: BX = what to do, ES:SI = the ECB.
//
// It has to be assembler because IPX wants its arguments in registers and is
// entered with a FAR CALL, neither of which C can express.
//
// The awkward bit is calling an address held in a variable. There is no
// "far call to this register pair" instruction, so the address is pushed on
// the stack and called from there, which is what the push cx / push dx /
// mov bp,sp / call dword ptr [bp] dance is doing. BP is put back before
// anything BP relative is touched again, because the driver is free to
// return with BP pointing anywhere.
//
// DS and ES are saved too: in the huge memory model the compiler assumes DS
// still points at this module's data when the block ends, and the driver
// makes no such promise.
//
// NOTE: if TCC ever refuses "call dword ptr [bp]", the same instruction can
// be written by hand as:   db 0FFh, 05Eh, 000h
//===========================================================
static void ipx_call(unsigned int function, void far *ecb){

	unsigned int ecb_segment;
	unsigned int ecb_offset;
	unsigned int entry_segment;
	unsigned int entry_offset;

	ecb_segment   = FP_SEG(ecb);
	ecb_offset    = FP_OFF(ecb);
	entry_segment = ipx_entry_segment;
	entry_offset  = ipx_entry_offset;

	asm {
		push	si
		push	di
		push	ds
		push	es
		push	bp

		mov		bx, function
		mov		ax, ecb_segment
		mov		si, ecb_offset
		mov		cx, entry_segment
		mov		dx, entry_offset
		mov		es, ax

		push	cx
		push	dx
		mov		bp, sp
		call	dword ptr [bp]
		add		sp, 4

		pop		bp
		pop		es
		pop		ds
		pop		di
		pop		si
	}

}


//===========================================================
// The socket calls. These two do not use an ECB: the socket number goes
// straight in DX, big endian, and the result comes back in AL.
//
// Same far call trick as above, only here the result has to be picked up
// after BP has been put back, or "mov result_code, ax" would write itself
// somewhere in the middle of the stack.
//===========================================================
static unsigned int ipx_socket_call(unsigned int function, unsigned int socket_high_low){

	unsigned int result_code;
	unsigned int entry_segment;
	unsigned int entry_offset;

	entry_segment = ipx_entry_segment;
	entry_offset  = ipx_entry_offset;

	result_code = 0;

	asm {
		push	si
		push	di
		push	ds
		push	es
		push	bp

		mov		bx, function
		mov		dx, socket_high_low
		mov		cx, entry_segment
		mov		si, entry_offset
		mov		al, 0

		push	cx
		push	si
		mov		bp, sp
		call	dword ptr [bp]
		add		sp, 4

		pop		bp

		mov		ah, 0
		mov		result_code, ax

		pop		es
		pop		ds
		pop		di
		pop		si
	}

	return result_code;

}


//===========================================================
// Asks the driver what our own node address is: 4 bytes of network and 6 of
// node. We only care about the node, and only to print it in the log.
//
// Our own packets are told apart by instance_id, not by this, precisely so
// that a driver that answers this call badly cannot break the pairing.
//===========================================================
static void ipx_get_local_address(void){

	unsigned char address_buffer[10];

	memset(address_buffer, 0, 10);

	ipx_call(IPX_FUNCTION_GET_ADDRESS, (void far *)address_buffer);

	memcpy(local_node, &address_buffer[4], 6);

}


//===========================================================
// Hands one receive buffer back to the driver.
//
// A buffer is either ours or the driver's, never both. Posting it makes it
// the driver's; it becomes ours again when in_use drops to 0.
//===========================================================
static void net_post_listen(int index){

	memset(&listen_ecb[index], 0, sizeof(struct ipx_ecb));

	// NULL on purpose. IPX can call a routine of ours the moment a packet
	// lands, but that routine would run at interrupt time, in the middle of
	// whatever the game was doing, with all the reentrancy problems that
	// brings. Looking at in_use once a frame is enough and cannot go wrong.
	listen_ecb[index].esr_address = (void far (*)())0;

	listen_ecb[index].socket_number    = net_swap16(NET_SOCKET_NUMBER);
	listen_ecb[index].fragment_count   = 1;
	listen_ecb[index].fragment_address = (void far *)&listen_packet[index];
	listen_ecb[index].fragment_size    = sizeof(struct net_packet);

	ipx_call(IPX_FUNCTION_LISTEN, (void far *)&listen_ecb[index]);

	listen_is_posted[index] = 1;

}


//===========================================================
// Is the driver still busy with the last packet we gave it?
//
// This MUST be asked before net_build_header() is called, not just before
// net_transmit(): while a send is in flight the driver owns send_packet, and
// building the next payload into it would rewrite a packet already on its
// way out.
//===========================================================
static int net_send_is_busy(void){

	if (net_is_running == 0){
		return 1;
	}

	if (send_ecb.in_use != 0){
		return 1;
	}

	return 0;

}


//===========================================================
// Sends whatever is already sitting in send_packet.payload to one node.
//
// Never waits. If the previous send has not finished the packet is simply
// dropped, because while the driver owns send_packet we must not write to
// it. Nothing is lost that matters: the next packet carries the last
// NET_REDUNDANCY frames of keys anyway.
//===========================================================
static int net_transmit(unsigned char *destination_node){

	if (net_is_running == 0){
		return 0;
	}

	if (send_ecb.in_use != 0){
		return 0;
	}

	// ---- The IPX header: who it is going to ----

	memset(&send_packet.header, 0, sizeof(struct ipx_header));

	send_packet.header.checksum    = 0xFFFF;
	send_packet.header.length      = net_swap16(sizeof(struct net_packet));
	send_packet.header.packet_type = IPX_PACKET_TYPE;

	// Network 0 means "the one I am on". Anything else would need a router,
	// and there is none between two machines on the same wifi.
	memset(send_packet.header.destination_network, 0, 4);
	memcpy(send_packet.header.destination_node, destination_node, 6);

	send_packet.header.destination_socket[0] = (NET_SOCKET_NUMBER >> 8) & 0x00FF;
	send_packet.header.destination_socket[1] = NET_SOCKET_NUMBER & 0x00FF;

	// ---- The form for the driver ----

	memset(&send_ecb, 0, sizeof(struct ipx_ecb));

	send_ecb.esr_address     = (void far (*)())0;
	send_ecb.socket_number   = net_swap16(NET_SOCKET_NUMBER);
	send_ecb.fragment_count  = 1;
	send_ecb.fragment_address = (void far *)&send_packet;
	send_ecb.fragment_size    = sizeof(struct net_packet);

	// On a flat network the next hop IS the destination. A router would need
	// IPX function 2 to work out something different, and there is no router.
	memcpy(send_ecb.immediate_address, destination_node, 6);

	ipx_call(IPX_FUNCTION_SEND, (void far *)&send_ecb);

	total_packets_sent = total_packets_sent + 1;

	return 1;

}


//===========================================================
// Fills in the part of the payload that every packet carries
//===========================================================
static void net_build_header(unsigned char packet_type){

	memset(&send_packet.payload, 0, sizeof(struct net_payload));

	send_packet.payload.magic[0] = NET_MAGIC_0;
	send_packet.payload.magic[1] = NET_MAGIC_1;
	send_packet.payload.magic[2] = NET_MAGIC_2;
	send_packet.payload.magic[3] = NET_MAGIC_3;

	send_packet.payload.type        = packet_type;
	send_packet.payload.instance_id = local_instance_id;

}


//===========================================================
// Is this one of ours, and is it worth looking at?
//===========================================================
static int net_packet_is_valid(struct net_packet *packet){

	if (packet->payload.magic[0] != NET_MAGIC_0){
		return 0;
	}

	if (packet->payload.magic[1] != NET_MAGIC_1){
		return 0;
	}

	if (packet->payload.magic[2] != NET_MAGIC_2){
		return 0;
	}

	if (packet->payload.magic[3] != NET_MAGIC_3){
		return 0;
	}

	// Our own broadcast finding its way back to us. On real Ethernet a card
	// does not hear itself and this never happens, but it costs nothing to
	// be sure.
	if (packet->payload.instance_id == local_instance_id){
		return 0;
	}

	return 1;

}


//===========================================================
// LAYER 3 - LOCKSTEP
//===========================================================

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
		sprintf(net_log_text, "NET DESYNC at frame %lu: mine %u theirs %u",
		        frame, local_checksum_value[index], value);
		tanks_log(net_log_text);
	}

	desync_detected = 1;

}


//===========================================================
// Deals with one packet that has arrived
//===========================================================
static void net_handle_packet(struct net_packet *packet){

	unsigned int  entry;
	unsigned long frame;

	total_packets_received = total_packets_received + 1;
	last_packet_tick = biostime(0, 0L);

	if (packet->payload.type == NET_TYPE_HELLO){

		// They are here. Remember who they are and answer them directly, so
		// they know we are here too: neither side starts until BOTH have
		// seen the other, or one would run off and start playing on its own.
		remote_instance_id = packet->payload.instance_id;
		memcpy(remote_node, packet->header.source_node, 6);

		if (net_send_is_busy() == 0){
			net_build_header(NET_TYPE_HELLO_ACK);
			net_transmit(remote_node);
		}

		paired_with_opponent = paired_with_opponent | 0x01;

		return;

	}

	if (packet->payload.type == NET_TYPE_HELLO_ACK){

		remote_instance_id = packet->payload.instance_id;
		memcpy(remote_node, packet->header.source_node, 6);

		paired_with_opponent = paired_with_opponent | 0x02;

		return;

	}

	if (packet->payload.type == NET_TYPE_INPUT){

		// The last NET_REDUNDANCY frames of their keys, so one lost packet
		// is covered by the next without anybody asking for it again
		entry = 0;
		while (entry < packet->payload.count){

			if (entry >= NET_REDUNDANCY){
				break;
			}

			frame = packet->payload.base_frame + (unsigned long)entry;
			net_store_remote_input(frame, packet->payload.inputs[entry]);

			entry = entry + 1;

		}

		if (packet->payload.has_checksum == 1){
			net_check_remote_checksum(packet->payload.checksum_frame,
			                          packet->payload.checksum_value);
		}

		return;

	}

}


//===========================================================
// Picks up everything the driver has for us and hands the buffers back.
//
// Cheap, and safe to call as often as you like: it is the only thing that
// ever moves data in, so any loop that waits MUST call it.
//===========================================================
void net_poll(void){

	int index;

	if (net_is_running == 0){
		return;
	}

	// Lets the driver get on with its own work. Some real drivers need this
	// to actually move packets; DOSBox does not care, and it costs nothing.
	ipx_call(IPX_FUNCTION_RELINQUISH, (void far *)0);

	index = 0;
	while (index < NET_LISTEN_ECB_COUNT){

		if (listen_is_posted[index] == 1){

			if (listen_ecb[index].in_use == 0){

				// The buffer is ours again. Deal with it, then give it
				// straight back so the driver is never short of one.
				listen_is_posted[index] = 0;

				if (listen_ecb[index].completion_code == 0){

					if (net_packet_is_valid(&listen_packet[index]) == 1){
						net_handle_packet(&listen_packet[index]);
					}

				}

				net_post_listen(index);

			}

		}

		index = index + 1;

	}

}


//===========================================================
// Finds the driver, opens the socket, and gets everything ready to listen.
//===========================================================
int net_init(void){

	int index;

	net_is_running   = 0;
	connection_lost  = 0;
	desync_detected  = 0;
	paired_with_opponent = 0;

	total_packets_sent     = 0;
	total_packets_received = 0;
	total_waits            = 0;

	// A structure that is the wrong size means the compiler has padded it,
	// and IPX would then read every field from the wrong place. Logging the
	// sizes turns that from a baffling crash into one obvious line.
	sprintf(net_log_text, "NET sizes: ecb=%u header=%u packet=%u (want 42/30/60)",
	        (unsigned int)sizeof(struct ipx_ecb),
	        (unsigned int)sizeof(struct ipx_header),
	        (unsigned int)sizeof(struct net_packet));
	tanks_log(net_log_text);

	if (ipx_detect() == 0){
		tanks_log("NET: no IPX driver found (int 2F/7A00 said no)");
		return 0;
	}

	sprintf(net_log_text, "NET: IPX driver entry at %04X:%04X",
	        ipx_entry_segment, ipx_entry_offset);
	tanks_log(net_log_text);

	if (ipx_socket_call(IPX_FUNCTION_OPEN_SOCKET, net_swap16(NET_SOCKET_NUMBER)) != 0){
		tanks_log("NET: could not open the socket, another copy may be running");
		return 0;
	}

	net_is_running = 1;

	ipx_get_local_address();

	// Our id for this run. The other machine will almost certainly have
	// booted at a different moment, so its BIOS tick, and therefore its id,
	// is different. Both halves are filled so the two ids differ in every
	// bit, not just the low ones.
	srand((unsigned int)biostime(0, 0L));
	local_instance_id = ((unsigned long)rand() << 16) | (unsigned long)rand();

	sprintf(net_log_text, "NET: node %02X%02X%02X%02X%02X%02X id %lu",
	        local_node[0], local_node[1], local_node[2],
	        local_node[3], local_node[4], local_node[5],
	        local_instance_id);
	tanks_log(net_log_text);

	// Hand every receive buffer to the driver before anything else, or the
	// first packets to arrive have nowhere to land
	index = 0;
	while (index < NET_LISTEN_ECB_COUNT){
		listen_is_posted[index] = 0;
		net_post_listen(index);
		index = index + 1;
	}

	last_packet_tick = biostime(0, 0L);

	return 1;

}


//===========================================================
// Gives the socket back. If this is skipped the driver keeps our socket and
// our buffers, and the next run cannot open the same socket.
//===========================================================
void net_shutdown(void){

	if (net_is_running == 0){
		return;
	}

	sprintf(net_log_text, "NET: sent %lu, received %lu, waited %lu frames",
	        total_packets_sent, total_packets_received, total_waits);
	tanks_log(net_log_text);

	ipx_socket_call(IPX_FUNCTION_CLOSE_SOCKET, net_swap16(NET_SOCKET_NUMBER));

	net_is_running = 0;

}


//===========================================================
// Looks for the other machine. Text mode, before the game switches to VGA,
// so it can say what is happening.
//
// Both copies do exactly the same thing: shout HELLO to everybody, answer
// any HELLO they hear, and wait until they have BOTH heard a HELLO and
// received an answer to their own. Only then is it certain that the other
// side also knows the game is on, and both can start at frame 0 together.
//
// Pairing on the first HELLO alone would not do: the faster machine would
// run off and start the game while the slower one was still waiting for an
// answer that was never coming.
//===========================================================
int net_find_opponent(void){

	long start_tick;
	long now_tick;
	long next_hello_tick;
	int  index;
	int  extra_acks;
	int  ack_attempts;

	if (net_is_running == 0){
		return 0;
	}

	printf("\n");
	printf("Looking for the other player...\n");
	printf("(both machines must be running the game, press any key to give up)\n");
	printf("\n");

	start_tick      = biostime(0, 0L);
	next_hello_tick = start_tick;

	while (paired_with_opponent != 0x03){

		now_tick = biostime(0, 0L);

		if (now_tick >= next_hello_tick){

			next_hello_tick = now_tick + NET_HELLO_INTERVAL_TICKS;

			if (net_send_is_busy() == 0){
				net_build_header(NET_TYPE_HELLO);
				net_transmit(broadcast_node);
			}

			printf(".");

		}

		net_poll();

		if (now_tick - start_tick > NET_DISCOVERY_SECONDS * NET_TICKS_PER_SECOND){
			printf("\n\nNobody answered.\n");
			tanks_log("NET: discovery timed out");
			return 0;
		}

		if (kbhit()){
			getch();
			printf("\n\nCancelled.\n");
			tanks_log("NET: discovery cancelled by the user");
			return 0;
		}

	}

	// A few more answers on the way out. We only get here because they
	// answered us, so they have already heard our HELLO, but the answer WE
	// sent them may have been the one that got lost. These cost nothing.
	extra_acks  = 0;
	ack_attempts = 0;

	while (extra_acks < 3){

		if (net_send_is_busy() == 0){
			net_build_header(NET_TYPE_HELLO_ACK);
			net_transmit(remote_node);
			extra_acks = extra_acks + 1;
		}

		net_poll();

		// Bounded on purpose. If the driver never frees the send buffer this
		// must not turn into a loop with no way out: the answers are a
		// courtesy, not something worth hanging the game for.
		ack_attempts = ack_attempts + 1;

		if (ack_attempts > 100){
			break;
		}

	}

	// Who drives which tank, settled without a word being exchanged about
	// it: both machines compare the same two numbers and reach the same
	// answer. The lower id is player 1, the tank at the bottom.
	if (local_instance_id < remote_instance_id){
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
		local_input_value[index]  = 0;
		remote_input_frame[index] = 0;
		remote_input_value[index] = 0;
		remote_input_valid[index] = 0;
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

	last_packet_tick = biostime(0, 0L);
	connection_lost  = 0;

	printf("\n\nConnected.\n");

	if (is_player1 == 1){
		printf("You are PLAYER 1, the tank at the bottom.\n");
	}else{
		printf("You are PLAYER 2, the tank at the top.\n");
	}

	printf("Both machines drive with the cursor keys and fire with keypad 5.\n");

	sprintf(net_log_text, "NET: paired. them id %lu node %02X%02X%02X%02X%02X%02X, we are player %d",
	        remote_instance_id,
	        remote_node[0], remote_node[1], remote_node[2],
	        remote_node[3], remote_node[4], remote_node[5],
	        2 - is_player1);
	tanks_log(net_log_text);

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

	unsigned long newest_frame;
	unsigned long frame;
	unsigned int  count;
	unsigned int  entry;
	unsigned int  index;

	if (net_is_running == 0){
		return;
	}

	// Busy sending the last one. Skipping is fine, that is what the
	// redundancy is for: the next packet carries this frame too.
	if (net_send_is_busy() == 1){
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

	net_build_header(NET_TYPE_INPUT);

	send_packet.payload.count      = (unsigned char)count;
	send_packet.payload.base_frame = newest_frame - (unsigned long)count + 1;

	entry = 0;
	while (entry < count){

		frame = send_packet.payload.base_frame + (unsigned long)entry;
		index = (unsigned int)(frame & (NET_INPUT_BUFFER_SIZE - 1));

		send_packet.payload.inputs[entry] = local_input_value[index];

		entry = entry + 1;

	}

	if (pending_checksum_ready == 1){

		send_packet.payload.has_checksum   = 1;
		send_packet.payload.checksum_frame = pending_checksum_frame;
		send_packet.payload.checksum_value = pending_checksum_value;

		pending_checksum_ready = 0;

	}

	net_transmit(remote_node);

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


//===========================================================
// Nothing at all for NET_TIMEOUT_SECONDS means the other machine has gone.
//
// Needed because the wait for an input has no other way out: without a
// timeout the game would sit there for ever if the other side crashed or
// somebody closed the window.
//===========================================================
int net_connection_lost(void){

	long now_tick;

	if (net_is_running == 0){
		return 0;
	}

	if (connection_lost == 1){
		return 1;
	}

	now_tick = biostime(0, 0L);

	if (now_tick - last_packet_tick > NET_TIMEOUT_SECONDS * NET_TICKS_PER_SECOND){

		sprintf(net_log_text, "NET: connection lost at frame %lu", simulation_frame);
		tanks_log(net_log_text);

		connection_lost = 1;

		return 1;

	}

	return 0;

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
