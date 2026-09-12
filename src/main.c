#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <string.h>
#include <bios.h>
#include <alloc.h>
#include "header\util.h"
#include "header\bmp.h"
#include "header\players.h"
#include "header\sound.h"
#include "header\net.h"
#include "header\lockstep.h"

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
void update_camera(int snap_to_target);
void draw_explosion(struct player *_player);
void update_keyboard();
void init_sprite_numbers();
void free_sprite_numbers();
int compute_proximity_percent();
void draw_proximity_radar();
void fade_in_from_black();
void fade_out_to_black();
void show_frame_in_the_dark();

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

// 1 when the big 640x400 map is being used instead of the single screen one.
// Set from the command line: game.exe /net /bigmap.
//
// It only ever comes on together with /net. The big map needs a camera, a
// camera can only follow one tank, and on one keyboard that would leave the
// second player driving blind, so /bigmap on its own is refused further
// down and the game falls back to the normal map.
//
// BOTH machines have to be started with the same flag. If they are not, the
// maps differ, the walls differ, and the two simulations come apart. That is
// what map_width in the checksum is there to catch: it turns an
// incomprehensible game into a desync report in the log.
int big_map_mode;

// Which set of graphics the big map uses. Set from the command line with
// -sky, -war or -neon; 0 is the original big.bmp.
//
// It changes the DRAWING and nothing else. The walls always come from
// bigcol.bmp, whatever the theme, so two machines playing with different
// themes stay perfectly in sync: they see different pictures of exactly the
// same world.
//
// Which is why the theme must NEVER go into the state checksum. Same rule as
// camera_x: if the drawing code decides it, it is decoration.
int map_theme;

#define THEME_ORIGINAL 	0
#define THEME_SKY 		1
#define THEME_WAR 		2
#define THEME_NEON 		3

// Which of the five levels the big map uses, from -level1 to -level5.
// 0 is the original big.bmp that came with the game.
//
// Unlike the theme, this one is NOT decoration: every level is a different set
// of walls. So the two machines have to agree on it before a single frame is
// simulated, and net_agree_level() is what does that. Player 1 decides.
int map_level;

#define MAX_LEVEL 5

// The three files the theme and the level pick between them. init_graphics()
// fills them in before loading anything, so the rest of it does not have to
// know which theme or level is on.
//
// The level ones are built with sprintf into these buffers, so they need
// somewhere to live: a pointer to a local would be dangling by the time it
// was used.
char *theme_map_file;
char *theme_sprite_file;
char *theme_collision_file;

char level_map_path[64];
char level_collision_path[64];

// The 11 figures of the proximity radar, declared in players.h. They live
// here because main.c is what owns the screen: players.c only knows tank
// geometry and has no business with a HUD.
//
// NULL means there is no radar this run, and that is the normal case: see
// init_sprite_numbers() for the two conditions that have to hold.
char *number_0 = NULL;
char *number_1 = NULL;
char *number_2 = NULL;
char *number_3 = NULL;
char *number_4 = NULL;
char *number_5 = NULL;
char *number_6 = NULL;
char *number_7 = NULL;
char *number_8 = NULL;
char *number_9 = NULL;
char *number_percent = NULL;

// Built with sprintf by init_sprite_numbers(), so it needs somewhere to
// live for the same reason level_map_path does.
char numbers_path[64];

// Which tank THIS machine drives over the network. Meaningless in local
// mode, where this keyboard drives both of them.
int local_player_is_1;

// ---- The sounds this game uses ----
//
// How loud each one is, out of SOUND_VOLUME_MAX. The engines are turned down
// so a shot can be heard over them, and because two engines flat out plus a
// shot would clip badly once they are all added together. 64 is twice the
// recorded level, which the mixer allows and clamps if it goes too far.
#define SOUND_VOLUME_FIRE 		16 //64
#define SOUND_VOLUME_ENGINE 	12 //34
#define SOUND_VOLUME_DIED 		34 //64

// The numbers load_sound() hands back. sound.c knows nothing about any of
// this: it is a library, and which WAV files exist is the game's business.
//
// They start at -1, which every sound call takes to mean "nothing", so the
// game runs exactly the same when there is no card and nothing was loaded.
int sound_fire     = -1;
int sound_engine_1 = -1;
int sound_engine_2 = -1;
int sound_died     = -1;

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

	// Text of the log line being built. 96 and not 64: the memory lines carry
	// two 10 digit numbers each, and sprintf() would run off the end of a 64
	// byte buffer sitting on the stack, which on DOS is not a crash, it is
	// whatever happens next being wrong.
	char log_message_text[96];
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

	// Two numbers, not one, and the second is the one that matters here.
	//
	// malloc() and farmalloc() are different pools in Turbo C. The screen
	// buffers and the collision mask come out of malloc; the map picture, the
	// DMA buffer and every WAV file come out of farmalloc. Freeing something
	// on one side does NOT give the other side any more room, which is
	// exactly how the sound effects went missing once the big map arrived.
	sprintf(log_message_text, "Memory at start: near %lu  far %lu",
	        (unsigned long)coreleft(), (unsigned long)farcoreleft());
	tanks_log(log_message_text);

	// game.exe /net plays against another machine. game.exe on its own is
	// the two players on one keyboard game that was here before.
	network_mode        = 0;
	big_map_mode        = 0;
	map_theme           = THEME_ORIGINAL;
	map_level           = 0;
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

		if (stricmp(argv[argument_index], "/bigmap") == 0){
			big_map_mode = 1;
		}

		if (stricmp(argv[argument_index], "-bigmap") == 0){
			big_map_mode = 1;
		}

		// The look of the big map. They are alternatives, so the last one on
		// the line wins rather than trying to combine them.
		if (stricmp(argv[argument_index], "-sky") == 0){
			map_theme = THEME_SKY;
		}

		if (stricmp(argv[argument_index], "-war") == 0){
			map_theme = THEME_WAR;
		}

		if (stricmp(argv[argument_index], "-neon") == 0){
			map_theme = THEME_NEON;
		}

		// Which of the five levels. They live in res\\15Level\\<THEME>\\NIVELnn\\
		// and each one brings its own big.bmp AND its own bigcol.bmp.
		if (stricmp(argv[argument_index], "-level1") == 0){
			map_level = 1;
		}

		if (stricmp(argv[argument_index], "-level2") == 0){
			map_level = 2;
		}

		if (stricmp(argv[argument_index], "-level3") == 0){
			map_level = 3;
		}

		if (stricmp(argv[argument_index], "-level4") == 0){
			map_level = 4;
		}

		if (stricmp(argv[argument_index], "-level5") == 0){
			map_level = 5;
		}

		argument_index = argument_index + 1;

	}

	// The big map is a NETWORK feature and only a network feature.
	//
	// It needs a camera, and a camera can only follow one tank. Over the wire
	// that is exactly right: each machine follows its own, the two of them see
	// different parts of the map, and going looking for the other one is the
	// game. On one keyboard there is one screen and two tanks, so either half
	// the players are driving blind or nobody can leave the first room. There
	// is no third option, so /bigmap on its own is simply ignored.
	if (network_mode == 0){

		if (big_map_mode == 1){
			printf("\n/bigmap needs /net: it is the big map that has to be\n");
			printf("played over the network. Starting on the normal map.\n\n");
			tanks_log("Big map asked for without /net, ignored");
		}

		big_map_mode = 0;

	}

	// The themes only dress the big map. On the 320x200 map there is only one
	// set of graphics, so asking for a theme there is a mistake worth saying
	// out loud instead of ignoring in silence.
	if (big_map_mode == 0){

		if (map_theme != THEME_ORIGINAL){
			printf("\n-sky, -war and -neon only dress the big map, so they need\n");
			printf("/net /bigmap. Starting on the normal map.\n\n");
			tanks_log("Theme asked for without /bigmap, ignored");
		}

		map_theme = THEME_ORIGINAL;

		if (map_level != 0){
			printf("\n-level1 .. -level5 only exist on the big map, so they need\n");
			printf("/net /bigmap. Starting on the normal map.\n\n");
			tanks_log("Level asked for without /bigmap, ignored");
		}

		map_level = 0;

	}

	// The five levels only come in the three themed looks: there is no
	// res\\15Level\\ORIGINAL. Picking a level without a theme is not an error
	// worth refusing, so it gets the first one and is told which.
	if (map_level != 0 && map_theme == THEME_ORIGINAL){

		map_theme = THEME_SKY;

		printf("\nThe levels only come dressed, so -level%d is using -sky.\n", map_level);
		printf("Add -war or -neon if you want another look.\n\n");
		tanks_log("Level without a theme, defaulted to sky");

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

		//---------------------------------------------------
		// Settle the LEVEL before anything is loaded.
		//
		// The theme does not travel and does not need to: sky, war and neon
		// share their collision map byte for byte, so the two machines can
		// wear different ones and stay in sync. The level is the opposite:
		// every level is a different set of walls.
		//
		// Player 1 decides and player 2 adopts. On player 2, whatever was
		// asked for on the command line is thrown away here, which is why it
		// gets told on screen instead of quietly playing something else.
		//---------------------------------------------------
		if (big_map_mode == 1){

			int agreed_level;

			printf("Agreeing the level ...\n");

			agreed_level = net_agree_level(map_level);

			if (agreed_level < 0){
				printf("\nCould not agree a level with the other machine.\n");
				printf("Not starting: you would be playing different maps.\n\n");
				net_shutdown();
				return 1;
			}

			if (agreed_level != map_level){
				printf("Player 1 chose level %d, so that is what we play.\n", agreed_level);
			}

			map_level = agreed_level;

			sprintf(log_message_text, "NET: playing level %d", map_level);
			tanks_log(log_message_text);

		}

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

	// The radar, and it goes HERE for two reasons that are both about
	// init_graphics(): the theme is not decided until it runs, and the sheet
	// reader has a single FILE * that it does not let go of until it has cut
	// out the last tank. It does nothing at all on the small map.
	init_sprite_numbers();

	// The camera has to be aimed AFTER init_graphics(), not before.
	//
	// init_players() ends by calling restart_game(), which aims it too, but at
	// that point bmp_init_buffers() has not run yet: map_width is still the
	// 320 it starts life with, the clamp decides the camera cannot move, and
	// it gets pinned at 0,0. On the big map that would put the first frame in
	// the top left corner with the tank nowhere near it.
	update_camera(1);

	// What we ended up with. Worth having in the log: the big map is the only
	// thing here that can fail to fit, and if farmalloc() ever comes back NULL
	// this line is what says so before anything strange happens.
	sprintf(log_message_text, "Map %dx%d theme %d level %d  mem: near %lu far %lu",
	        map_width, map_height, map_theme, map_level,
	        (unsigned long)coreleft(), (unsigned long)farcoreleft());
	tanks_log(log_message_text);

	// Sound is optional: if there is no card sound_start() returns 0, says so
	// in the log, and every later sound call does nothing. The game plays
	// exactly the same, in silence.
	//
	// sound.c is a library and knows nothing about tanks, so everything that
	// IS about this game happens right here: where to report problems, which
	// WAV files to load, and how loud each one has to be.
	sound_set_log(tanks_log);

	if (sound_start() == 1){

		sound_fire     = load_sound("..\\res\\fire.wav");
		sound_engine_1 = load_sound("..\\res\\engip1.wav");
		sound_engine_2 = load_sound("..\\res\\engip2.wav");
		sound_died     = load_sound("..\\res\\died.wav");

		if (sound_fire == -1){
			tanks_log("Sound: could not load fire.wav");
		}

		if (sound_engine_1 == -1){
			tanks_log("Sound: could not load engip1.wav");
		}

		if (sound_engine_2 == -1){
			tanks_log("Sound: could not load engip2.wav");
		}

		if (sound_died == -1){
			tanks_log("Sound: could not load died.wav");
		}

		set_sound_volume(sound_fire,     SOUND_VOLUME_FIRE);
		set_sound_volume(sound_engine_1, SOUND_VOLUME_ENGINE);
		set_sound_volume(sound_engine_2, SOUND_VOLUME_ENGINE);
		set_sound_volume(sound_died,     SOUND_VOLUME_DIED);

		// init_players() ran before any of these files existed in memory, so
		// this is where each tank finds out which engine sound is its own.
		player1.sound_engine_sample = sound_engine_1;
		player2.sound_engine_sample = sound_engine_2;

		// Background music. Unlike the effects above it is NOT loaded: it is
		// read from the file while it plays, so a one minute song costs the
		// same 16 KB as a five second one and loops for ever.
		//
		// Drop an 8 bit mono 44100 Hz WAV in res\\ under this name. Unlike the
		// effects, the rate has to be EXACTLY that: load_sound() can convert a
		// file because it does it once at startup, and there is nowhere to do
		// that while streaming. If it is not there, or it is at another rate,
		// play_song() says so in the log and the game carries on perfectly well
		// without music.
		play_song("..\\res\\prody8.wav");

	}

	//---------------------------------------------------
	// The screen has not been painted once yet: everything so far has gone
	// into buffers. So this is the moment to put the DAC in the dark, drop
	// the first frame onto a screen where every color is black, and bring it
	// up.
	//
	// It goes AFTER the sound block on purpose. sound_update() is called
	// inside the fade and it has to be able to answer, which means
	// sound_start() must already have run (or already have failed, which is
	// just as good: every sound call then does nothing).
	//
	// And the music starting half a second before the picture is not a
	// problem, it is rather nice. It does not drift between the two machines
	// either: the fade is paced by the vertical retrace, so 32 steps take the
	// same time on a 386 and on a Pentium.
	//---------------------------------------------------
	bmp_write_black_pallete_into_dac();
	show_frame_in_the_dark();
	fade_in_from_black();

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
				stop_looping_sound(player1.sound_engine_sample);
				stop_looping_sound(player2.sound_engine_sample);

				// The bang. Played once from here and never cut, so it keeps
				// ringing into the start of the new round: the sound is
				// longer than the pause itself.
				play_sound(sound_died);

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

				//-------------------------------------------
				// The round changes behind a fade: out with the burnt tank
				// still on screen, the teleport in the dark, and in again on
				// the new frame. It hides the jump of two tanks and a camera
				// all moving at once.
				//
				// Over the network this is safe, and for a reason worth
				// knowing. Both machines get here on the SAME frame number,
				// because explosion_pause_counter is part of the simulation
				// and not of the drawing. So the two of them stop for the
				// same 64 retraces at the same moment, and the lockstep never
				// even notices there was a pause.
				//
				// Nor does the connection: net_poll() keeps running inside
				// both fades, and NET_TIMEOUT_SECONDS is 10 against the 0.9
				// seconds this takes.
				//
				// The fade is NOT inside restart_game() on purpose.
				// init_players() calls that too, long before there is a
				// palette to fade or a screen to fade it on.
				//-------------------------------------------
				fade_out_to_black();

				restart_game();

				show_frame_in_the_dark();
				fade_in_from_black();

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
   		//    frames, so we do not flood game.log thousands of times per second
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
   			// in the current facing direction), asked of bmp_is_wall(), the
   			// very same function the collision check above uses, so the log
   			// always shows exactly what the check is really seeing.
   			//
   			// The coordinates are WORLD ones, and the camera is logged next to
   			// them: when something looks wrong on screen, the first question is
   			// always whether the tank moved or the window did.
   			player_update_future_collision_points(&player1, player1.current_direction);
   			cannon_tip_pixel_value = (unsigned int)bmp_is_wall((int)player1.future_cannon_tip_x, (int)player1.future_cannon_tip_y);
   			sprintf(log_message_text, "Future cannon tip (%u,%u) wall=%u camera (%d,%d)", player1.future_cannon_tip_x, player1.future_cannon_tip_y, cannon_tip_pixel_value, camera_x, camera_y);
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
	sound_end();

	// The socket has to go back to the driver, or the next run cannot open
	// the same one and the game says there is no network
	if (network_mode == 1){
		net_shutdown();
	}

	player_free(&player1);
	player_free(&player2);
	free_sprite_numbers();
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
			printf("The two machines went out of step. See game.log.\n");
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

	// The size of the world, which never changes during a game, is in here on
	// purpose. It is not state: it is a tripwire. If one machine was started
	// with /bigmap and the other without it, the walls are in different places
	// and the two simulations drift apart in a way that is very hard to read
	// from the outside. This turns that into a clean desync report on the
	// first check.
	//
	// camera_x and camera_y must NEVER be added here. Each machine follows its
	// own tank, so they are legitimately different, and putting them in would
	// report a desync on frame one of every network game.
	checksum = checksum + ((unsigned int)map_width * 73);
	checksum = checksum + ((unsigned int)map_height * 79);

	// And the level, for the same reason, because map_width alone would NOT
	// catch it: every level is 640x400, so two machines on different levels
	// have the same map size and completely different walls. Without this the
	// symptom would be tanks walking through each other's walls with no
	// explanation.
	//
	// net_agree_level() should make it impossible. This is the belt to that
	// pair of braces, and it costs one addition every 30 frames.
	//
	// map_theme is deliberately NOT here: the three themes share their
	// collision map byte for byte, so different themes are not a desync and
	// checking them would forbid something that works.
	checksum = checksum + ((unsigned int)map_level * 83);

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
// Always read from the collision mask, never from VGA memory: that mask
// never has the tanks drawn on top of it, and it covers the WHOLE world, not
// just the part being shown. Both matter. The second one is what lets this
// machine work out whether the OTHER tank, off in a room nobody here can
// see, has run into something. If that answer differed between the two
// machines the game would come apart.
//
// These are WORLD coordinates. The camera has nothing to do with any of it.
//===========================================================
int is_blocked_by_wall(struct player *_player){

	if (bmp_is_wall((int)_player->future_cannon_tip_x, (int)_player->future_cannon_tip_y) == 1){
		return 1;
	}

	if (bmp_is_wall((int)_player->future_track1_x, (int)_player->future_track1_y) == 1){
		return 1;
	}

	if (bmp_is_wall((int)_player->future_track2_x, (int)_player->future_track2_y) == 1){
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

	if (big_map_mode == 1){

		player_reset(&player1, BIG_PLAYER1_START_X, BIG_PLAYER1_START_Y, BIG_PLAYER1_START_DIRECTION);
		player_reset(&player2, BIG_PLAYER2_START_X, BIG_PLAYER2_START_Y, BIG_PLAYER2_START_DIRECTION);

	}else{

		player_reset(&player1, PLAYER1_START_X, PLAYER1_START_Y, PLAYER1_START_DIRECTION);
		player_reset(&player2, PLAYER2_START_X, PLAYER2_START_Y, PLAYER2_START_DIRECTION);

	}

	// Put the camera straight on the tank instead of letting it slide over
	// from wherever the last round ended. There is nothing to follow smoothly
	// from when everything has just been teleported.
	update_camera(1);

}

//===========================================================
// Moves the tank one step. WORLD coordinates.
//
// There is no screen limit in here any more, and that is deliberate. What
// stops the tank is the WALL, checked by is_blocked_by_wall() before this is
// ever called, and both maps are drawn with a solid border around the whole
// world. The limit stopped being code and became part of the picture.
//
// That border has to be at least 8 pixels thick, and the reason is this
// function: the tank only ever stands on positions PIXEL_TO_MOVE apart, so a
// wall one pixel thick can sit at a coordinate the tank never lands on and
// get walked straight through. The maps have 16 to 33 pixels, which is
// plenty.
//
// The two >= PIXEL_TO_MOVE tests below are NOT limits, and they stay. The
// positions are unsigned, so 1 - 2 is not -1, it is 65535, and everything
// downstream would then read a long way outside the map. With a proper
// border they never fire; they are there for the day a map is drawn wrong.
//===========================================================
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

		_player->position_y = _player->position_y + PIXEL_TO_MOVE;

	}else if (direction == MOVE_LEFT){

		if (_player->position_x >= PIXEL_TO_MOVE){
			_player->position_x = _player->position_x - PIXEL_TO_MOVE;
		}else{
			_player->position_x = 0;
		}

	}else if (direction == MOVE_RIGHT){

		_player->position_x = _player->position_x + PIXEL_TO_MOVE;

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
				play_sound(sound_fire);
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
	// loop_sound() knows it is already playing this sound and does nothing,
	// so calling it every frame is free. Neither call needs a voice number:
	// the mixer keeps track of which voice this sound went to.
	if (is_driving == 1){
		loop_sound(_player->sound_engine_sample);
	}else{
		stop_looping_sound(_player->sound_engine_sample);
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

	if (bmp_is_wall((int)(_player->bullet_position_x + BULLET_CENTER_X),
	                (int)(_player->bullet_position_y + BULLET_CENTER_Y)) == 1){
		_player->bullet_is_flying = 0;
	}

	return 0;

}

//===========================================================
// Points the camera at the tank THIS machine is driving.
//
// Over the network each machine follows its own tank, so the two cameras
// hold different values and that is exactly right: the camera is not part of
// the game. It never goes into the checksum, no rule ever reads it, and the
// two simulations stay identical while showing completely different parts of
// the map.
//
// In a local game there is one screen and two tanks, so it follows player 1
// and player 2 has to make do. /bigmap on its own is for trying the camera
// out, not for playing two up.
//
// snap_to_target = 1 jumps there at once, for the start of a round.
//===========================================================
void update_camera(int snap_to_target){

	struct player *target;

	target = &player1;

	if (network_mode == 1){
		if (local_player_is_1 == 0){
			target = &player2;
		}
	}

	if (snap_to_target == 1){
		bmp_camera_snap((int)target->position_x, (int)target->position_y, TANK_WIDTH, TANK_HEIGHT);
	}else{
		bmp_camera_follow((int)target->position_x, (int)target->position_y, TANK_WIDTH, TANK_HEIGHT);
	}

}


void draw_to_buffer(){

	// One per player, and they MUST be two variables: with a single one the
	// block that picks player 2's sprite would overwrite player 1's choice
	// and both tanks would be drawn with the same sprite.
	char *sprite_to_draw_player1;
	char *sprite_to_draw_player2;

	// Where the window is going to be BEFORE anything is painted, so the map
	// and everything standing on it agree about the same frame.
	update_camera(0);

	// The visible 320x200 window of the map, copied in as the background. On
	// the normal one screen map the camera is pinned at 0,0 and this comes out
	// as exactly the same 64000 bytes it always copied.
	bmp_draw_world_window(buffer_background_image_data);


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
					  (int)player1.position_x - camera_x,
					  (int)player1.position_y - camera_y,
					  buffer_background_image_data);

	}

	if (player2.is_exploding == 1){

		draw_explosion(&player2);

	}else{

		// Draw Player 2
		draw_sprite_to_buffer(sprite_to_draw_player2,
					  TANK_WIDTH,
					  TANK_HEIGHT,
					  (int)player2.position_x - camera_x,
					  (int)player2.position_y - camera_y,
					  buffer_background_image_data);

	}


	// Bullets, only while they fly. A loaded bullet is still sitting on the
	// cannon tip, but it is not painted, so the tank does not carry a
	// visible bullet around.
	if (player1.bullet_is_flying == 1){

		draw_sprite_to_buffer(player1.sprite_tank_bullet,
					  TANK_BULLET_WIDTH,
					  TANK_BULLET_HEIGHT,
					  (int)player1.bullet_position_x - camera_x,
					  (int)player1.bullet_position_y - camera_y,
					  buffer_background_image_data);
	}

	if (player2.bullet_is_flying == 1){

		draw_sprite_to_buffer(player2.sprite_tank_bullet,
					  TANK_BULLET_WIDTH,
					  TANK_BULLET_HEIGHT,
					  (int)player2.bullet_position_x - camera_x,
					  (int)player2.bullet_position_y - camera_y,
					  buffer_background_image_data);
	}

	// The radar last, so nothing can be painted over it, and in SCREEN
	// coordinates: it is the only thing in this function that is not part of
	// the world, which is why it is also the only one that does not subtract
	// the camera. It does nothing at all when there is no radar.
	draw_proximity_radar();

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
				  (int)(_player->position_x + EXPLOSION_OFFSET_X) - camera_x,
				  (int)(_player->position_y + EXPLOSION_OFFSET_Y) - camera_y,
				  buffer_background_image_data);

}


//===========================================================
// THE FADES
//
// A fade in mode 13h does not touch one pixel. The picture is already sitting
// in video memory and it stays there the whole time: what changes is what
// each of the 256 color indexes MEANS, and that lives in the VGA DAC.
//
// So the trick for a load is to put the DAC in the dark FIRST, paint the
// frame into a screen where every color is black, and only then bring the
// palette up. Nobody sees the map arrive; they see it appear.
//
// Three things have to happen inside the loop, and none of them is optional:
//
//   wait_retrace()  paces it. One step per retrace at about 70 Hz, so
//                   FADE_TOTAL_STEPS decides the duration in SECONDS and the
//                   speed of the machine does not come into it. That is what
//                   keeps two machines on the network fading for the same
//                   length of time, which matters because the music is
//                   streamed and nobody ever resynchronises it.
//
//   sound_update()  or the card replays whichever half of the DMA buffer it
//                   has just finished and the music stutters for half a
//                   second. Same reason it is called inside the lockstep
//                   wait.
//
//   net_poll()      or the packets arriving during the fade are dropped. A
//                   buffer that nobody picks up is a lost packet.
//
// The DAC is written immediately after wait_retrace() returns, which is the
// START of the vertical blanking: that is the window where writing the
// palette cannot show up as sparkle on a real VGA card.
//===========================================================

//===========================================================
// From black up to the real palette.
//===========================================================
void fade_in_from_black(){

	int level;

	for (level = 0; level <= FADE_TOTAL_STEPS; level++){

		wait_retrace();
		bmp_write_pallete_data_into_dac_scaled(buffer_palleta_data, level);

		sound_update();

		if (network_mode == 1){
			net_poll();
		}

	}

}


//===========================================================
// And back down. Whatever is on screen stays on screen: it just goes dark.
//===========================================================
void fade_out_to_black(){

	int level;

	for (level = FADE_TOTAL_STEPS; level >= 0; level--){

		wait_retrace();
		bmp_write_pallete_data_into_dac_scaled(buffer_palleta_data, level);

		sound_update();

		if (network_mode == 1){
			net_poll();
		}

	}

}


//===========================================================
// Builds a frame and puts it on the VGA while the DAC is still black, so it
// arrives invisible and there is something for fade_in_from_black() to bring
// up. Without this the fade would bring up the PREVIOUS frame.
//===========================================================
void show_frame_in_the_dark(){

	draw_to_buffer();
	wait_retrace();
	bmp_paint_image_data_to_vga(buffer_background_image_data);

}


//===========================================================
// THE PROXIMITY RADAR
//
// On the big map the two tanks start in opposite corners and cannot see each
// other, so without something like this looking for the other one is walking
// around at random until you trip over him. The radar answers one question
// and nothing else: how close is he, from 0% to 100%.
//
// It is DECORATION, in exactly the sense camera_x is. It is worked out from
// two positions that both machines already simulate identically, so both of
// them show the same figure without one byte crossing the network, and like
// the camera it must never end up in compute_state_checksum().
//===========================================================

// Figures the radar paints: three digits and the % after them.
//
// Three and not "as many as it needs", so 9 comes out as 009 and the figure
// is always the same width. A centered number whose width changes jumps
// sideways every time it crosses 10 or 100, and a thing that jumps is a
// thing you look at instead of playing.
#define RADAR_DIGITS 			3
#define RADAR_CELLS 			(RADAR_DIGITS + 1)

// The box the whole figure occupies. The first cells only advance
// NUMBER_ADVANCE each; the last one still takes its full width.
#define RADAR_WIDTH 			(((RADAR_CELLS - 1) * NUMBER_ADVANCE) + NUMBER_WIDTH)

// Where it sits. SCREEN coordinates: the radar is not in the world, so
// nothing in here ever subtracts the camera.
#define RADAR_MARGIN_BOTTOM 	2
#define RADAR_X 				((WIDTH - RADAR_WIDTH) / 2)
#define RADAR_Y 				(HEIGHT - NUMBER_HEIGHT - RADAR_MARGIN_BOTTOM)

// The outline the figures are drawn on top of.
//
// It is needed because the radar has no background of its own: it sits on
// whatever part of the map the camera happens to be showing. Over the dark
// floor of the sky theme the figures were perfectly readable and over the
// pale stone border of the war theme they nearly disappeared, and that is
// not something the number can be trusted to survive by luck.
//
// Color 0 and not a dark grey picked per theme, because index 0 is pure
// black in the palette of all three of them, checked entry by entry. It is
// also the transparent color in a SPRITE, which costs nothing here: what is
// transparent is what is read from the sheet, and this is what gets written
// to the screen.
#define RADAR_OUTLINE_COLOR 	0
#define RADAR_OUTLINE_STEPS 	4

// The four places the outline is stamped: one pixel left, right, up and
// down. Not the diagonals, which would cost half again as much for a
// thickness the eye does not see at this size.
static int radar_outline_x[RADAR_OUTLINE_STEPS] = { -1,  1,  0,  0 };
static int radar_outline_y[RADAR_OUTLINE_STEPS] = {  0,  0, -1,  1 };


//===========================================================
// Reserves the 11 figures and cuts them out of the theme's numbers.bmp.
//
// TWO conditions have to hold, and neither of them is about taste:
//
//   big_map_mode - the radar only means anything when you cannot see the
//                  other tank. It also settles the network on its own: the
//                  argument check above refuses /bigmap without /net and
//                  falls back to the small map, so big_map_mode is only ever
//                  1 in a network game.
//
//   a theme      - numbers.bmp is drawn in the palette of its own theme, and
//                  the DAC is loaded from the map. In the undressed original
//                  look 254 of the 256 palette entries are different ones,
//                  so the figures would come out in whatever colors happened
//                  to land on those indexes. There is no
//                  res\Numbers\ORIGINAL to read either.
//
// It has to run AFTER init_graphics(), and not just for the theme: the sheet
// reader works on ONE FILE * (bmp.c, file_sprites_game_open) and it is not
// handed back until init_graphics() has cut out the last tank.
//
// Either all 11 exist or none do. A half filled set would be drawn with a
// NULL in the middle of it, so a failed malloc gives everything back and
// leaves the game running exactly as it did before the radar existed.
//===========================================================
void init_sprite_numbers(){

	// The 11 globals, in sheet order, so the loop below can reach them
	char **target[NUMBER_TOTAL_SPRITES];
	char *theme_folder;
	char radar_log_text[96];
	unsigned int cell;

	if (big_map_mode == 0){
		return;
	}

	if (map_theme == THEME_SKY){
		theme_folder = "SKYNET";
	}else if (map_theme == THEME_WAR){
		theme_folder = "MILITAR";
	}else if (map_theme == THEME_NEON){
		theme_folder = "NEON";
	}else{
		tanks_log("Radar: the original look has no numbers, no radar");
		return;
	}

	target[0]  = &number_0;
	target[1]  = &number_1;
	target[2]  = &number_2;
	target[3]  = &number_3;
	target[4]  = &number_4;
	target[5]  = &number_5;
	target[6]  = &number_6;
	target[7]  = &number_7;
	target[8]  = &number_8;
	target[9]  = &number_9;
	target[NUMBER_PERCENT_CELL] = &number_percent;

	// 11 cells of 18x18 is 3564 bytes, and they are asked for AFTER the
	// 256000 byte map already has its place. Small blocks reserved before a
	// big one are what leaves the big one without a contiguous hole to fit
	// in, and that is a lesson this game learned the hard way.
	for (cell = 0; cell < NUMBER_TOTAL_SPRITES; cell++){

		*target[cell] = (char *)malloc(NUMBER_WIDTH * NUMBER_HEIGHT);

		if (*target[cell] == NULL){
			tanks_log("Radar: no memory for the numbers, no radar");
			free_sprite_numbers();
			return;
		}

	}

	// 8.3 all the way down: Numbers is 7 characters, SKYNET, MILITAR and
	// NEON are 8 or fewer, numbers.bmp is 7.3. Go over that anywhere in this
	// path and DOS mangles the name and the open fails.
	sprintf(numbers_path, "..\\res\\Numbers\\%s\\numbers.bmp", theme_folder);

	bmp_open_sprite_sheet(numbers_path);

	// One row of 11 cells, left to right: the digits 0 to 9 and then the %.
	// numbers.bmp is 320x200 like every other sheet, which is what
	// bmp_extract_sprite() assumes when it flips the bottom-up rows of the
	// BMP round.
	for (cell = 0; cell < NUMBER_TOTAL_SPRITES; cell++){

		bmp_extract_sprite(cell * NUMBER_WIDTH,
		                   0,
		                   NUMBER_WIDTH,
		                   NUMBER_HEIGHT,
		                   *target[cell]);

	}

	bmp_close_sprite_sheet();

	sprintf(radar_log_text, "Radar: numbers loaded from %s", numbers_path);
	tanks_log(radar_log_text);

}


//===========================================================
// Gives the 11 figures back. Safe to call on a set that was never reserved,
// which is how init_sprite_numbers() cleans up after itself.
//===========================================================
void free_sprite_numbers(){

	char **target[NUMBER_TOTAL_SPRITES];
	unsigned int cell;

	target[0]  = &number_0;
	target[1]  = &number_1;
	target[2]  = &number_2;
	target[3]  = &number_3;
	target[4]  = &number_4;
	target[5]  = &number_5;
	target[6]  = &number_6;
	target[7]  = &number_7;
	target[8]  = &number_8;
	target[9]  = &number_9;
	target[NUMBER_PERCENT_CELL] = &number_percent;

	for (cell = 0; cell < NUMBER_TOTAL_SPRITES; cell++){

		if (*target[cell] != NULL){
			free(*target[cell]);
			*target[cell] = NULL;
		}

	}

}


//===========================================================
// How close the two tanks are: 0 = as far apart as this map allows,
// 100 = on top of each other.
//
// Integer arithmetic the whole way, on purpose. There is not one float in
// this project, and a radar is not a good reason to drag Turbo C's floating
// point library into a game that is already counting its bytes.
//
// So the distance is the cheap classic instead of a real one:
//
//     distance = bigger + (smaller / 2)
//
// It lands within about 11% of sqrt(dx*dx + dy*dy) and costs a compare, an
// add and a shift. Plain |dx| + |dy| was the other candidate and it is worse
// HERE: it makes a tank on the diagonal read much further away than one
// straight ahead at the same real distance, and the diagonal is exactly
// where the other player starts.
//
// The percentage has to be worked out in long. distance * 100 reaches about
// 78000 on the 640x400 map, an unsigned int stops at 65535, and in 16 bit
// arithmetic the radar would wrap round and read a cheerful 100% at the far
// end of the map.
//===========================================================
int compute_proximity_percent(){

	int dx;
	int dy;
	int swap;
	long distance;
	long worst_distance;
	long percent;

	// Cast to int BEFORE subtracting. The positions are unsigned, so a
	// player1 standing to the LEFT of player2 gives 65000-something here
	// instead of a negative number, and the radar would read 0% every time
	// the tanks happened to be the wrong way round.
	dx = (int)player1.position_x - (int)player2.position_x;
	dy = (int)player1.position_y - (int)player2.position_y;

	if (dx < 0){
		dx = -dx;
	}
	if (dy < 0){
		dy = -dy;
	}

	if (dx < dy){
		swap = dx;
		dx = dy;
		dy = swap;
	}

	distance = (long)dx + ((long)dy / 2L);

	// The same formula over the whole world, so the scale follows
	// map_width and map_height instead of having 640 written into it. A tank
	// is a box and not a point, so the furthest its corner can ever get from
	// the other corner is the map minus one tank.
	if (map_width > map_height){
		worst_distance = (long)(map_width  - TANK_WIDTH)
		               + ((long)(map_height - TANK_HEIGHT) / 2L);
	}else{
		worst_distance = (long)(map_height - TANK_HEIGHT)
		               + ((long)(map_width  - TANK_WIDTH) / 2L);
	}

	if (worst_distance <= 0){
		return 0;
	}

	percent = 100L - ((distance * 100L) / worst_distance);

	if (percent < 0){
		percent = 0;
	}
	if (percent > 100){
		percent = 100;
	}

	return (int)percent;

}


//===========================================================
// Paints the radar at the bottom of the screen.
//
// Nothing here subtracts the camera, and that is the whole point: this is the
// one thing drawn each frame that does not live in the world. It stays in the
// same 4 cells whatever the tank is doing.
//
// The one NULL check is enough for all 11: init_sprite_numbers() either
// fills the whole set or leaves the whole set empty.
//===========================================================
void draw_proximity_radar(){

	char *figure[RADAR_CELLS];
	char *digit[10];
	int percent;
	int value;
	int cell;
	int step;

	if (number_0 == NULL){
		return;
	}

	digit[0] = number_0;
	digit[1] = number_1;
	digit[2] = number_2;
	digit[3] = number_3;
	digit[4] = number_4;
	digit[5] = number_5;
	digit[6] = number_6;
	digit[7] = number_7;
	digit[8] = number_8;
	digit[9] = number_9;

	percent = compute_proximity_percent();

	// Right to left, so the units land in the last digit cell and a 9 comes
	// out as 009 instead of shifting the whole figure one cell over.
	value = percent;

	for (cell = RADAR_DIGITS - 1; cell >= 0; cell--){
		figure[cell] = digit[value % 10];
		value = value / 10;
	}

	figure[RADAR_DIGITS] = number_percent;

	// Two passes, and they cannot be folded into one.
	//
	// The figures are placed NUMBER_ADVANCE apart while their ink is up to 14
	// pixels wide, so every cell overlaps the one before it a little. If each
	// figure were outlined and then filled before moving on to the next, the
	// black outline of a digit would land on top of the right hand edge of
	// the digit already painted and eat it. So: every outline first, and
	// every figure afterwards.
	for (cell = 0; cell < RADAR_CELLS; cell++){

		for (step = 0; step < RADAR_OUTLINE_STEPS; step++){

			draw_sprite_silhouette_to_buffer(figure[cell],
			                      NUMBER_WIDTH,
			                      NUMBER_HEIGHT,
			                      RADAR_X + (cell * NUMBER_ADVANCE) + radar_outline_x[step],
			                      RADAR_Y + radar_outline_y[step],
			                      RADAR_OUTLINE_COLOR,
			                      buffer_background_image_data);

		}

	}

	for (cell = 0; cell < RADAR_CELLS; cell++){

		draw_sprite_to_buffer(figure[cell],
		                      NUMBER_WIDTH,
		                      NUMBER_HEIGHT,
		                      RADAR_X + (cell * NUMBER_ADVANCE),
		                      RADAR_Y,
		                      buffer_background_image_data);

	}

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


	// Which world are we playing in. The buffers are sized from this, so it
	// has to be the first thing that happens.
	//
	// The two maps go through EXACTLY the same code from here on. A 320x200
	// world is just one where the window covers everything and the camera can
	// never move, so the normal game is not a special case of anything: it
	// falls out of the general one.
	if (big_map_mode == 1){

		//---------------------------------------------------
		// The theme picks the two files that are DRAWN, and only those.
		//
		// The collision map is ALWAYS bigcol.bmp. The themes are the same
		// world repainted: same walls, same doorways, same everything that
		// decides. So a machine on -neon and one on -war play exactly the
		// same match and stay in sync, they just look different. It is the
		// same idea as the camera.
		//
		// Each map carries the palette its own sprites were drawn with, which
		// is why bmp_extract_pallete_from_file() reads the map: load the
		// palette of one theme and the sprites of another and the tanks come
		// out the wrong colour.
		//---------------------------------------------------
		char *theme_folder;

		// The sprite sheet depends ONLY on the theme: every level of a theme
		// uses the same tanks. And the folder name, for the levels.
		if (map_theme == THEME_SKY){

			theme_sprite_file = "..\\res\\spr_sky.bmp";
			theme_map_file    = "..\\res\\map_sky.bmp";
			theme_folder      = "SKYNET";

		}else if (map_theme == THEME_WAR){

			theme_sprite_file = "..\\res\\spr_war.bmp";
			theme_map_file    = "..\\res\\map_war.bmp";
			theme_folder      = "MILITAR";

		}else if (map_theme == THEME_NEON){

			theme_sprite_file = "..\\res\\spr_neon.bmp";
			theme_map_file    = "..\\res\\map_neon.bmp";
			theme_folder      = "NEON";

		}else{

			theme_sprite_file = "..\\res\\sprites.bmp";
			theme_map_file    = "..\\res\\big.bmp";
			theme_folder      = "SKYNET";

		}

		//---------------------------------------------------
		// And now the level decides which pair of files.
		//
		// Level 0 is the original map that shipped with the game, in res\\
		// with the shared bigcol.bmp. Levels 1 to 5 each bring their OWN
		// collision map, which is the whole reason they have to be agreed
		// over the network.
		//
		// Every folder name fits DOS 8.3: 15Level, SKYNET, MILITAR, NEON and
		// NIVEL01 are all 8 characters or fewer. If you add a theme, keep to
		// that or DOS mangles the name and the open fails.
		//---------------------------------------------------
		if (map_level == 0){

			theme_collision_file = "..\\res\\bigcol.bmp";

		}else{

			sprintf(level_map_path, "..\\res\\15Level\\%s\\NIVEL%02d\\big.bmp",
			        theme_folder, map_level);

			sprintf(level_collision_path, "..\\res\\15Level\\%s\\NIVEL%02d\\bigcol.bmp",
			        theme_folder, map_level);

			theme_map_file       = level_map_path;
			theme_collision_file = level_collision_path;

		}

		bmp_init_buffers(640, 400);

		bmp_fill_background_in_main_buffer(theme_map_file);
		bmp_fill_background_collision_in_buffer(theme_collision_file);
		bmp_extract_pallete_from_file(theme_map_file);

	}else{

		// The small map has one set of graphics, no theme and no levels.
		theme_map_file       = "..\\res\\cutre.bmp";
		theme_sprite_file    = "..\\res\\sprites.bmp";
		theme_collision_file = "..\\res\\cutrecol.bmp";

		bmp_init_buffers(WIDTH, HEIGHT);

		bmp_fill_background_in_main_buffer(theme_map_file);
		bmp_fill_background_collision_in_buffer(theme_collision_file);
		bmp_extract_pallete_from_file(theme_map_file);

	}
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
	//		Open sprites.bmp and cut each sprite straight out of the file
	//		into the player's own little buffer
	//		Add this sprite under buffer_background_image_data
	//		Show the final result in screen
	//============================================

	// ============================
	// Extract sprites from sprites.bmp
	// ============================
	// The theme's sheet, picked above. Every theme ships its own with the same
	// grid and the same silhouettes: only the colours change, so all the cut
	// coordinates below work unchanged for all four.
	bmp_open_sprite_sheet(theme_sprite_file);

    // ============================
	// First frame of background, so there is something sensible on screen
	// before the loop paints anything. It comes out of the map already in
	// memory, not out of the file: with a map bigger than one screen the file
	// does not hold a 320x200 picture anywhere.
	// ============================
	bmp_draw_world_window(buffer_background_image_data);

	// ============================
    // Fill player 1 with animation TANK_UP and
    // ============================
	bmp_extract_sprite(2  ,5 , TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_up);
	bmp_extract_sprite(23, 5 , TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_up_2);


	// ============================
    // Fill player 1 with animation TANK_DOWN and
    // ============================
	bmp_extract_sprite(43  , 10 , TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_down);
	bmp_extract_sprite(63  , 10  , TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_down_2);

	// ============================
    // Fill player 1 with animation TANK_LEFT and
    // ============================
	bmp_extract_sprite(83  , 8 , TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_left);
	bmp_extract_sprite(102 , 8 , TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_left_2);

	// ============================
    // Fill player 1 with animation TANK_RIGHT and
    // ============================
	bmp_extract_sprite(124 , 8 , TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_right);
	bmp_extract_sprite(145 , 8 , TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_right_2);

	// ============================
    // Fill bullet animation
    // ============================


    // Bullet tank 1
	bmp_extract_sprite(252 , 14, TANK_BULLET_WIDTH, TANK_BULLET_HEIGHT, player1.sprite_tank_bullet);
	bmp_extract_sprite(259 , 14, TANK_BULLET_WIDTH, TANK_BULLET_HEIGHT, player1.sprite_tank_bullet2);
	
	// Explosion. 13x13, the only sprites in the sheet that are not tank
	// sized: extracting them with TANK_WIDTH would drag in the gap between
	// cells plus the first columns of the next one, gluing a piece of one
	// frame to the right of the other.
	bmp_extract_sprite(171 , 11 , EXPLOSION_WIDTH, EXPLOSION_HEIGHT, player1.sprite_tank_explosion);
	bmp_extract_sprite(190 , 11 , EXPLOSION_WIDTH, EXPLOSION_HEIGHT, player1.sprite_tank_explosion2);

	
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
	bmp_extract_sprite(2  , 26 , TANK_WIDTH, TANK_HEIGHT, player2.sprite_tank_up);
	bmp_extract_sprite(23  , 26 , TANK_WIDTH, TANK_HEIGHT, player2.sprite_tank_up_2);


	// ============================
    // Fill player 2 with animation TANK_DOWN and
    // ============================
	bmp_extract_sprite(43  , 31 , TANK_WIDTH, TANK_HEIGHT, player2.sprite_tank_down);
	bmp_extract_sprite(63  , 31 , TANK_WIDTH, TANK_HEIGHT, player2.sprite_tank_down_2);

	// ============================
    // Fill player 2 with animation TANK_LEFT and
    // ============================
	bmp_extract_sprite(83  , 29 , TANK_WIDTH, TANK_HEIGHT, player2.sprite_tank_left);
	bmp_extract_sprite(102 , 29 , TANK_WIDTH, TANK_HEIGHT, player2.sprite_tank_left_2);

	// ============================
    // Fill player 2 with animation TANK_RIGHT and
    // ============================
	bmp_extract_sprite(124 , 29 , TANK_WIDTH, TANK_HEIGHT, player2.sprite_tank_right);
	bmp_extract_sprite(145 , 29 , TANK_WIDTH, TANK_HEIGHT, player2.sprite_tank_right_2);

	// ============================
    // Fill bullet animation
    // ============================


    // Bullet tank 2 - there is only one pair of bullets in the sprite
    // sheet, so both players shoot the same sprite
	bmp_extract_sprite(252 , 14, TANK_BULLET_WIDTH, TANK_BULLET_HEIGHT, player2.sprite_tank_bullet);
	bmp_extract_sprite(259 , 14, TANK_BULLET_WIDTH, TANK_BULLET_HEIGHT, player2.sprite_tank_bullet2);
	
	
	// Explosion - same as player 1, on the second row of the sheet
	bmp_extract_sprite(171 , 32 , EXPLOSION_WIDTH, EXPLOSION_HEIGHT, player2.sprite_tank_explosion);
	bmp_extract_sprite(192 , 32 , EXPLOSION_WIDTH, EXPLOSION_HEIGHT, player2.sprite_tank_explosion2);


	// Every sprite has been cut out by now, so the file is closed.
	//
	// The sheet is never loaded into memory at all any more. Those 64000 bytes
	// used to be alive at the same time as the 256000 byte map, and giving them
	// back afterwards left a hole the map could not use and the last WAV file
	// could not fit into. Reading the rows straight off disk costs a couple of
	// hundred seeks at startup and nothing ever again.
	bmp_close_sprite_sheet();

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

	// Nothing has been loaded yet at this point, so there is no engine sound
	// to point at. main() fills it in once the WAV files are in memory.
	player1.sound_engine_sample = -1;

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

	player2.sound_engine_sample = -1;

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