#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <string.h>
#include <bios.h>
#include "header\util.h"
#include "header\bmp.h"
#include "header\players.h"
#include "header\sound.h"
#include "header\net.h"

//===========================================================
// The game: main loop, keyboard, collisions against the map, and drawing.
//
// Everything that needs to know about the map or the screen lives here.
// players.c only deals with the geometry of a tank.
//===========================================================

// Loop iterations between track animation frames
#define FRAMES_COUNTER	3

// The loop waits for one vertical retrace per iteration and mode 13h runs
// at about 70 Hz, so 70 iterations is roughly one second. Approximate: it
// follows the video card, not a clock.
#define LOG_INTERVAL_FRAMES 70

#define SCREEN_SIZE 			64000

// Palette index that marks a wall in cutrecol.bmp. That file only uses 2
// colors: 3 (floor) and 252 (wall).
#define COLLISION_COLOR 		252

// Keyboards Directions
#define DIRECTION_UP			0
#define DIRECTION_DOWN	1
#define DIRECTION_LEFT		2
#define DIRECTION_RIGHT		3

// Keyboard hardware ports
#define KEY_BUFFER 0x60

// Scan codes. Player 1 drives with the cursor keys and fires with the 5 of
// the numeric keypad, player 2 with W/A/S/D and G.
//
// The keypad 5 is a clean key for this: it sends a single 0x4C, with no
// 0xE0 prefix and no twin anywhere else on the keyboard, and the Num Lock
// state does not matter because the keyboard always sends that same code.
// Num Lock is something the BIOS interprets, and new_kbd_handler() reads
// the raw scan code straight from port 0x60 without going through it.
#define KEY_UP			0x48
#define KEY_DOWN		0x50
#define KEY_LEFT		0x4B
#define KEY_RIGHT		0x4D
#define KEY_ESC			0x01
#define KEY_NUMPAD_5	0x4C

#define KEY_W			0x11
#define KEY_A			0x1E
#define KEY_S			0x1F
#define KEY_D			0x20
#define KEY_G			0x22

#define IRQ_KEYBOARD	9

// One slot per scan code: 1 while the key is held, 0 when released. Filled
// by new_kbd_handler() behind our back, on every keyboard interrupt.
unsigned char keys[128] = {0};


// function pointer to save the old keyboard handler
void interrupt far (*old_kbd_handler)();
void interrupt far new_kbd_handler();

extern void hola();
extern void reset_pic();

extern set_vga_320_200_mode();
void setup_screen();
void init_graphics();
void init_players();
int is_blocked_by_wall(struct player *_player);
int is_blocked_by_tank(struct player *_player, struct player *_other);
int is_move_blocked(struct player *_player, struct player *_other);
int bullet_has_hit_tank(struct player *_player, struct player *_other);
void restart_game();
void wait_retrace();
void update_game(int direction);
void update_player_animation(struct player *_player);
void move_sprite(struct player *_player, int direction);
int update_bullet(struct player *_player, struct player *_other);
void process_player_input(struct player *_player,
                          struct player *_other,
                          unsigned char input_bits);
unsigned char read_input_from_keys(unsigned char key_up_code,
                                   unsigned char key_down_code,
                                   unsigned char key_left_code,
                                   unsigned char key_right_code,
                                   unsigned char key_fire_code);
unsigned int compute_state_checksum();
void set_text_mode();
void draw_to_buffer();
void draw_explosion(struct player *_player);
void update_keyboard();

/*  Players */
struct player player1;
struct player player2;

// Counts loop iterations to pace the track animation, shared by both tanks
// so they roll at the same rhythm
int frame_counter;

// Iterations left before the round restarts after a hit. While it is above
// 0 the round is FROZEN: no keyboard, no bullets, only the explosion moves.
// 0 = the round is running normally.
unsigned int explosion_pause_counter;

// Iterations since the last line written to the log. Kept apart from
// frame_counter so throttling the log does not touch the animation speed.
int log_frame_counter;

// 1 when the game is being played against another machine, 0 for the two
// players on this same keyboard. Set from the command line: game.exe /net
int network_mode;

// Which tank THIS machine drives over the network. Meaningless in local
// mode, where this keyboard drives both of them.
int local_player_is_1;

// Install our custom interruption vector
void install_kbd()   {
	old_kbd_handler = getvect(IRQ_KEYBOARD);
	setvect(IRQ_KEYBOARD, new_kbd_handler);
}

void uninstall_kbd() {
	setvect(IRQ_KEYBOARD, old_kbd_handler);
}


void interrupt far new_kbd_handler() {

	unsigned char scancode;

    asm {
	    in  al, 0x60      /* Read scan code by 0x60 Hardware Port */
        mov scancode, al
    }

    // When a key is pressed, the scancode is between 0 and 128
    // When a key is released the scancode is > 128.
    if (scancode & 0x80){ // if bit 7 is 1 , means key released
    	// is need substract 128 to access to the correct index and set a 0
        keys[scancode - 128] = 0;  /* Set to 0 like key released */
	}else{
        keys[scancode] = 1;          /* Set to 1 like pressed key */
    }

    /* Reset 8042 Controller and weak PIC */
    reset_pic();

}



int main(int argc, char *argv[]){

	char log_message_text[64];			// text of the log line being built
	unsigned int cannon_tip_pixel_value;	// map color under the cannon tip, for the log

	// Raised when a bullet has hit a tank this frame. Checked after BOTH
	// bullets have been dealt with, so if they shoot each other on the same
	// frame both shots count.
	unsigned int tank_was_hit;

	// The keys driving each tank this frame. Everything downstream works on
	// these two bytes, so it never has to know whether they came from this
	// keyboard or down the wire.
	unsigned char player1_input;
	unsigned char player2_input;
	unsigned char local_input;

	// Raised when the other machine has gone, so the loop can get out
	int connection_was_lost;

	int argument_index;

	// BIOS tick to stop the "connected" message at, so it can be read
	long message_until_tick;

	// Start each run with an empty log instead of mixing runs
	tanks_log_clear();

	tanks_log("Starting game ...");

	// game.exe /net plays against another machine. game.exe on its own is
	// the two players on one keyboard game that was here before.
	network_mode        = 0;
	local_player_is_1   = 1;
	connection_was_lost = 0;

	argument_index = 1;
	while (argument_index < argc){

		if (stricmp(argv[argument_index], "/net") == 0){
			network_mode = 1;
		}

		if (stricmp(argv[argument_index], "-net") == 0){
			network_mode = 1;
		}

		argument_index = argument_index + 1;

	}

	// The two machines find each other BEFORE the screen is switched to
	// VGA, on purpose: in graphics mode there is nowhere to print, and this
	// is exactly the part that needs to be able to say what is going on.
	//
	// The custom INT 9 handler is not installed yet either, so plain kbhit()
	// and getch() still work here.
	if (network_mode == 1){

		tanks_log("Network mode");

		if (net_init() == 0){
			printf("\nNo IPX driver found.\n\n");
			printf("In DOSBox: put ipx=true in dosbox.conf, then run\n");
			printf("  ipxnet startserver        on one machine\n");
			printf("  ipxnet connect <its ip>   on the other\n\n");
			printf("On real DOS: load LSL, your card's ODI driver and IPXODI first.\n");
			return 1;
		}

		if (net_find_opponent() == 0){
			net_shutdown();
			return 1;
		}

		local_player_is_1 = net_is_player1();

		// Two seconds to read the message, and NOT a keypress. Both machines
		// start at frame 0 and the first one there simply waits for the other,
		// which lockstep handles fine as long as the wait is short. Waiting
		// for a key would make it as long as the other player takes to press
		// one, and the connection would time out first.
		//
		// The network is still being read in here: their packets are already
		// arriving, and a buffer that is not picked up is a packet dropped.
		message_until_tick = biostime(0, 0L) + 36L;
		while (biostime(0, 0L) < message_until_tick){
			net_poll();
		}

	}

	// Instal custom Vector ( INT 9 ) keyboard
	install_kbd();

	// Init Players and buffers
	setup_screen();
	init_players();
	init_graphics();

	// Sound is optional: if there is no card sound_init() returns 0, says so
	// in the log, and every later sound call does nothing. The game plays
	// exactly the same, in silence.
	sound_init();

	// The first round starts running, not burning
	explosion_pause_counter = 0;

	//main loop

    do{

		// 0. The keys for BOTH tanks this frame.
		//
		// This happens on every single frame, explosion pause included. Over
		// the network the two machines have to keep stepping through the same
		// frame numbers even while nothing on screen is moving, or one of them
		// would sit waiting for an input the other one never sent.
		if (network_mode == 1){

			// Our own keys go IN at frame + NET_INPUT_DELAY and come back OUT
			// at the current frame, so they are applied exactly as late as the
			// other machine's. Applying them straight away would feel better
			// and desync within the first second.
			//
			// Both machines drive with the cursor keys here: it does not matter
			// which of the two tanks you were given.
			local_input = read_input_from_keys(KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_NUMPAD_5);

			net_set_local_input(local_input);
			net_send_input();

			if (net_has_remote_input() == 0){
				net_count_wait();
			}

			// Stand still until their keys for THIS frame turn up. That is the
			// price of lockstep, and NET_INPUT_DELAY is what keeps it from
			// being paid often.
			//
			// sound_update() is called in here on purpose: the wait is usually
			// a fraction of a frame, but one bad moment on the wifi would make
			// the card replay the same half buffer and stutter.
			while (net_has_remote_input() == 0){

				net_poll();
				sound_update();

				if (net_connection_lost() == 1){
					connection_was_lost = 1;
					break;
				}

			}

			if (connection_was_lost == 1){
				break;
			}

			net_poll();

			if (local_player_is_1 == 1){
				player1_input = net_get_local_input();
				player2_input = net_get_remote_input();
			}else{
				player1_input = net_get_remote_input();
				player2_input = net_get_local_input();
			}

		}else{

			player1_input = read_input_from_keys(KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_NUMPAD_5);
			player2_input = read_input_from_keys(KEY_W,  KEY_S,    KEY_A,    KEY_D,     KEY_G);

		}

		// The round has two states:
		//
		//   running   -> keyboard and bullets, the game itself
		//   exploding -> a tank has been hit: everything above is frozen for
		//                half a second while the explosion burns, so the
		//                survivor cannot drive and shoot over a dead tank
		if (explosion_pause_counter == 0){

			// 1. One call per player. Two calls means two separate if / else if
			// chains, so both tanks can move on the same frame. Each is told
			// about the other, so one tank stops the other just like a wall does.
			process_player_input(&player1, &player2, player1_input);
			process_player_input(&player2, &player1, player2_input);

			// 2. Move each bullet. Returns 1 if it hit the other tank, and
			// the one that blows up is the tank that was HIT, not the one
			// that fired: player 1's bullet hitting means player 2 explodes.
			tank_was_hit = 0;

			if (update_bullet(&player1, &player2) == 1){
				player_start_explosion(&player2);
				tank_was_hit = 1;
			}

			if (update_bullet(&player2, &player1) == 1){
				player_start_explosion(&player1);
				tank_was_hit = 1;
			}

			if (tank_was_hit == 1){

				sprintf(log_message_text, "Tank hit - wins %u / %u", player1.wins, player2.wins);
				tanks_log(log_message_text);

				// Put out any bullet still in the air, or it would hang
				// frozen in mid air for the whole pause
				player1.bullet_is_flying = 0;
				player2.bullet_is_flying = 0;

				// Cut both engines. The keyboard is not read during the
				// pause, so nothing else would ever turn them off and they
				// would keep looping while the tanks burn.
				sound_stop(player1.sound_engine_voice);
				sound_stop(player2.sound_engine_voice);

				// The bang. One single voice, so two tanks dying on the same
				// frame is one explosion and not two on top of each other.
				// It is longer than the pause and nothing cuts it, so it
				// carries on ringing into the start of the new round.
				sound_play(SOUND_VOICE_EXPLOSION, SOUND_SAMPLE_DIED, SOUND_VOLUME_DIED);

				// Start the pause. The round is NOT restarted here: the
				// tanks stay where they were shot, so the explosion can be
				// drawn on top of them.
				explosion_pause_counter = EXPLOSION_TOTAL_FRAMES;

			}

		}else{

			// Burning: only the explosion animation moves
			player_update_explosion(&player1);
			player_update_explosion(&player2);

			explosion_pause_counter = explosion_pause_counter - 1;

			if (explosion_pause_counter == 0){
				restart_game();
			}

		}

		// Refill whichever half of the sound buffer the card has finished.
		// It is outside the two states above on purpose: the sound has to
		// keep running during the explosion pause too, or it would stutter.
		sound_update();

		// 2. Update logic game
   		update_game(0);

   		// 3. Double buffering
   		draw_to_buffer();

   		// 4. Wait vertial retrace
   		wait_retrace();

   		// 5. Show new map in screen
   		bmp_paint_image_data_to_vga(buffer_background_image_data);


   		// 6. Log the current direction, but only once every LOG_INTERVAL_FRAMES
   		//    frames, so we do not flood tanks.log thousands of times per second
   		log_frame_counter = log_frame_counter + 1;
   		if (log_frame_counter >= LOG_INTERVAL_FRAMES){
   			log_frame_counter = 0;

   			if (player1.current_direction == MOVE_UP){
   				sprintf(log_message_text, "Direction: UP");
   			}else if (player1.current_direction == MOVE_DOWN){
   				sprintf(log_message_text, "Direction: DOWN");
   			}else if (player1.current_direction == MOVE_LEFT){
   				sprintf(log_message_text, "Direction: LEFT");
   			}else{
   				sprintf(log_message_text, "Direction: RIGHT");
   			}

   			tanks_log(log_message_text);

   			// Pick the cannon tip pair that matches the current facing
   			// direction, and log the color index sitting under it in the
   			// clean map buffer (not the VGA memory: that one already has
   			// the tank drawn on top of it, so it would just show the
   			// tank's own color instead of the map's)
			/*
   			if (player1.current_direction == MOVE_UP){
   				cannon_tip_pixel_value = bmp_get_map_pixel(player1.canonn_head_top_up_x, player1.canonn_head_top_up_y);
   				sprintf(log_message_text, "Cannon tip (%u,%u) = %u", player1.canonn_head_top_up_x, player1.canonn_head_top_up_y, cannon_tip_pixel_value);
   			}else if (player1.current_direction == MOVE_DOWN){
   				cannon_tip_pixel_value = bmp_get_map_pixel(player1.canonn_head_top_down_x, player1.canonn_head_top_down_y);
   				sprintf(log_message_text, "Cannon tip (%u,%u) = %u", player1.canonn_head_top_down_x, player1.canonn_head_top_down_y, cannon_tip_pixel_value);
   			}else if (player1.current_direction == MOVE_LEFT){
   				cannon_tip_pixel_value = bmp_get_map_pixel(player1.canonn_head_top_left_x, player1.canonn_head_top_left_y);
   				sprintf(log_message_text, "Cannon tip (%u,%u) = %u", player1.canonn_head_top_left_x, player1.canonn_head_top_left_y, cannon_tip_pixel_value);
   			}else{
   				cannon_tip_pixel_value = bmp_get_map_pixel(player1.canonn_head_top_right_x, player1.canonn_head_top_right_y);
   				sprintf(log_message_text, "Cannon tip (%u,%u) = %u", player1.canonn_head_top_right_x, player1.canonn_head_top_right_y, cannon_tip_pixel_value);
   			}

   			tanks_log(log_message_text);*/

   			// Also log the FUTURE cannon tip (one PIXEL_TO_MOVE step ahead
   			// in the current facing direction), read from the same
   			// collision map buffer_map_collisions_data used by the
   			// collision check above, so the log always shows exactly what
   			// the check is really seeing
   			player_update_future_collision_points(&player1, player1.current_direction);
   			cannon_tip_pixel_value = bmp_get_collision_pixel(player1.future_cannon_tip_x, player1.future_cannon_tip_y);
   			sprintf(log_message_text, "Future cannon tip (%u,%u) = %u", player1.future_cannon_tip_x, player1.future_cannon_tip_y, cannon_tip_pixel_value);
   			tanks_log(log_message_text);
   		}


   		// 7. Over the network this frame is finished. Both machines must
   		//    have reached this line with EXACTLY the same state, and the
   		//    checksum is what proves it: it is compared against the other
   		//    machine's a few frames later.
   		//
   		//    Then, and only then, the frame number moves on. Both machines
   		//    always sit on the same one.
   		if (network_mode == 1){
   			net_set_local_checksum(compute_state_checksum());
   			net_advance_frame();
   		}

    }while(!keys[KEY_ESC]);


	// Before anything else: while the card is running its DMA is reading
	// our buffer, so it has to be stopped before that memory is given back
	sound_shutdown();

	// The socket has to go back to the driver, or the next run cannot open
	// the same one and the game says there is no network
	if (network_mode == 1){
		net_shutdown();
	}

	player_free(&player1);
	player_free(&player2);
	bmp_delete_buffers();
	bmp_close_files();

	uninstall_kbd();  /* NEVER REMOVE  */

	// Back to text, so whatever happened can actually be read. Only in
	// network mode, where there is something to say.
	if (network_mode == 1){

		set_text_mode();

		if (connection_was_lost == 1){
			printf("\nThe other machine stopped answering.\n");
		}else{
			printf("\nGame over.\n");
		}

		if (net_desync_detected() == 1){
			printf("The two machines went out of step. See tanks.log.\n");
		}

		printf("Final score: player 1 %u - player 2 %u\n", player1.wins, player2.wins);

	}

	return 0;
}


//===========================================================
// Back to the 80x25 text screen. Used on the way out of a network game, so
// the message about what happened is not painted into a 320x200 buffer
// nobody is looking at any more.
//===========================================================
void set_text_mode(){

	union REGS registers;

	registers.x.ax = 0x0003;
	int86(0x10, &registers, &registers);

}


//===========================================================
// Turns this keyboard into the one byte the rest of the game works with.
//
// Everything downstream only ever sees these 5 bits, so it cannot tell
// whether they came from this keyboard or arrived from the other machine,
// and does not need to. That is what let the network be bolted on without
// touching the collisions, the bullets or the drawing.
//===========================================================
unsigned char read_input_from_keys(unsigned char key_up_code,
                                   unsigned char key_down_code,
                                   unsigned char key_left_code,
                                   unsigned char key_right_code,
                                   unsigned char key_fire_code){

	unsigned char input_bits;

	input_bits = 0;

	if (keys[key_up_code]){
		input_bits = input_bits | NET_INPUT_UP;
	}

	if (keys[key_down_code]){
		input_bits = input_bits | NET_INPUT_DOWN;
	}

	if (keys[key_left_code]){
		input_bits = input_bits | NET_INPUT_LEFT;
	}

	if (keys[key_right_code]){
		input_bits = input_bits | NET_INPUT_RIGHT;
	}

	if (keys[key_fire_code]){
		input_bits = input_bits | NET_INPUT_FIRE;
	}

	return input_bits;

}


//===========================================================
// One number standing for the whole state of the game, to catch a desync.
//
// In lockstep, when the two machines stop agreeing nothing looks wrong:
// each screen carries on making perfect sense, just a different one, and
// you can chase that for days. So each side works this out every frame and
// sends it now and then, and net.c compares it with its own.
//
// Every value the simulation can change goes in. The multipliers are there
// so that swapping two of them, say the two tanks' X, still comes out to a
// different total. It is allowed to overflow: that wraps the same way on
// both machines, which is all that matters.
//===========================================================
unsigned int compute_state_checksum(){

	unsigned int checksum;

	checksum = 0;

	checksum = checksum + (player1.position_x * 3);
	checksum = checksum + (player1.position_y * 5);
	checksum = checksum + (player1.current_direction * 7);
	checksum = checksum + (player1.bullet_position_x * 11);
	checksum = checksum + (player1.bullet_position_y * 13);
	checksum = checksum + (player1.bullet_is_flying * 17);
	checksum = checksum + (player1.bullet_direction * 19);
	checksum = checksum + (player1.wins * 23);
	checksum = checksum + (player1.is_exploding * 29);

	checksum = checksum + (player2.position_x * 31);
	checksum = checksum + (player2.position_y * 37);
	checksum = checksum + (player2.current_direction * 41);
	checksum = checksum + (player2.bullet_position_x * 43);
	checksum = checksum + (player2.bullet_position_y * 47);
	checksum = checksum + (player2.bullet_is_flying * 53);
	checksum = checksum + (player2.bullet_direction * 59);
	checksum = checksum + (player2.wins * 61);
	checksum = checksum + (player2.is_exploding * 67);

	checksum = checksum + (explosion_pause_counter * 71);

	return checksum;

}

void update_game(int direction){

	//Animation
	player1.speed_counter = player1.speed_counter + 1;
	player2.speed_counter = player2.speed_counter + 1;

	frame_counter++;
	// 3
	if (frame_counter >= FRAMES_COUNTER) {
        frame_counter = 0;

        // Both tanks are animated from the same frame_counter on purpose, so
        // they move their tracks at the same rhythm. What is NOT shared is
        // speed_counter / current_frame / is_moving: those live in each
        // player, so a tank that is standing still keeps its own frame while
        // the other one is rolling.
        update_player_animation(&player1);
        update_player_animation(&player2);
    }

}


//===========================================================
// Advances the track animation of one tank, but only if it has actually
// moved (is_moving), so a parked tank does not roll its tracks on the spot.
//===========================================================
void update_player_animation(struct player *_player){

	// increment speed
	if (_player->speed_counter >= _player->speed_total){
		_player->speed_counter = 0;

		//check if player is moving
		//If player is in moving, then change frames
		if(_player->is_moving == 1){

			_player->current_frame = _player->current_frame + 1;
			if(_player->current_frame >= _player->total_frames){
				_player->current_frame = 0;
			}

			// set to 0 player
			_player->is_moving = 0;

		}
	}

}


//===========================================================
// Would the tank run into a wall if it moved? Reads the 3 points
// player_update_future_collision_points() has just worked out: the cannon
// tip and both tracks. All three are needed, see players.h.
//
// Always read from buffer_map_collisions_data, never from VGA memory: that
// buffer never has the tanks drawn on top of it.
//===========================================================
int is_blocked_by_wall(struct player *_player){

	if (bmp_get_collision_pixel(_player->future_cannon_tip_x, _player->future_cannon_tip_y) == COLLISION_COLOR){
		return 1;
	}

	if (bmp_get_collision_pixel(_player->future_track1_x, _player->future_track1_y) == COLLISION_COLOR){
		return 1;
	}

	if (bmp_get_collision_pixel(_player->future_track2_x, _player->future_track2_y) == COLLISION_COLOR){
		return 1;
	}

	return 0;

}


//===========================================================
// Would the tank run into the OTHER tank if it moved? Compares the box it
// WOULD occupy against the box the other one occupies right now.
//
// The move is just refused, nothing is pushed back. That is what keeps the
// two tanks from ever ending up glued: they start apart, every move is
// checked before it is applied, and turning does not change the box, so
// they can never reach an overlap. And while they never overlap, a blocked
// tank always has a free direction left, at least the one it came from.
//===========================================================
int is_blocked_by_tank(struct player *_player, struct player *_other){

	// Box the tank WOULD occupy
	unsigned int player_left;
	unsigned int player_top;
	unsigned int player_right;
	unsigned int player_bottom;

	// Box the other tank occupies right now
	unsigned int other_left;
	unsigned int other_top;
	unsigned int other_right;
	unsigned int other_bottom;

	player_left   = _player->future_position_x + TANK_COLLISION_MARGIN;
	player_top    = _player->future_position_y + TANK_COLLISION_MARGIN;
	player_right  = player_left + TANK_COLLISION_WIDTH  - 1;
	player_bottom = player_top  + TANK_COLLISION_HEIGHT - 1;

	other_left   = _other->position_x + TANK_COLLISION_MARGIN;
	other_top    = _other->position_y + TANK_COLLISION_MARGIN;
	other_right  = other_left + TANK_COLLISION_WIDTH  - 1;
	other_bottom = other_top  + TANK_COLLISION_HEIGHT - 1;

	// Two boxes overlap unless one of them is completely to one side of the
	// other. So it is quicker to look for a reason why they CANNOT touch:
	// if any of these four is true, there is a gap between them and the
	// tank is free to move.
	if (player_right < other_left){
		return 0;
	}

	if (player_left > other_right){
		return 0;
	}

	if (player_bottom < other_top){
		return 0;
	}

	if (player_top > other_bottom){
		return 0;
	}

	// No gap on any side: the boxes overlap
	return 1;

}


//===========================================================
// Is there anything in the way, a wall or the other tank?
// player_update_future_collision_points() must have been called for the
// direction being tried first: both checks read the values it works out.
//===========================================================
int is_move_blocked(struct player *_player, struct player *_other){

	if (is_blocked_by_wall(_player) == 1){
		return 1;
	}

	if (is_blocked_by_tank(_player, _other) == 1){
		return 1;
	}

	return 0;

}


//===========================================================
// Is this player's bullet hitting the OTHER tank?
//
// Tested by the bullet's center pixel against the FULL 18x18 box, not the
// smaller box used for tank against tank: for pushing you want to be
// forgiving, for a hit generous, because a shot that looks like it hit has
// to count.
//
// NEVER tested against the tank that fired it: the bullet is born on its
// own cannon tip, and the RIGHT tip (15,8) is inside its own box, so the
// player would die the instant he fires to the right.
//
// No swept test needed: the bullet moves 3 pixels per frame and the box is
// 18 wide, so it cannot jump over a tank between two frames.
//===========================================================
int bullet_has_hit_tank(struct player *_player, struct player *_other){

	unsigned int bullet_x;
	unsigned int bullet_y;

	if (_player->bullet_is_flying == 0){
		return 0;
	}

	bullet_x = _player->bullet_position_x + BULLET_CENTER_X;
	bullet_y = _player->bullet_position_y + BULLET_CENTER_Y;

	if (bullet_x < _other->position_x){
		return 0;
	}

	if (bullet_x > _other->position_x + TANK_WIDTH - 1){
		return 0;
	}

	if (bullet_y < _other->position_y){
		return 0;
	}

	if (bullet_y > _other->position_y + TANK_HEIGHT - 1){
		return 0;
	}

	return 1;

}


//===========================================================
// New round: both tanks back to their starting spots, facing each other,
// bullets loaded. The scores are NOT touched: they carry over.
//===========================================================
void restart_game(){

	player_reset(&player1, PLAYER1_START_X, PLAYER1_START_Y, PLAYER1_START_DIRECTION);
	player_reset(&player2, PLAYER2_START_X, PLAYER2_START_Y, PLAYER2_START_DIRECTION);

}

void move_sprite(struct player *_player, int direction){

	// Remember facing direction so draw_to_buffer() can pick the right sprite
	_player->current_direction = direction;

	if (direction == MOVE_UP){

		if (_player->position_y >= PIXEL_TO_MOVE){
			_player->position_y = _player->position_y - PIXEL_TO_MOVE ;
		}else{
			_player->position_y = 0;
		}

	}else if (direction == MOVE_DOWN){

		if (_player->position_y + PIXEL_TO_MOVE <=  ( HEIGHT ) - TANK_HEIGHT ){
			_player->position_y = _player->position_y + PIXEL_TO_MOVE;
		}else{
			_player->position_y = HEIGHT - TANK_HEIGHT ;
		}

	}else if (direction == MOVE_LEFT){

		if (_player->position_x >= PIXEL_TO_MOVE){
			_player->position_x = _player->position_x - PIXEL_TO_MOVE;
		}else{
			_player->position_x = 0;
		}

	}else if (direction == MOVE_RIGHT){

		if (_player->position_x + PIXEL_TO_MOVE <= WIDTH - TANK_WIDTH){
			_player->position_x = _player->position_x + PIXEL_TO_MOVE;
		}else{
			_player->position_x = WIDTH - TANK_WIDTH;
		}

	}

}


//===========================================================
// Turns one player's keys into movement and shots.
//
// It is handed the 5 bits already worked out, NOT the keyboard, so it does
// not care where they came from: this keyboard in a local game, or the
// other machine in a network game. That single change is what let the whole
// network be bolted on without touching the collisions or the drawing.
//
// IMPORTANT: each player needs its OWN call, and therefore its own
// if / else if chain. Inside one chain only one direction gets through per
// frame (that is what stops the diagonal), so sharing it between the two
// players would let only one of them move per frame.
//===========================================================
void process_player_input(struct player *_player,
                          struct player *_other,
                          unsigned char input_bits){

	// 1 if a direction key is held this frame, whether the tank actually
	// managed to move or not. It is what drives the engine noise.
	unsigned int is_driving;

	is_driving = 0;

	// One direction per frame, so the tank can never go diagonal: with
	// several keys held, only the first of UP, DOWN, LEFT, RIGHT counts.
	//
	// Look before you leap: player_update_future_collision_points() works
	// out where the tank WOULD land one step ahead, is_move_blocked() checks
	// that against the map and the other tank, and only then does the tank
	// move. Checking after moving would mean having to get it back out.
	if (input_bits & NET_INPUT_UP){

		is_driving = 1;

		player_update_future_collision_points(_player, MOVE_UP);

		if (is_move_blocked(_player, _other) == 0){
			_player->is_moving = 1;
			move_sprite(_player, MOVE_UP);
		}

	}else if (input_bits & NET_INPUT_DOWN){

		is_driving = 1;

		player_update_future_collision_points(_player, MOVE_DOWN);

		if (is_move_blocked(_player, _other) == 0){
			_player->is_moving = 1;
			move_sprite(_player, MOVE_DOWN);
		}

	}else if (input_bits & NET_INPUT_LEFT){

		is_driving = 1;

		player_update_future_collision_points(_player, MOVE_LEFT);

		if (is_move_blocked(_player, _other) == 0){
			_player->is_moving = 1;
			move_sprite(_player, MOVE_LEFT);
		}

	}else if (input_bits & NET_INPUT_RIGHT){

		is_driving = 1;

		player_update_future_collision_points(_player, MOVE_RIGHT);

		if (is_move_blocked(_player, _other) == 0){
			_player->is_moving = 1;
			move_sprite(_player, MOVE_RIGHT);
		}

	}

	// OUTSIDE the chain above on purpose: shooting is not a direction, and
	// the tank has to be able to move and fire on the same frame.
	//
	// Only the frame the key GOES down counts. The bit stays set while the
	// key is held, so firing on the plain value would shoot again by itself
	// the moment the bullet died: an automatic weapon.
	if (input_bits & NET_INPUT_FIRE){

		// The sound only goes off if the shot really did. Pressing the key
		// while your own bullet is still flying does nothing, and it has to
		// be silent too: a bang with no bullet coming out is worse than no
		// bang at all.
		if (_player->fire_was_pressed == 0){

			if (player_fire_bullet(_player) == 1){
				sound_play(_player->sound_fire_voice, SOUND_SAMPLE_FIRE, SOUND_VOLUME_FIRE);
			}

		}

		_player->fire_was_pressed = 1;

	}else{

		_player->fire_was_pressed = 0;

	}

	// Keep the 4 cannon tips up to date with the new position, ready for the
	// bullet, the log and next frame's check
	player_update_cannon_tip(_player);

	// Engine noise while a direction key is held. is_driving is used and not
	// is_moving, because is_moving is turned off again by the track
	// animation, so the engine would cut in and out several times a second.
	// Holding a key against a wall still revs, which is what a tank pushing
	// against something should sound like.
	//
	// sound_loop() knows it is already playing this sample and does nothing,
	// so calling it every frame is free.
	if (is_driving == 1){
		sound_loop(_player->sound_engine_voice, _player->sound_engine_sample, SOUND_VOLUME_ENGINE);
	}else{
		sound_stop(_player->sound_engine_voice);
	}

}


//===========================================================
// Moves the bullet of ONE player. Returns 1 if it has hit the other tank.
//
// A bullet has two lives:
//   loaded -> it follows the cannon tip, so it is always at the mouth of
//             the cannon when the tank moves or turns
//   flying -> it travels on its own and dies against the edge of the screen
//             (inside player_move_bullet()), a tank, or a wall. Once dead
//             it goes back to loaded and returns to the cannon.
//
// The wall check is here and not in players.c on purpose: players.c knows
// nothing about the map, reading it is this file's job.
//===========================================================
int update_bullet(struct player *_player, struct player *_other){

	if (_player->bullet_is_flying == 0){

		player_update_bullet_position(_player);

		return 0;

	}

	player_move_bullet(_player);

	// player_move_bullet() may have just killed it for leaving the screen.
	// Only read the map while it is alive, so the coordinate is always a
	// real point inside the 320x200.
	if (_player->bullet_is_flying == 0){
		return 0;
	}

	// The tank is checked before the wall: no practical difference, since a
	// tank can never be standing on a wall, but a hit is the point of the
	// game.
	if (bullet_has_hit_tank(_player, _other) == 1){

		_player->bullet_is_flying = 0;
		_player->wins = _player->wins + 1;

		return 1;

	}

	if (bmp_get_collision_pixel(_player->bullet_position_x + BULLET_CENTER_X,
	                            _player->bullet_position_y + BULLET_CENTER_Y) == COLLISION_COLOR){
		_player->bullet_is_flying = 0;
	}

	return 0;

}

void draw_to_buffer(){

	// One per player, and they MUST be two variables: with a single one the
	// block that picks player 2's sprite would overwrite player 1's choice
	// and both tanks would be drawn with the same sprite.
	char *sprite_to_draw_player1;
	char *sprite_to_draw_player2;

	// Copy again original map to current buffer to show in screen
	memcpy(buffer_background_image_data,buffer_original_background_bmp,SCREEN_SIZE);


	/* DRAW FRAME of each animation list, according to the direction the player is facing */

	// Directions PLAYER 1
	if ( player1.current_direction == MOVE_UP ){

		if ( player1.current_frame == 0 ){
			sprite_to_draw_player1 = player1.sprite_tank_up;
		}else{
			sprite_to_draw_player1 = player1.sprite_tank_up_2;
		}

	}else if ( player1.current_direction == MOVE_DOWN ){

		if ( player1.current_frame == 0 ){
			sprite_to_draw_player1 = player1.sprite_tank_down;
		}else{
			sprite_to_draw_player1 = player1.sprite_tank_down_2;
		}

	}else if ( player1.current_direction == MOVE_LEFT ){

		if ( player1.current_frame == 0 ){
			sprite_to_draw_player1 = player1.sprite_tank_left;
		}else{
			sprite_to_draw_player1 = player1.sprite_tank_left_2;
		}

	}else { // if ( player1.current_direction == MOVE_RIGHT ){

		if ( player1.current_frame == 0 ){
			sprite_to_draw_player1 = player1.sprite_tank_right;
		}else{
			sprite_to_draw_player1 = player1.sprite_tank_right_2;
		}

	}

	// Directions PLAYER 2
	if ( player2.current_direction == MOVE_UP ){

		if ( player2.current_frame == 0 ){
			sprite_to_draw_player2 = player2.sprite_tank_up;
		}else{
			sprite_to_draw_player2 = player2.sprite_tank_up_2;
		}

	}else if ( player2.current_direction == MOVE_DOWN ){

		if ( player2.current_frame == 0 ){
			sprite_to_draw_player2 = player2.sprite_tank_down;
		}else{
			sprite_to_draw_player2 = player2.sprite_tank_down_2;
		}

	}else if ( player2.current_direction == MOVE_LEFT ){

		if ( player2.current_frame == 0 ){
			sprite_to_draw_player2 = player2.sprite_tank_left;
		}else{
			sprite_to_draw_player2 = player2.sprite_tank_left_2;
		}

	}else { // if ( player2.current_direction == MOVE_RIGHT ){

		if ( player2.current_frame == 0 ){
			sprite_to_draw_player2 = player2.sprite_tank_right;
		}else{
			sprite_to_draw_player2 = player2.sprite_tank_right_2;
		}

	}


	// Each tank in its new position, unless it has been blown up, in which
	// case its explosion goes there instead.
	//
	// The choice is made HERE, per tank, not where draw_to_buffer() is
	// called: during an explosion the map and the surviving tank still have
	// to be drawn as always. Same shape as the bullets below.
	if (player1.is_exploding == 1){

		draw_explosion(&player1);

	}else{

		// Draw Player 1
		draw_sprite_to_buffer(sprite_to_draw_player1,
					  TANK_WIDTH,
					  TANK_HEIGHT,
					  player1.position_x,
					  player1.position_y,
					  buffer_background_image_data);

	}

	if (player2.is_exploding == 1){

		draw_explosion(&player2);

	}else{

		// Draw Player 2
		draw_sprite_to_buffer(sprite_to_draw_player2,
					  TANK_WIDTH,
					  TANK_HEIGHT,
					  player2.position_x,
					  player2.position_y,
					  buffer_background_image_data);

	}


	// Bullets, only while they fly. A loaded bullet is still sitting on the
	// cannon tip, but it is not painted, so the tank does not carry a
	// visible bullet around.
	if (player1.bullet_is_flying == 1){

		draw_sprite_to_buffer(player1.sprite_tank_bullet,
					  TANK_BULLET_WIDTH,
					  TANK_BULLET_HEIGHT,
					  player1.bullet_position_x,
					  player1.bullet_position_y,
					  buffer_background_image_data);
	}

	if (player2.bullet_is_flying == 1){

		draw_sprite_to_buffer(player2.sprite_tank_bullet,
					  TANK_BULLET_WIDTH,
					  TANK_BULLET_HEIGHT,
					  player2.bullet_position_x,
					  player2.bullet_position_y,
					  buffer_background_image_data);
	}

}


//===========================================================
// Draws the explosion of a tank that has been hit, in place of its sprite.
// It is 13x13 against the tank's 18x18, so it is pushed in 2 pixels on each
// side to sit centered in the box the tank was filling.
//
// Which of the 2 sprites shows is decided by player_update_explosion().
//===========================================================
void draw_explosion(struct player *_player){

	char *sprite_to_draw;

	if (_player->explosion_current_frame == 0){
		sprite_to_draw = _player->sprite_tank_explosion;
	}else{
		sprite_to_draw = _player->sprite_tank_explosion2;
	}

	draw_sprite_to_buffer(sprite_to_draw,
				  EXPLOSION_WIDTH,
				  EXPLOSION_HEIGHT,
				  _player->position_x + EXPLOSION_OFFSET_X,
				  _player->position_y + EXPLOSION_OFFSET_Y,
				  buffer_background_image_data);

}


void init_graphics(){

	//============================================
	// First Stage:
	//
	// 	Create buffers and initialize them.
	// 	Create a original copy of the file (bmp_fill_background_in_main_buffer)
	// 	Extract pallete colors information
	// 	Write this pallete color information into DAC
	//============================================


	//  Create all buffers
	bmp_init_buffers();
	// Save a original copy of map file
	// This create a backup of the original file in a buffer
	bmp_fill_background_in_main_buffer("..\\res\\cutre.bmp");
	//bmp_fill_background_in_main_buffer("..\\res\\cutrecol.bmp");  // <--- FOR TESTING

	//load map collision
	bmp_fill_background_collision_in_buffer("..\\res\\cutrecol.bmp");


	// Extract the pallete colors to save it un DAC
	bmp_extract_pallete_from_file("..\\res\\cutre.bmp");
	// Set the pallete data into the VGA DAC
	bmp_write_pallete_data_into_dac(buffer_palleta_data);


	//============================================
	// Second stage :
	//
	// 	Load the sprites sheet and revert it
	//
	//		Fill "buffer_background_image_data" with image data from
	//		file "file_background_image_game" ( cutre.bmp)
	//
	//		Fill "buffer_sprites_data" with the sprites from "file_sprites_game" ( sprites.bmp)
	//		Extract a sprite from "buffer_sprites_data" and save it sprites player list
	//		Add this sprite under buffer_background_image_data
	//		Show the final result in screen
	//============================================

	// ============================
	// Extract sprites from sprites.bmp
	// ============================
	bmp_fill_sprites_in_buffer("..\\res\\sprites.bmp");
    bmp_revert_bmp(buffer_sprites_data);

    // ============================
	// Extract the background_image data from background file
	// ============================
	bmp_fill_buffer_with_image_data_from_file(buffer_background_image_data, file_background_image_game);

	// ============================
    // Fill player 1 with animation TANK_UP and
    // ============================
	bmp_extract_sprite(buffer_sprites_data,  2  ,5 , TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_up);
	bmp_extract_sprite(buffer_sprites_data, 23, 5 , TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_up_2);


	// ============================
    // Fill player 1 with animation TANK_DOWN and
    // ============================
	bmp_extract_sprite(buffer_sprites_data, 43  , 10 , TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_down);
	bmp_extract_sprite(buffer_sprites_data, 63  , 10  , TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_down_2);

	// ============================
    // Fill player 1 with animation TANK_LEFT and
    // ============================
	bmp_extract_sprite(buffer_sprites_data, 83  , 8 , TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_left);
	bmp_extract_sprite(buffer_sprites_data, 102 , 8 , TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_left_2);

	// ============================
    // Fill player 1 with animation TANK_RIGHT and
    // ============================
	bmp_extract_sprite(buffer_sprites_data, 124 , 8 , TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_right);
	bmp_extract_sprite(buffer_sprites_data, 145 , 8 , TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_right_2);

	// ============================
    // Fill bullet animation
    // ============================


    // Bullet tank 1
	bmp_extract_sprite(buffer_sprites_data, 252 , 14, TANK_BULLET_WIDTH, TANK_BULLET_HEIGHT, player1.sprite_tank_bullet);
	bmp_extract_sprite(buffer_sprites_data, 259 , 14, TANK_BULLET_WIDTH, TANK_BULLET_HEIGHT, player1.sprite_tank_bullet2);
	
	// Explosion. 13x13, the only sprites in the sheet that are not tank
	// sized: extracting them with TANK_WIDTH would drag in the gap between
	// cells plus the first columns of the next one, gluing a piece of one
	// frame to the right of the other.
	bmp_extract_sprite(buffer_sprites_data, 171 , 11 , EXPLOSION_WIDTH, EXPLOSION_HEIGHT, player1.sprite_tank_explosion);
	bmp_extract_sprite(buffer_sprites_data, 190 , 11 , EXPLOSION_WIDTH, EXPLOSION_HEIGHT, player1.sprite_tank_explosion2);

	
	// ==============================================================
	//           SPRITES PLAYER 2
	//
	// The second tank is a second row in sprites.bmp, exactly 21 pixels
	// below the first and in a different color, so every origin here is the
	// same X as player 1 with Y + 21.
	//
	// Both tanks have the same silhouette (checked cell by cell against the
	// sheet), so player 2 reuses every CANNON_TIP_OFFSET_* and
	// TRACK*_OFFSET_* without recalculating anything.
	//===============================================================
	
	// ============================
    // Fill player 2 with animation TANK_UP and
    // ============================
	bmp_extract_sprite(buffer_sprites_data,  2  , 26 , TANK_WIDTH, TANK_HEIGHT, player2.sprite_tank_up);
	bmp_extract_sprite(buffer_sprites_data, 23  , 26 , TANK_WIDTH, TANK_HEIGHT, player2.sprite_tank_up_2);


	// ============================
    // Fill player 2 with animation TANK_DOWN and
    // ============================
	bmp_extract_sprite(buffer_sprites_data, 43  , 31 , TANK_WIDTH, TANK_HEIGHT, player2.sprite_tank_down);
	bmp_extract_sprite(buffer_sprites_data, 63  , 31 , TANK_WIDTH, TANK_HEIGHT, player2.sprite_tank_down_2);

	// ============================
    // Fill player 2 with animation TANK_LEFT and
    // ============================
	bmp_extract_sprite(buffer_sprites_data, 83  , 29 , TANK_WIDTH, TANK_HEIGHT, player2.sprite_tank_left);
	bmp_extract_sprite(buffer_sprites_data, 102 , 29 , TANK_WIDTH, TANK_HEIGHT, player2.sprite_tank_left_2);

	// ============================
    // Fill player 2 with animation TANK_RIGHT and
    // ============================
	bmp_extract_sprite(buffer_sprites_data, 124 , 29 , TANK_WIDTH, TANK_HEIGHT, player2.sprite_tank_right);
	bmp_extract_sprite(buffer_sprites_data, 145 , 29 , TANK_WIDTH, TANK_HEIGHT, player2.sprite_tank_right_2);

	// ============================
    // Fill bullet animation
    // ============================


    // Bullet tank 2 - there is only one pair of bullets in the sprite
    // sheet, so both players shoot the same sprite
	bmp_extract_sprite(buffer_sprites_data, 252 , 14, TANK_BULLET_WIDTH, TANK_BULLET_HEIGHT, player2.sprite_tank_bullet);
	bmp_extract_sprite(buffer_sprites_data, 259 , 14, TANK_BULLET_WIDTH, TANK_BULLET_HEIGHT, player2.sprite_tank_bullet2);
	
	
	// Explosion - same as player 1, on the second row of the sheet
	bmp_extract_sprite(buffer_sprites_data, 171 , 32 , EXPLOSION_WIDTH, EXPLOSION_HEIGHT, player2.sprite_tank_explosion);
	bmp_extract_sprite(buffer_sprites_data, 192 , 32 , EXPLOSION_WIDTH, EXPLOSION_HEIGHT, player2.sprite_tank_explosion2);




}


void setup_screen(){
	//Init 320x200 VGA Mode
	set_vga_320_200_mode();
}

void init_players(){
	//printf("Players Initialization ... !!\n");

	// What is set here is set ONCE for the whole game: sprite buffers,
	// animation settings and score. Whatever belongs to a single round
	// (position, direction, bullet) is set by restart_game() at the bottom,
	// the very same call used after a hit, so a new game and a new round
	// always start from the same state.

	// ============================
	// INIT PLAYER 1
	// ============================

	player1.wins = 0;
	player1.frame_counter = 0;
	player1.total_frames = 2;
	player1.speed_total = 2;

	// Nothing has been fired yet, so no fire key is being held down
	player1.fire_was_pressed = 0;

	// Its own mixer voice for the engine and another for the shot, so the
	// two tanks never cut each other off
	player1.sound_engine_voice  = SOUND_VOICE_ENGINE_1;
	player1.sound_engine_sample = SOUND_SAMPLE_ENGINE_1;
	player1.sound_fire_voice    = SOUND_VOICE_FIRE_1;

	player1.canonn_head_top_up_x = 0;
	player1.canonn_head_top_up_y = 0;
	player1.canonn_head_top_down_x = 0;
	player1.canonn_head_top_down_y = 0;
	player1.canonn_head_top_left_x = 0;
	player1.canonn_head_top_left_y = 0;
	player1.canonn_head_top_right_x = 0;
	player1.canonn_head_top_right_y = 0;

	player1.bullet_position_x = 0;
	player1.bullet_position_y = 0;

	player_init(&player1);

	// ============================
	// INIT PLAYER 2
	// ============================

	player2.wins = 0;
	player2.frame_counter = 0;
	player2.total_frames = 2;
	player2.speed_total = 2;

	player2.fire_was_pressed = 0;

	player2.sound_engine_voice  = SOUND_VOICE_ENGINE_2;
	player2.sound_engine_sample = SOUND_SAMPLE_ENGINE_2;
	player2.sound_fire_voice    = SOUND_VOICE_FIRE_2;

	player2.canonn_head_top_up_x = 0;
	player2.canonn_head_top_up_y = 0;
	player2.canonn_head_top_down_x = 0;
	player2.canonn_head_top_down_y = 0;
	player2.canonn_head_top_left_x = 0;
	player2.canonn_head_top_left_y = 0;
	player2.canonn_head_top_right_x = 0;
	player2.canonn_head_top_right_y = 0;

	player2.bullet_position_x = 0;
	player2.bullet_position_y = 0;

	player_init(&player2);

	// Both tanks to their starting spots, facing each other. This also works
	// out their cannon tips and loads their bullets, so the first frame of
	// the loop already has valid coordinates.
	restart_game();

}

void wait_retrace(void)
{
    while (inp(0x3DA) & 0x08);   // wait current retrace
    while (!(inp(0x3DA) & 0x08)); // wait to start next retrace
}