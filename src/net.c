#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <bios.h>
#include <string.h>
#include <stdlib.h>
#include "header\net.h"

//===========================================================
// Sending bytes from one program to another, on top of IPX.
//
// This file is a LIBRARY: it names no program, knows no game, and can be
// copied into another project together with header\net.h and nothing else.
// What travels and what it means is decided entirely by the caller.
//
// It is split in two layers, bottom to top:
//
//   1. Talking to the IPX driver at all   (the ipx_* functions)
//   2. Our envelope on top of IPX         (net_send / net_receive)
//
// The lockstep synchronisation this grew out of is NOT here any more: it
// lives in lockstep.c, on top of these functions, because keeping frames of
// keys in step is one particular program's problem and not the network's.
//
// No printf outside the pairing functions, which run in text mode on purpose
// and say so in net.h. Everything else goes to whatever net_set_log() was
// given, and nowhere at all if it was never called.
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
// application; it just has to be the SAME in both programs, since it is what
// tells our packets apart from anything else on the wire.
//
// Change it if you want two different programs of yours to be able to run on
// the same network without hearing each other.
//
// It travels big endian (high byte first), which is the opposite of how the
// 8086 stores an int, hence net_swap16() everywhere a socket is written.
#define NET_SOCKET_NUMBER 				0x869C

// Packet type 4 is "PEP", an ordinary unsequenced datagram: fire and forget,
// no acknowledgement, no ordering. Exactly what this library promises.
#define IPX_PACKET_TYPE 				4

// How many receive buffers are left posted with the driver at once.
//
// More than one on purpose: while we are dealing with a packet its buffer
// belongs to us, not to the driver, and a packet arriving right then would
// be dropped if it were the only one.
#define NET_LISTEN_ECB_COUNT 			4

// What kind of packet this is. HELLO and HELLO_ACK are the library's own
// pairing chatter and never reach the caller; DATA is everything a program
// ever sends.
#define NET_TYPE_HELLO 					1
#define NET_TYPE_HELLO_ACK 				2
#define NET_TYPE_DATA 					3

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
// they will have to catch up on once they start.
#define NET_HELLO_INTERVAL_TICKS 		5L


//===========================================================
// The Event Control Block: the form you fill in for the driver.
//
// You hand it one of these, it does the job, and it sets in_use back to 0
// when it has finished. That is the whole conversation.
//
// The layout is fixed by Novell down to the byte, so nothing here may be
// reordered, resized or padded. Turbo C aligns structures on bytes by
// default (-a-), which is what we need; net_start() logs the sizes so a
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
// Our own envelope, wrapped round whatever the caller handed us.
//
// 12 bytes in front of the data, and the IPX header in front of that is 30
// more. Worth knowing when you decide how much to put in one net_send():
// sending 3 bytes costs a 45 byte packet, and so does sending 30.
//===========================================================
struct net_envelope {

	unsigned char  magic[4];				// "CTRE", so we ignore other traffic
	unsigned char  type;					// NET_TYPE_*
	unsigned char  reserved;				// keeps the long below on an even offset
	unsigned long  instance_id;				// who sent it
	unsigned int   length;					// how many bytes of data[] are real
	unsigned char  data[NET_MAX_DATA];		// the caller's bytes, untouched

};

struct net_packet {

	struct ipx_header   header;
	struct net_envelope envelope;

};

// Everything in a packet that is not the caller's data
#define NET_PACKET_OVERHEAD (sizeof(struct net_packet) - NET_MAX_DATA)


// Where the driver lives. Found once by ipx_detect() and far called from
// then on. NOT an interrupt: IPX is entered with a far call.
static unsigned int ipx_entry_segment;
static unsigned int ipx_entry_offset;

// 1 once net_start() has found the driver and opened the socket
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

// Messages that have arrived and are waiting for net_receive() to collect
// them. A ring: head is where the next one goes in, tail is where the next
// one comes out.
static unsigned char queue_data[NET_QUEUE_SIZE][NET_MAX_DATA];
static unsigned int  queue_length[NET_QUEUE_SIZE];
static int queue_head = 0;
static int queue_tail = 0;
static int queue_count = 0;

// Broadcast: "every node on this network"
static unsigned char broadcast_node[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Us, and them
static unsigned char local_node[6];
static unsigned char remote_node[6];

// A number picked at random when the program starts. It does two jobs: it
// tells our own packets from theirs, and the caller can use the two of them
// to settle things with no negotiation, see net_get_local_id() in net.h.
static unsigned long local_instance_id;
static unsigned long remote_instance_id;

// Bit 0x01 = we have heard from them. Bit 0x02 = they have answered us.
static int pairing_state = 0;
static int is_connected = 0;

// BIOS tick when the last packet arrived, for the timeout
static long last_packet_tick;
static int  connection_lost = 0;

// Counters for the log, so a bad run can be read afterwards
static unsigned long total_packets_sent;
static unsigned long total_packets_received;
static unsigned long total_messages_dropped;

static char net_log_text[100];


// Where this library reports problems. NULL, the default, means nowhere.
//
// This is what keeps this file free of any program's code. Without it, net.c
// would have to #include the log of one particular program and could not be
// copied into the next one as it is. The caller hands over its own function
// once, and net.c has no idea what that function does with the text.
static void (*net_log_function)(char *message) = NULL;


static void net_log(char *message){

	if (net_log_function == NULL){
		return;
	}

	net_log_function(message);

}


void net_set_log(void (*log_function)(char *message)){

	net_log_function = log_function;

}


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
// node. We only care about the node, and only to put it in the log.
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
	// whatever the program was doing, with all the reentrancy problems that
	// brings. Looking at in_use once a loop is enough and cannot go wrong.
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
// This MUST be asked before the next payload is built, not just before it is
// sent: while a send is in flight the driver owns send_packet, and writing
// the next message into it would rewrite a packet already on its way out.
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
// Sends whatever is already sitting in send_packet.envelope to one node.
//
// Only the bytes that are really used travel: a 3 byte message is a 45 byte
// packet, not a 522 byte one. IPX is told the true length twice, once in its
// own header and once in the ECB fragment, and both have to agree.
//===========================================================
static int net_transmit(unsigned char *destination_node, unsigned int data_length){

	unsigned int packet_size;

	if (net_is_running == 0){
		return 0;
	}

	if (send_ecb.in_use != 0){
		return 0;
	}

	packet_size = (unsigned int)NET_PACKET_OVERHEAD + data_length;

	// ---- The IPX header: who it is going to ----

	memset(&send_packet.header, 0, sizeof(struct ipx_header));

	send_packet.header.checksum    = 0xFFFF;
	send_packet.header.length      = net_swap16(packet_size);
	send_packet.header.packet_type = IPX_PACKET_TYPE;

	// Network 0 means "the one I am on". Anything else would need a router,
	// and there is none between two machines on the same wifi.
	memset(send_packet.header.destination_network, 0, 4);
	memcpy(send_packet.header.destination_node, destination_node, 6);

	send_packet.header.destination_socket[0] = (NET_SOCKET_NUMBER >> 8) & 0x00FF;
	send_packet.header.destination_socket[1] = NET_SOCKET_NUMBER & 0x00FF;

	// ---- The form for the driver ----

	memset(&send_ecb, 0, sizeof(struct ipx_ecb));

	send_ecb.esr_address      = (void far (*)())0;
	send_ecb.socket_number    = net_swap16(NET_SOCKET_NUMBER);
	send_ecb.fragment_count   = 1;
	send_ecb.fragment_address = (void far *)&send_packet;
	send_ecb.fragment_size    = packet_size;

	// On a flat network the next hop IS the destination. A router would need
	// IPX function 2 to work out something different, and there is no router.
	memcpy(send_ecb.immediate_address, destination_node, 6);

	ipx_call(IPX_FUNCTION_SEND, (void far *)&send_ecb);

	total_packets_sent = total_packets_sent + 1;

	return 1;

}


//===========================================================
// Fills in the part of the envelope that every packet carries
//===========================================================
static void net_build_envelope(unsigned char packet_type, unsigned int data_length){

	send_packet.envelope.magic[0] = NET_MAGIC_0;
	send_packet.envelope.magic[1] = NET_MAGIC_1;
	send_packet.envelope.magic[2] = NET_MAGIC_2;
	send_packet.envelope.magic[3] = NET_MAGIC_3;

	send_packet.envelope.type        = packet_type;
	send_packet.envelope.reserved    = 0;
	send_packet.envelope.instance_id = local_instance_id;
	send_packet.envelope.length      = data_length;

}


//===========================================================
// Is this one of ours, and is it worth looking at?
//===========================================================
static int net_packet_is_valid(struct net_packet *packet){

	if (packet->envelope.magic[0] != NET_MAGIC_0){
		return 0;
	}

	if (packet->envelope.magic[1] != NET_MAGIC_1){
		return 0;
	}

	if (packet->envelope.magic[2] != NET_MAGIC_2){
		return 0;
	}

	if (packet->envelope.magic[3] != NET_MAGIC_3){
		return 0;
	}

	// Our own broadcast finding its way back to us. On real Ethernet a card
	// does not hear itself and this never happens, but it costs nothing to
	// be sure.
	if (packet->envelope.instance_id == local_instance_id){
		return 0;
	}

	// A length longer than the buffer it came in means the packet is corrupt
	// or somebody else is using our socket. Either way it is not ours.
	if (packet->envelope.length > NET_MAX_DATA){
		return 0;
	}

	return 1;

}


//===========================================================
// LAYER 2 - MESSAGES
//===========================================================

//===========================================================
// Puts one arrived message at the back of the queue.
//
// If the queue is full the OLDEST is thrown away rather than the new one. On
// a link that is falling behind, the newest news is the news worth keeping,
// and a program that reads its messages every loop never sees this happen.
//===========================================================
static void net_queue_push(unsigned char *data, unsigned int length){

	if (queue_count == NET_QUEUE_SIZE){

		queue_tail = (queue_tail + 1) % NET_QUEUE_SIZE;
		queue_count = queue_count - 1;

		total_messages_dropped = total_messages_dropped + 1;

	}

	if (length > 0){
		memcpy(queue_data[queue_head], data, length);
	}

	queue_length[queue_head] = length;

	queue_head = (queue_head + 1) % NET_QUEUE_SIZE;
	queue_count = queue_count + 1;

}


//===========================================================
// Deals with one packet that has arrived
//===========================================================
static void net_handle_packet(struct net_packet *packet){

	total_packets_received = total_packets_received + 1;
	last_packet_tick = biostime(0, 0L);

	if (packet->envelope.type == NET_TYPE_HELLO){

		// Somebody is looking for us. Remember who they are and answer them
		// directly, so they know we are here too.
		//
		// This is answered ALWAYS, not only while pairing. A program that is
		// already up and running is exactly what a late arrival needs to
		// hear from, and it is also what covers an answer of ours that got
		// lost: they simply ask again and this replies again.
		remote_instance_id = packet->envelope.instance_id;
		memcpy(remote_node, packet->header.source_node, 6);

		if (net_send_is_busy() == 0){
			net_build_envelope(NET_TYPE_HELLO_ACK, 0);
			net_transmit(remote_node, 0);
		}

		pairing_state = pairing_state | 0x01;

		return;

	}

	if (packet->envelope.type == NET_TYPE_HELLO_ACK){

		remote_instance_id = packet->envelope.instance_id;
		memcpy(remote_node, packet->header.source_node, 6);

		pairing_state = pairing_state | 0x02;

		return;

	}

	if (packet->envelope.type == NET_TYPE_DATA){

		net_queue_push(packet->envelope.data, packet->envelope.length);

		return;

	}

}


void net_update(void){

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


int net_send(void *data, int length){

	if (net_is_running == 0){
		return 0;
	}

	if (is_connected == 0){
		return 0;
	}

	// Refused rather than chopped in half. Silently sending the first
	// NET_MAX_DATA bytes would look like it worked and lose the rest.
	if (length < 0 || length > NET_MAX_DATA){
		net_log("NET: message too long, raise NET_MAX_DATA or send it in pieces");
		return 0;
	}

	// The driver is still holding the previous packet. Dropping this one is
	// the whole reason net_send() never blocks: the caller gets a 0 and
	// decides for itself whether that matters.
	if (net_send_is_busy() == 1){
		return 0;
	}

	net_build_envelope(NET_TYPE_DATA, (unsigned int)length);

	if (length > 0){
		memcpy(send_packet.envelope.data, data, (unsigned int)length);
	}

	return net_transmit(remote_node, (unsigned int)length);

}


int net_receive(void *buffer, int max_length){

	unsigned int length;
	int slot;

	if (queue_count == 0){
		return 0;
	}

	// The slot is taken out of the queue FIRST and read afterwards. Reading
	// it through queue_tail after moving queue_tail on would read the next
	// message instead of this one.
	slot = queue_tail;
	length = queue_length[slot];

	queue_tail = (queue_tail + 1) % NET_QUEUE_SIZE;
	queue_count = queue_count - 1;

	// Thrown away rather than truncated. Half a message looks exactly like a
	// whole one to the caller, and that is a bug that takes days to find.
	if (max_length < 0 || length > (unsigned int)max_length){
		net_log("NET: a message did not fit in the buffer and was dropped");
		return 0;
	}

	if (length > 0){
		memcpy(buffer, queue_data[slot], length);
	}

	return (int)length;

}


//===========================================================
// STARTING AND STOPPING
//===========================================================

int net_start(void){

	int index;

	net_is_running  = 0;
	is_connected    = 0;
	connection_lost = 0;
	pairing_state   = 0;

	queue_head  = 0;
	queue_tail  = 0;
	queue_count = 0;

	total_packets_sent     = 0;
	total_packets_received = 0;
	total_messages_dropped = 0;

	// A structure that is the wrong size means the compiler has padded it,
	// and IPX would then read every field from the wrong place. Logging the
	// sizes turns that from a baffling crash into one obvious line.
	sprintf(net_log_text, "NET sizes: ecb=%u header=%u overhead=%u (want 42/30/42)",
	        (unsigned int)sizeof(struct ipx_ecb),
	        (unsigned int)sizeof(struct ipx_header),
	        (unsigned int)NET_PACKET_OVERHEAD);
	net_log(net_log_text);

	if (ipx_detect() == 0){
		net_log("NET: no IPX driver found (int 2F/7A00 said no)");
		return 0;
	}

	sprintf(net_log_text, "NET: IPX driver entry at %04X:%04X",
	        ipx_entry_segment, ipx_entry_offset);
	net_log(net_log_text);

	if (ipx_socket_call(IPX_FUNCTION_OPEN_SOCKET, net_swap16(NET_SOCKET_NUMBER)) != 0){
		net_log("NET: could not open the socket, another copy may be running");
		return 0;
	}

	net_is_running = 1;

	ipx_get_local_address();

	// Our id for this run. Whoever draws the lower one is player 1, so two
	// machines drawing the SAME one is not a curiosity, it is a hang: the
	// comparison is "lower than", which is false on both sides at once, so
	// both believe they are player 2. Nobody offers a level and the game
	// refuses to start; and the intro handshake used to deadlock the same
	// way, with both ends waiting for an intro nobody was showing.
	//
	// And it is not far fetched. The seed is the BIOS tick, which moves 18.2
	// times a second and counts from midnight, so two machines whose clocks
	// agree and that are started within an eighteenth of a second of each
	// other draw exactly the same number.
	//
	// So the node address goes into it. IPX gives every machine a different
	// one (DOSBox builds it from the IP address), which means two ids can
	// only collide now if the random halves collide AND the nodes match,
	// and the nodes cannot match.
	srand((unsigned int)biostime(0, 0L));
	local_instance_id = ((unsigned long)rand() << 16) | (unsigned long)rand();

	local_instance_id = local_instance_id
	                  ^ (((unsigned long)local_node[2] << 24)
	                   | ((unsigned long)local_node[3] << 16)
	                   | ((unsigned long)local_node[4] <<  8)
	                   |  (unsigned long)local_node[5]);

	sprintf(net_log_text, "NET: node %02X%02X%02X%02X%02X%02X id %lu",
	        local_node[0], local_node[1], local_node[2],
	        local_node[3], local_node[4], local_node[5],
	        local_instance_id);
	net_log(net_log_text);

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


void net_end(void){

	if (net_is_running == 0){
		return;
	}

	sprintf(net_log_text, "NET: sent %lu, received %lu, dropped %lu",
	        total_packets_sent, total_packets_received, total_messages_dropped);
	net_log(net_log_text);

	ipx_socket_call(IPX_FUNCTION_CLOSE_SOCKET, net_swap16(NET_SOCKET_NUMBER));

	net_is_running = 0;
	is_connected   = 0;

}


//===========================================================
// FINDING THE OTHER MACHINE
//===========================================================

//===========================================================
// The one loop all three pairing functions are made of.
//
// It shouts HELLO every quarter of a second if it is supposed to shout, and
// waits until pairing_state has the bits it was told to wait for. Answering
// somebody else's HELLO is not done here: net_handle_packet() does that
// always, whether we are pairing or not.
//
//   wanted 0x01  "I have heard from them"    - the server, which only listens
//   wanted 0x02  "they have answered me"     - the client, which shouts
//   wanted 0x03  both                        - two equals finding each other
//
// Returns 1 when paired, 0 on timeout or if a key was pressed.
//===========================================================
static int net_pair(int seconds, int do_broadcast, int wanted_state){

	long start_tick;
	long now_tick;
	long next_hello_tick;
	long limit_ticks;

	if (net_is_running == 0){
		return 0;
	}

	limit_ticks = (long)seconds * NET_TICKS_PER_SECOND;

	start_tick      = biostime(0, 0L);
	next_hello_tick = start_tick;

	while ((pairing_state & wanted_state) != wanted_state){

		now_tick = biostime(0, 0L);

		if (do_broadcast == 1){

			if (now_tick >= next_hello_tick){

				next_hello_tick = now_tick + NET_HELLO_INTERVAL_TICKS;

				if (net_send_is_busy() == 0){
					net_build_envelope(NET_TYPE_HELLO, 0);
					net_transmit(broadcast_node, 0);
				}

				printf(".");

			}

		}

		net_update();

		if (now_tick - start_tick > limit_ticks){
			printf("\n\nNobody answered.\n");
			net_log("NET: pairing timed out");
			return 0;
		}

		if (kbhit()){
			getch();
			printf("\n\nCancelled.\n");
			net_log("NET: pairing cancelled by the user");
			return 0;
		}

	}

	is_connected    = 1;
	last_packet_tick = biostime(0, 0L);
	connection_lost = 0;

	printf("\n\nConnected.\n");

	sprintf(net_log_text, "NET: paired with id %lu node %02X%02X%02X%02X%02X%02X",
	        remote_instance_id,
	        remote_node[0], remote_node[1], remote_node[2],
	        remote_node[3], remote_node[4], remote_node[5]);
	net_log(net_log_text);

	return 1;

}


int net_wait_for_client(int seconds){

	printf("\n");
	printf("Waiting for the other machine to connect...\n");
	printf("(press any key to give up)\n");
	printf("\n");

	// Listens only. It is paired the moment it has heard a HELLO, because
	// net_handle_packet() has already answered it by then. If that answer
	// got lost the client simply asks again, and it is answered again.
	return net_pair(seconds, 0, 0x01);

}


int net_connect_to_server(int seconds){

	printf("\n");
	printf("Looking for the server...\n");
	printf("(press any key to give up)\n");
	printf("\n");

	// Shouts, and waits for an answer. Waiting for the ANSWER and not just
	// for any packet is what makes sure the server knows about us too.
	return net_pair(seconds, 1, 0x02);

}


int net_find_peer(int seconds){

	printf("\n");
	printf("Looking for the other machine...\n");
	printf("(both must be running the program, press any key to give up)\n");
	printf("\n");

	// Both sides do exactly the same thing: shout, answer whoever shouts,
	// and wait until they have BOTH heard a HELLO and had their own answered.
	//
	// Waiting for both bits is not fussiness. Pairing on the first HELLO
	// alone would let the faster machine run off and start while the slower
	// one was still waiting for an answer that was never coming.
	return net_pair(seconds, 1, 0x03);

}


int net_is_connected(void){

	return is_connected;

}


unsigned long net_get_local_id(void){

	return local_instance_id;

}


unsigned long net_get_remote_id(void){

	return remote_instance_id;

}


//===========================================================
// Nothing at all for NET_TIMEOUT_SECONDS means the other machine has gone.
//
// Needed because a wait for a message has no other way out: without a
// timeout a program would sit there for ever if the other side crashed or
// somebody closed the window.
//===========================================================
int net_connection_lost(void){

	long now_tick;

	if (net_is_running == 0){
		return 0;
	}

	if (is_connected == 0){
		return 0;
	}

	if (connection_lost == 1){
		return 1;
	}

	now_tick = biostime(0, 0L);

	if (now_tick - last_packet_tick > NET_TIMEOUT_SECONDS * NET_TICKS_PER_SECOND){

		net_log("NET: connection lost");

		connection_lost = 1;

		return 1;

	}

	return 0;

}
