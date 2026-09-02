#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include "header\util.h"
#include "header\bmp.h"
#include "header\players.h"

#define FRAMES_COUNTER	3


// The main loop runs once per vertical retrace (see wait_retrace()), and
// VGA mode 13h (320x200, 256 colors) refreshes at approximately 70 Hz.
// So waiting for 70 loop iterations is roughly one second. This is only
// approximate, since it depends on the real refresh rate of the video
// card being used, not on an actual clock.
#define LOG_INTERVAL_FRAMES 70

#define SCREEN_SIZE 			64000

// Color/pallete index used in the map bmp (cutrecol.bmp) to mark a wall.
// If the pixel under the cannon tip is this color, movement in that
// direction is blocked.
//
// Confirmed by reading cutrecol.bmp directly: it only uses 2 colors,
// index 3 (blue, background/floor) and index 252 (yellow, wall).
#define COLLISION_COLOR 		252

// Keyboards Directions
#define DIRECTION_UP			0
#define DIRECTION_DOWN	1
#define DIRECTION_LEFT		2
#define DIRECTION_RIGHT		3

// Keyboard hardware ports
#define KEY_BUFFER 0x60

// Scan codes
//
// Player 1 plays with the cursor keys and fires with the space bar,
// player 2 plays with W/A/S/D and fires with G.
#define KEY_UP			0x48
#define KEY_DOWN		0x50
#define KEY_LEFT		0x4B
#define KEY_RIGHT		0x4D
#define KEY_ESC			0x01
#define KEY_SPACE		0x39

#define KEY_W			0x11
#define KEY_A			0x1E
#define KEY_S			0x1F
#define KEY_D			0x20
#define KEY_G			0x22

#define IRQ_KEYBOARD	9

// The scan codes are 128 .
// Create a array with 128 positions. If
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
                          unsigned char key_up_code,
                          unsigned char key_down_code,
                          unsigned char key_left_code,
                          unsigned char key_right_code,
                          unsigned char key_fire_code);
void draw_to_buffer();
void draw_explosion(struct player *_player);
void update_keyboard();

/*  Players */
struct player player1;
struct player player2;

//=====================
// Frame counter:
//
// This variable is used to count how many retraces are in screen

//=====================
int frame_counter;

//=====================
// Explosion pause counter:
//
// Number of main loop iterations left before the round is restarted after a
// hit. While it is greater than 0 the round is FROZEN: the keyboard is not
// read and the bullets do not move, so the surviving player cannot keep
// driving and shooting over a tank that is already blown up. Only the
// explosion animation and the screen keep running.
//
// 0 means the round is running normally.
//=====================
unsigned int explosion_pause_counter;

//=====================
// Log frame counter:
//
// This variable is separate from frame_counter above, so that throttling
// the debug log does not affect the speed of the animation. It counts how
// many loop iterations have passed since the last time we wrote to the log.
//=====================
int log_frame_counter;

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



int main(){

	// Buffer used to build the text of each log line before sending it to tanks_log()
	char log_message_text[64];

	// Color/pallete index of the map pixel that is currently under the
	// tank's cannon tip (position is now tracked on player1 itself, see
	// player_update_cannon_tip() in players.c)
	unsigned int cannon_tip_pixel_value;

	// Raised on any frame in which a bullet has hit a tank, so the round is
	// restarted once, after BOTH bullets have been dealt with. Doing it this
	// way means that if the two tanks shoot each other on the very same
	// frame, both shots count and both players get their win.
	unsigned int tank_was_hit;

	// Start each run with a fresh, empty log file, instead of mixing
	// lines from this run with lines left over from the previous run
	tanks_log_clear();

	tanks_log("Starting game ...");


	// Instal custom Vector ( INT 9 ) keyboard
	install_kbd();

	// Init Players and buffers
	setup_screen();
	init_players();
	init_graphics();

	// The first round starts running, not burning
	explosion_pause_counter = 0;

	//main loop

    do{

		// The round has two very different states, and this is where they
		// part ways:
		//
		//   running   -> the keyboard is read and the bullets move, which is
		//                the game itself.
		//
		//   exploding -> a tank has been hit. Everything above is frozen for
		//                EXPLOSION_TOTAL_FRAMES iterations (about half a
		//                second) while the explosion burns where the tank
		//                was shot, and only then is the round restarted.
		//                Freezing is what stops the survivor from driving
		//                and shooting over a tank that is already dead.
		if (explosion_pause_counter == 0){

			// 1. Read the keyboard, one call per player.
			//
			// Two separate calls means two separate if / else if chains, so
			// both tanks can move on the same frame. Player 1 drives with
			// the cursor keys and fires with the space bar, player 2 drives
			// with W/A/S/D and fires with G.
			// Each call is also told about the other tank, so a tank can be
			// stopped by the other one exactly like it is stopped by a wall.
			process_player_input(&player1, &player2, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_SPACE);
			process_player_input(&player2, &player1, KEY_W,  KEY_S,    KEY_A,    KEY_D,     KEY_G);

			// Move each bullet (or keep it sitting on its cannon, if that
			// player has not fired yet). Each one is checked against the
			// OTHER tank, and returns 1 if it has hit it.
			//
			// The tank that blows up is the one that was HIT, not the one
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

			// Both bullets have already been dealt with, so if they shot
			// each other on this very frame both shots counted and both
			// tanks are now burning.
			if (tank_was_hit == 1){

				sprintf(log_message_text, "Tank hit - wins %u / %u", player1.wins, player2.wins);
				tanks_log(log_message_text);

				// Put out any bullet still in the air. The round is frozen
				// from here on, so a bullet left flying would just hang in
				// mid air for half a second.
				player1.bullet_is_flying = 0;
				player2.bullet_is_flying = 0;

				// Start the pause. The round is NOT restarted here: the
				// tanks have to stay where they were shot for the explosion
				// to be drawn on top of them.
				explosion_pause_counter = EXPLOSION_TOTAL_FRAMES;

			}

		}else{

			// Burning: only the explosion animation moves
			player_update_explosion(&player1);
			player_update_explosion(&player2);

			explosion_pause_counter = explosion_pause_counter - 1;

			// The pause is over, so now the tanks go back to their starting
			// spots and a new round begins
			if (explosion_pause_counter == 0){
				restart_game();
			}

		}

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


    }while(!keys[KEY_ESC]);


	player_free(&player1);
	player_free(&player2);
	bmp_delete_buffers();
	bmp_close_files();

	uninstall_kbd();  /* NEVER REMOVE  */

	return 0;
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
// Advances the track animation of one tank by one frame, but only if it has
// actually moved since the last time (is_moving), so a parked tank does not
// keep rolling its tracks on the spot.
//
// This is the body that used to be inlined inside update_game() for
// player 1 alone; it is exactly the same logic, only reading the player it
// is given instead of the global player1.
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
// Answers whether the tank would run into a wall if it moved. It reads the
// 3 points that player_update_future_collision_points() has just worked
// out for the direction being tried: the cannon tip, in the middle of the
// front of the tank, and the 2 tracks, at both ends of that front.
//
// All three are needed. The cannon tip alone lets a corner of the tank go
// through the corner of a wall that the cannon passed next to, and the
// tracks alone let the cannon go into a wall the tracks are not touching
// yet.
//
// The reads are done on buffer_map_collisions_data (the dedicated
// collision map), never on VGA memory: that buffer never has the tank
// drawn on top of it.
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
// Answers whether the tank would run into the OTHER tank if it moved.
//
// It compares the box the tank WOULD occupy (worked out by
// player_update_future_collision_points(), which is called right before
// this) against the box the other tank occupies RIGHT NOW.
//
// The box is TANK_COLLISION_WIDTH x TANK_COLLISION_HEIGHT, smaller than the
// 18x18 sprite and centered inside it, so the two tanks are allowed to
// overlap a few pixels before one stops the other. Being generous here is
// on purpose: stopping a player short of a tank he can clearly still drive
// past feels worse than a couple of pixels of overlap.
//
// The move is simply refused when the boxes would overlap, nothing is
// pushed back. That is enough, and it is what guarantees the two tanks can
// never end up glued together: they start apart, every move is checked
// before it is applied, and turning does not change the box, so they can
// never reach a state where the boxes already overlap. As long as they
// never overlap, a blocked tank always has a free direction left (at the
// very least the one it just came from), so the player just turns and
// drives away.
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
// The single question process_player_input() asks before moving a tank:
// is there anything at all in the way? A wall, or the other tank.
//
// player_update_future_collision_points() must have been called for the
// direction being tried before calling this, since both checks read the
// "future" values it works out.
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
// Answers whether this player's bullet is hitting the OTHER tank.
//
// The bullet is tested by the same single pixel it uses against the map
// (its center), against the FULL 18x18 box of the other tank, not the
// smaller 14x14 box used for tank against tank. The two are different
// questions on purpose: for pushing you want to be forgiving, so the player
// is not stopped early, but for a hit you want to be generous, because a
// shot that looks like it hit has to count.
//
// A bullet is never tested against the tank that fired it. It is born on
// its own cannon tip, and the tip for RIGHT is at offset (15,8), which is
// inside its own 18x18 box: testing against the owner would kill the player
// the instant he fires to the right.
//
// There is no need for a swept test: the bullet travels
// BULLET_PIXEL_TO_MOVE (3) pixels per frame and the box is 18 pixels wide,
// so a bullet can never jump over a tank between two frames.
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
// Starts a new round: both tanks go back to where they started, facing each
// other, with their bullets loaded again.
//
// The scores (wins) are NOT touched, they are what carries over from one
// round to the next.
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
// Reads the keyboard for ONE player and turns it into movement and shots.
//
// The 5 scan codes are passed in instead of being hardcoded, so the very
// same function drives player 1 with the cursor keys + space bar and
// player 2 with W/A/S/D + G.
//
// IMPORTANT: each player gets its own call to this function, and therefore
// its own if / else if chain. The chain must NOT be shared between the two
// players: inside one chain only one key gets through per frame (that is
// what stops a single tank from moving in diagonal), so if both players
// were in the same chain only one of them would be able to move on any
// given frame.
//===========================================================
void process_player_input(struct player *_player,
                          struct player *_other,
                          unsigned char key_up_code,
                          unsigned char key_down_code,
                          unsigned char key_left_code,
                          unsigned char key_right_code,
                          unsigned char key_fire_code){

	// Only one direction can be applied per frame, so the tank can never
	// move in diagonal. If more than one direction key is held down at the
	// same time, only the first one below (in the order UP, DOWN, LEFT,
	// RIGHT) is used for this frame.
	//
	// Before moving, player_update_future_collision_points() works out where
	// the 3 collision points (cannon tip and both tracks) WOULD land, one
	// PIXEL_TO_MOVE step ahead, without actually moving the tank yet. Those
	// positions are checked against buffer_map_collisions_data (loaded by
	// bmp_fill_background_collision_in_buffer()), the dedicated collision
	// map, instead of VGA memory: this buffer never has the tank or anything
	// else drawn on top of it, so it is always safe to read regardless of
	// how close the tip already is. The tentative box is also checked
	// against the other tank. If it is a wall or the other tank, the key is
	// ignored and the tank does not move in that direction.
	if (keys[key_up_code]){

		player_update_future_collision_points(_player, MOVE_UP);

		if (is_move_blocked(_player, _other) == 0){
			_player->is_moving = 1;
			move_sprite(_player, MOVE_UP);
		}

	}else if (keys[key_down_code]){

		player_update_future_collision_points(_player, MOVE_DOWN);

		if (is_move_blocked(_player, _other) == 0){
			_player->is_moving = 1;
			move_sprite(_player, MOVE_DOWN);
		}

	}else if (keys[key_left_code]){

		player_update_future_collision_points(_player, MOVE_LEFT);

		if (is_move_blocked(_player, _other) == 0){
			_player->is_moving = 1;
			move_sprite(_player, MOVE_LEFT);
		}

	}else if (keys[key_right_code]){

		player_update_future_collision_points(_player, MOVE_RIGHT);

		if (is_move_blocked(_player, _other) == 0){
			_player->is_moving = 1;
			move_sprite(_player, MOVE_RIGHT);
		}

	}

	// The fire key is checked OUTSIDE the if/else if chain above, on
	// purpose: that chain only lets one direction through per frame so the
	// tank cannot move in diagonal, but shooting is not a direction, and the
	// tank has to be able to move and fire in the same frame.
	//
	// Only the frame in which the key GOES down counts. keys[] stays at 1
	// for as long as the key is held, so firing on the plain value would
	// shoot again by itself the moment the previous bullet died, turning it
	// into an automatic weapon. fire_was_pressed lives in the player, so
	// each one remembers its own key without main() needing a variable per
	// player.
	if (keys[key_fire_code]){

		if (_player->fire_was_pressed == 0){
			player_fire_bullet(_player);
		}

		_player->fire_was_pressed = 1;

	}else{

		_player->fire_was_pressed = 0;

	}

	// Keep the cannon tip position (for all 4 directions) up to date with
	// the tank's current position, so it is ready whenever it is needed
	// (log below, collision check on the next frame)
	player_update_cannon_tip(_player);

}


//===========================================================
// Moves the bullet of ONE player, and kills it when it reaches a wall.
//
// The bullet has two very different lives, so it is updated in two
// different ways:
//
//   not flying -> it is loaded in the cannon: it follows the tip of
//                 whichever direction the tank is facing, so it stays glued
//                 to the mouth of the cannon when the tank moves and when
//                 it turns.
//
//   flying     -> it travels on its own, ignoring the tank, and it dies
//                 either by leaving the screen (checked inside
//                 player_move_bullet()) or by reaching a wall (checked
//                 here). Once dead it goes back to being loaded, and the
//                 next frame puts it back on the cannon.
//
// The wall check lives here in main.c, and not in players.c next to
// player_move_bullet(), on purpose: players.c only deals with the geometry
// of a tank and knows nothing about the map. Reading the collision map is
// this file's job.
//===========================================================
int update_bullet(struct player *_player, struct player *_other){

	if (_player->bullet_is_flying == 0){

		player_update_bullet_position(_player);

		return 0;

	}

	player_move_bullet(_player);

	// player_move_bullet() may have just killed the bullet for leaving the
	// screen. Only read the collision map while it is still alive, so the
	// position being read is always a real point inside the 320x200 map.
	if (_player->bullet_is_flying == 0){
		return 0;
	}

	// The other tank is checked BEFORE the wall. It makes no difference in
	// practice, since a tank can never be standing on a wall, but a hit is
	// what the game is about, so it is the question worth asking first.
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

	// One pointer per player. They must be two separate variables: with a
	// single one, the block that picks player 2's sprite would overwrite
	// the choice already made for player 1, and both tanks would end up
	// drawn with the same sprite.
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


	// Put each tank in its new position, unless it has been blown up, in
	// which case its explosion is drawn in its place.
	//
	// The choice is made HERE, per tank, and not where draw_to_buffer() is
	// called: during an explosion the map and the surviving tank still have
	// to be drawn exactly as always, so this is not a case of drawing the
	// explosion INSTEAD of the frame, only instead of one tank sprite. It is
	// the same shape the bullets below already use.
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


	// Draw bullets
	// Show each bullet only if it is flying. While it is not, it is sitting
	// on the cannon tip (update_bullet() keeps it there) but it is not
	// painted, so the tank does not carry a visible bullet around.
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
// Draws the explosion of a tank that has been hit, in the place of its tank
// sprite.
//
// The explosion is 13x13 and the tank is 18x18, so it is pushed in
// EXPLOSION_OFFSET_X / EXPLOSION_OFFSET_Y ( 2 pixels ) on each side to sit
// in the middle of the box the tank was filling. Without that it would be
// drawn stuck to the top left corner of where the tank was, and would look
// off center.
//
// Which of the 2 sprites is showing is decided by
// player_update_explosion(), this only paints it.
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
	
	// Explosion
	//
	// These cells are 13x13, NOT the 18x18 of a tank, and they are the only
	// sprites in the sheet that are not tank sized. Their exact origins were
	// read off sprites.bmp: player 1's two frames start at (171,11) and
	// (190,11), player 2's at (171,32) and (192,32).
	//
	// Extracting them with TANK_WIDTH / TANK_HEIGHT would drag in the blank
	// gap that separates the two cells plus the first columns of the next
	// one, so each frame would come out with a piece of the other one glued
	// to its right.
	bmp_extract_sprite(buffer_sprites_data, 171 , 11 , EXPLOSION_WIDTH, EXPLOSION_HEIGHT, player1.sprite_tank_explosion);
	bmp_extract_sprite(buffer_sprites_data, 190 , 11 , EXPLOSION_WIDTH, EXPLOSION_HEIGHT, player1.sprite_tank_explosion2);

	
	// ==============================================================
	//           SPRITES PLAYER 2
	//
	// The second tank lives in sprites.bmp on a second row, exactly 21
	// pixels below the first one, and painted in a different color so the
	// two players can tell their tanks apart on screen. So every origin
	// below is the same X as player 1 with Y + 21:
	//
	//   UP:    (2,5)   -> (2,26)      DOWN:  (43,10) -> (43,31)
	//   LEFT:  (83,8)  -> (83,29)     RIGHT: (124,8) -> (124,29)
	//
	// The silhouette of both tanks is the same (checked cell by cell
	// against the sprite sheet), so player 2 reuses every
	// CANNON_TIP_OFFSET_* and TRACK*_OFFSET_* from players.h without any
	// recalculation.
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

	// Everything that is set here is set ONCE for the whole game: the
	// sprite buffers, the animation settings and the score. Everything that
	// belongs to a single round (position, facing direction, bullet) is set
	// by player_reset() at the bottom, which is the very same function that
	// restart_game() calls after a hit, so a fresh game and a fresh round
	// always start from exactly the same state.

	// ============================
	// INIT PLAYER 1
	// ============================

	player1.wins = 0;
	player1.frame_counter = 0;
	player1.total_frames = 2;
	player1.speed_total = 2;

	// Nothing has been fired yet, so no fire key is being held down
	player1.fire_was_pressed = 0;

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

	// Put both tanks on their starting spots, facing each other. This also
	// works out their cannon tips and puts their (loaded) bullets on them,
	// so the very first frame of the main loop already has valid
	// coordinates to check collisions with.
	restart_game();

}

void wait_retrace(void)
{
    while (inp(0x3DA) & 0x08);   // wait current retrace
    while (!(inp(0x3DA) & 0x08)); // wait to start next retrace
}