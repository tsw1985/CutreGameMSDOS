#ifndef NET
#define NET

//===========================================================
// NETWORK LIBRARY
//
// Sends a handful of bytes from one program to another, over the network, on
// top of IPX. It knows nothing about any particular program: you hand it
// some bytes, they come out the other side.
//
// WHY IPX AND NOT SOCKETS: Turbo C++ 3.0 has no networking at all, and DOS
// has no TCP/IP either. In DOS the network is a driver you load before the
// program, and IPX is the one that needs no stack on top: you fill a
// structure in memory, tell the driver "send this", and it goes. It is what
// Doom used.
//
// Nothing here is DOSBox specific. DOSBox implements the same IPX calls a
// real driver does, so the very same .exe runs on two DOSBoxes talking over
// UDP, or on two real machines with real cards and IPXODI loaded. No
// recompiling, no #ifdef.
//
// If there is no driver, net_start() returns 0 and every other call here
// does nothing at all. A program using this runs exactly the same, on its
// own, with no special case anywhere in it.
//
//
// HOW TO USE IT IN A NEW PROGRAM
//
//   1. Once, at the start:
//
//          net_set_log(my_log_function);   /* optional, see below */
//
//          if (net_start() == 1){
//
//              /* ONE of the two machines does this... */
//              net_wait_for_client(30);
//
//              /* ...and the other one does this */
//              net_connect_to_server(30);
//
//          }
//
//   2. Once per loop, ALWAYS:
//
//          net_update();
//
//   3. Whenever you want to say something:
//
//          net_send(&my_data, sizeof(my_data));
//
//   4. And to pick up whatever has arrived, until it says 0:
//
//          while (net_receive(&buffer, sizeof(buffer)) > 0){
//              ...
//          }
//
//   5. Before leaving the program, without fail:
//
//          net_end();
//
//
// WHAT TRAVELS, AND WHAT DOES NOT
//
// One net_send() is one MESSAGE, not a stream of bytes. If you send 3 bytes
// the other side receives exactly 3 bytes in one net_receive(). Two sends of
// 3 bytes never arrive glued together as 6, and one send of 400 bytes never
// arrives split in two. This is not TCP: there is no stream to cut up.
//
// What you do NOT get, and TCP would have given you:
//
//   - No guarantee it arrives.   A message can be lost and nobody notices.
//   - No guarantee of order.     Two messages can arrive the other way round.
//   - No connection.             "Connected" here only means the two sides
//                                have found each other and know where to aim.
//
// That sounds worse than it is. On a LAN, and on DOSBox, losses are rare.
// For a game it is exactly what you want: a lost frame of keys is covered by
// the next packet, and waiting for a retransmission would be worse than the
// loss. For sending a FILE it is not enough on its own, and the tutorial
// shows the twenty lines of numbering and acknowledgement that fix it.
//===========================================================


// The most that fits in one message.
//
// IPX guarantees 546 bytes for the whole packet and its 30 byte header comes
// out of that. What is left, minus our own little envelope, is this. Ask to
// send more and net_send() refuses and returns 0 rather than quietly
// chopping your data in half.
#define NET_MAX_DATA 			480

// How many arrived messages are kept waiting for net_receive() to collect
// them.
//
// It exists because the driver can hand us several packets between two turns
// round your loop, and net_receive() gives you one at a time. If your loop
// is slow and they pile up past this, the OLDEST is dropped: on a link that
// is falling behind, the newest news is the news worth keeping.
#define NET_QUEUE_SIZE 			8

// Seconds without a single packet before net_connection_lost() gives up.
#define NET_TIMEOUT_SECONDS 	10


// ---------- Starting and stopping ----------

// Finds the IPX driver and opens our socket. Returns 1 if there is network,
// 0 if there is not (no driver loaded, or the socket was busy).
//
// It connects to nobody: who talks to whom is the program's business. Call
// one of the three functions below once this has returned 1.
int net_start(void);

// Closes the socket and gives the driver back what is ours. MUST be called
// before leaving the program, or the driver keeps our socket and our buffers
// and the next run cannot open the same socket.
void net_end(void);

// Picks up whatever the driver has for us and hands its buffers back.
//
// Has to be called once per loop, and also inside any loop that waits for
// something: it is the ONLY thing that ever moves data in, so while it is
// not being called nothing arrives, however much the other side sends. It is
// cheap and safe to call as often as you like.
void net_update(void);


// ---------- Finding the other machine ----------
//
// Nobody types an address anywhere. IPX has broadcast, so the two copies
// find each other on their own.
//
// IPX has no such thing as a server or a client: there is no connection to
// accept. The two functions below are a CONVENTION on top of it, and the
// only difference between them is who shouts and who listens. Once they have
// found each other the two sides are identical and either can send whenever
// it likes.
//
// All three of these run in TEXT mode and print what is going on, so call
// them before switching to graphics.

// SERVER: waits, quietly, for a client to turn up, and answers it.
// Returns 1 when somebody has arrived, 0 on timeout or if a key was pressed.
int net_wait_for_client(int seconds);

// CLIENT: shouts for a server four times a second until one answers.
// Returns 1 when it has found one, 0 on timeout or if a key was pressed.
int net_connect_to_server(int seconds);

// NEITHER: both sides run this one and whoever hears the other first does
// the answering. Use it when the two copies are the same program and there
// is no reason for one of them to be in charge, which is the usual case for
// a two player game. Returns 1 when paired.
int net_find_peer(int seconds);

// 1 once we know who the other machine is and can send to it.
int net_is_connected(void);


// ---------- Sending and receiving ----------

// Sends up to NET_MAX_DATA bytes to the other machine. Returns 1 if it was
// handed to the driver, 0 if it was not.
//
// It NEVER waits. If the driver is still busy with the previous packet this
// one is dropped and 0 comes back, which on a fast loop happens now and
// again and is normal. If losing it matters, look at what it returned.
int net_send(void *data, int length);

// Takes the oldest message that has arrived, copies it into your buffer, and
// returns how many bytes it was. Returns 0 when there is nothing waiting.
//
// A message longer than max_length is thrown away rather than truncated, so
// you can never be handed half of something and take it for the whole.
//
// Call it in a loop until it returns 0: more than one message can arrive
// between two turns round yours.
int net_receive(void *buffer, int max_length);


// ---------- Who is who ----------

// A number this copy picked at random when net_start() ran, and the other
// machine's, learned while pairing.
//
// They are here for deciding things with no negotiation at all: both sides
// compare the same two numbers and reach the same answer on their own. That
// is how the game settles who drives which tank, without a single packet
// being spent on the question.
unsigned long net_get_local_id(void);
unsigned long net_get_remote_id(void);


// ---------- Trouble ----------

// 1 when nothing at all has arrived for NET_TIMEOUT_SECONDS: the other
// machine is gone. Without this a program waiting for a message would wait
// for ever if the other side crashed or somebody closed the window.
int net_connection_lost(void);

// Where this library should report problems. Optional: with no log function
// it stays completely quiet, which is the default.
//
// It exists because a program in graphics mode cannot have anything printed
// to the screen. It is also what keeps this file free of any program of
// yours: hand it your own log function once and net.c never needs to know
// anything about you, which is what makes it copyable into the next program
// as it is.
void net_set_log(void (*log_function)(char *message));


#endif
