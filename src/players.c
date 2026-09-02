//===========================================================
// The geometry of a tank: where it is, where it would be, where its cannon
// tip is, and how its bullet and its explosion behave.
//
// Nothing here knows about the map, the screen or the keyboard. Whoever
// needs to read the collision map or paint pixels does it in main.c. That
// is why player_move_bullet() only checks the edges of the screen, and the
// walls are somebody else's problem.
//===========================================================

#include "header\players.h"
#include "header\bmp.h"     /* solo por WIDTH / HEIGHT: los limites de la pantalla */
#include <alloc.h>

void player_init(struct player *_player){
	
	_player->sprite_tank_up = (char*)malloc(TANK_WIDTH * TANK_HEIGHT);
	if(_player->sprite_tank_up == NULL){
		printf("Error creating player_buffer_sprites_data\n");	
	}
	_player->sprite_tank_up_2 = (char*)malloc(TANK_WIDTH * TANK_HEIGHT);
	if(_player->sprite_tank_up_2 == NULL){
		printf("Error creating player_buffer_sprites_data\n");	
	}
	
	
	_player->sprite_tank_down = (char*)malloc(TANK_WIDTH * TANK_HEIGHT);
	if(_player->sprite_tank_down == NULL){
		printf("Error creating player_buffer_sprites_data\n");	
	}
	_player->sprite_tank_down_2 = (char*)malloc(TANK_WIDTH * TANK_HEIGHT);
	if(_player->sprite_tank_down_2 == NULL){
		printf("Error creating player_buffer_sprites_data\n");	
	}
	
	
	_player->sprite_tank_left = (char*)malloc(TANK_WIDTH * TANK_HEIGHT);
	if(_player->sprite_tank_left == NULL){
		printf("Error creating player_buffer_sprites_data\n");	
	}
	_player->sprite_tank_left_2 = (char*)malloc(TANK_WIDTH * TANK_HEIGHT);
	if(_player->sprite_tank_left_2 == NULL){
		printf("Error creating player_buffer_sprites_data\n");	
	}
	
	
	_player->sprite_tank_right = (char*)malloc(TANK_WIDTH * TANK_HEIGHT);
	if(_player->sprite_tank_right == NULL){
		printf("Error creating player_buffer_sprites_data\n");	
	}
	_player->sprite_tank_right_2 = (char*)malloc(TANK_WIDTH * TANK_HEIGHT);
	if(_player->sprite_tank_right_2 == NULL){
		printf("Error creating player_buffer_sprites_data\n");	
	}
	
	
	//#define TANK_BULLET_WIDTH 4
	//#define TANK_BULLET_HEIGHT 3
	
	 _player->sprite_tank_bullet = (char*)malloc(TANK_BULLET_WIDTH * TANK_BULLET_HEIGHT);
	 if (_player->sprite_tank_bullet == NULL){
		printf("Error creating player_buffer_sprites_data\n");	
	 }
	 
	 
     _player->sprite_tank_bullet2 = (char*)malloc(TANK_BULLET_WIDTH * TANK_BULLET_HEIGHT);	
	 if (_player->sprite_tank_bullet2 == NULL){
		printf("Error creating player_buffer_sprites_data\n");	
	 }
	 
	 
	 
	 _player->sprite_tank_explosion = (char*)malloc(EXPLOSION_WIDTH * EXPLOSION_HEIGHT);
	 if (_player->sprite_tank_explosion == NULL){
		printf("Error creating player_buffer_sprites_data\n");	
	 }
	 
	_player->sprite_tank_explosion2 = (char*)malloc(EXPLOSION_WIDTH * EXPLOSION_HEIGHT);
	 if (_player->sprite_tank_explosion2 == NULL){
		printf("Error creating player_buffer_sprites_data\n");	
	 }
	
	
	
	
	
}


//===========================================================
// Adds an offset to a position, never going below 0.
//
// The CANNON_TIP_OFFSET_* values are tuned by eye and some are NEGATIVE
// (UP_Y is -1), while positions are unsigned. On the top row of the screen
// 0 + (-1) would not be -1, it would wrap round to 65535, and every read
// from that coordinate would land outside the map.
//
// Doing the sum in a signed int and clamping at 0 means any offset can be
// tuned negative without that happening.
//===========================================================
unsigned int player_add_offset(unsigned int position, int offset){

	int result;

	result = (int)position + offset;

	if (result < 0){
		result = 0;
	}

	return (unsigned int)result;

}


void player_update_cannon_tip(struct player *_player){

	// All 4 tips are recomputed, not just the one matching
	// current_direction, so whoever reads them later never has to guess
	// which one is valid.
	//
	// These are what the bullet sits on and what the log reads. The
	// collision check does NOT use them: it uses the "future" values
	// computed below, from the tentative position.
	_player->canonn_head_top_up_x = player_add_offset(_player->position_x, CANNON_TIP_OFFSET_UP_X);
	_player->canonn_head_top_up_y = player_add_offset(_player->position_y, CANNON_TIP_OFFSET_UP_Y);

	_player->canonn_head_top_down_x = player_add_offset(_player->position_x, CANNON_TIP_OFFSET_DOWN_X);
	_player->canonn_head_top_down_y = player_add_offset(_player->position_y, CANNON_TIP_OFFSET_DOWN_Y);

	_player->canonn_head_top_left_x = player_add_offset(_player->position_x, CANNON_TIP_OFFSET_LEFT_X);
	_player->canonn_head_top_left_y = player_add_offset(_player->position_y, CANNON_TIP_OFFSET_LEFT_Y);

	_player->canonn_head_top_right_x = player_add_offset(_player->position_x, CANNON_TIP_OFFSET_RIGHT_X);
	_player->canonn_head_top_right_y = player_add_offset(_player->position_y, CANNON_TIP_OFFSET_RIGHT_Y);

}


void player_update_bullet_position(struct player *_player){

	// Pick the tip that matches the direction the tank is facing. That tip
	// is the same pixel the collision check reads, so the bullet sits
	// exactly where a shot would come out of the cannon.
	if (_player->current_direction == MOVE_UP){

		_player->bullet_position_x = _player->canonn_head_top_up_x;
		_player->bullet_position_y = _player->canonn_head_top_up_y;

	}else if (_player->current_direction == MOVE_DOWN){

		_player->bullet_position_x = _player->canonn_head_top_down_x;
		_player->bullet_position_y = _player->canonn_head_top_down_y;

	}else if (_player->current_direction == MOVE_LEFT){

		_player->bullet_position_x = _player->canonn_head_top_left_x;
		_player->bullet_position_y = _player->canonn_head_top_left_y;

	}else{ // MOVE_RIGHT

		_player->bullet_position_x = _player->canonn_head_top_right_x;
		_player->bullet_position_y = _player->canonn_head_top_right_y;

	}

}


int player_fire_bullet(struct player *_player){

	// One bullet in the air at a time: while the shot is still flying, the
	// fire key does nothing. Saying so with the return value matters,
	// because the caller has to know not to play the firing sound for a
	// shot that never happened.
	if (_player->bullet_is_flying == 1){
		return 0;
	}

	// Freeze the facing direction. From here on the bullet ignores
	// current_direction, so it keeps going the same way even if the tank
	// turns around right after firing.
	_player->bullet_direction = _player->current_direction;

	// Recompute from the position at this very instant, so the shot starts
	// at the mouth of the cannon and not one frame behind it.
	player_update_cannon_tip(_player);
	player_update_bullet_position(_player);

	_player->bullet_is_flying = 1;

	return 1;

}


void player_move_bullet(struct player *_player){

	// One step in the direction it was fired. Only geometry: whether the
	// bullet is still on screen. Walls and tanks are the caller's business.
	//
	// The edges are checked BEFORE moving, like move_sprite() does: the
	// positions are unsigned, so subtracting past 0 would wrap round to
	// 65535 and the read that follows would land far outside the buffer.
	if (_player->bullet_is_flying == 0){
		return;
	}

	if (_player->bullet_direction == MOVE_UP){

		if (_player->bullet_position_y >= BULLET_PIXEL_TO_MOVE){
			_player->bullet_position_y = _player->bullet_position_y - BULLET_PIXEL_TO_MOVE;
		}else{
			_player->bullet_is_flying = 0;
		}

	}else if (_player->bullet_direction == MOVE_DOWN){

		if (_player->bullet_position_y + BULLET_PIXEL_TO_MOVE + TANK_BULLET_HEIGHT <= HEIGHT){
			_player->bullet_position_y = _player->bullet_position_y + BULLET_PIXEL_TO_MOVE;
		}else{
			_player->bullet_is_flying = 0;
		}

	}else if (_player->bullet_direction == MOVE_LEFT){

		if (_player->bullet_position_x >= BULLET_PIXEL_TO_MOVE){
			_player->bullet_position_x = _player->bullet_position_x - BULLET_PIXEL_TO_MOVE;
		}else{
			_player->bullet_is_flying = 0;
		}

	}else{ // MOVE_RIGHT

		if (_player->bullet_position_x + BULLET_PIXEL_TO_MOVE + TANK_BULLET_WIDTH <= WIDTH){
			_player->bullet_position_x = _player->bullet_position_x + BULLET_PIXEL_TO_MOVE;
		}else{
			_player->bullet_is_flying = 0;
		}

	}

}


void player_update_future_collision_points(struct player *_player, int direction){

	// Tentative position, one step further in "direction". Mirrors the
	// clamping move_sprite() does, but without committing it to the tank.
	// From it come the 3 points the collision check reads: the cannon tip
	// and the 2 tracks, at both ends of the leading edge.
	unsigned int future_position_x;
	unsigned int future_position_y;

	future_position_x = _player->position_x;
	future_position_y = _player->position_y;

	if (direction == MOVE_UP){

		if (future_position_y >= PIXEL_TO_MOVE){
			future_position_y = future_position_y - PIXEL_TO_MOVE;
		}else{
			future_position_y = 0;
		}

		_player->future_cannon_tip_x = future_position_x + CANNON_TIP_OFFSET_UP_X;
		_player->future_cannon_tip_y = future_position_y + CANNON_TIP_OFFSET_UP_Y + 2;

		_player->future_track1_x = future_position_x + TRACK1_OFFSET_UP_X;
		_player->future_track1_y = future_position_y + TRACK1_OFFSET_UP_Y;

		_player->future_track2_x = future_position_x + TRACK2_OFFSET_UP_X;
		_player->future_track2_y = future_position_y + TRACK2_OFFSET_UP_Y;

	}else if (direction == MOVE_DOWN){

		future_position_y = future_position_y + PIXEL_TO_MOVE;

		_player->future_cannon_tip_x = future_position_x + CANNON_TIP_OFFSET_DOWN_X;
		_player->future_cannon_tip_y = future_position_y + CANNON_TIP_OFFSET_DOWN_Y - 2;

		_player->future_track1_x = future_position_x + TRACK1_OFFSET_DOWN_X;
		_player->future_track1_y = future_position_y + TRACK1_OFFSET_DOWN_Y;

		_player->future_track2_x = future_position_x + TRACK2_OFFSET_DOWN_X;
		_player->future_track2_y = future_position_y + TRACK2_OFFSET_DOWN_Y;

	}else if (direction == MOVE_LEFT){

		if (future_position_x >= PIXEL_TO_MOVE){
			future_position_x = future_position_x - PIXEL_TO_MOVE;
		}else{
			future_position_x = 0;
		}

		_player->future_cannon_tip_x = future_position_x + CANNON_TIP_OFFSET_LEFT_X ;
		_player->future_cannon_tip_y = future_position_y + CANNON_TIP_OFFSET_LEFT_Y;

		_player->future_track1_x = future_position_x + TRACK1_OFFSET_LEFT_X;
		_player->future_track1_y = future_position_y + TRACK1_OFFSET_LEFT_Y;

		_player->future_track2_x = future_position_x + TRACK2_OFFSET_LEFT_X;
		_player->future_track2_y = future_position_y + TRACK2_OFFSET_LEFT_Y;

	}else{ // MOVE_RIGHT

		future_position_x = future_position_x + PIXEL_TO_MOVE;

		_player->future_cannon_tip_x = future_position_x + CANNON_TIP_OFFSET_RIGHT_X;
		_player->future_cannon_tip_y = future_position_y + CANNON_TIP_OFFSET_RIGHT_Y;

		_player->future_track1_x = future_position_x + TRACK1_OFFSET_RIGHT_X;
		_player->future_track1_y = future_position_y + TRACK1_OFFSET_RIGHT_Y;

		_player->future_track2_x = future_position_x + TRACK2_OFFSET_RIGHT_X;
		_player->future_track2_y = future_position_y + TRACK2_OFFSET_RIGHT_Y;

	}

	// The corner too: the wall check works with the 3 points, but the tank
	// against tank check needs the whole box, and this is the only place
	// where that corner is known.
	_player->future_position_x = future_position_x;
	_player->future_position_y = future_position_y;

}


void player_reset(struct player *_player, unsigned int start_x, unsigned int start_y, unsigned int start_direction){

	// Used both to set the game up and to restart a round after a hit, so
	// both go through exactly the same code and can never drift apart.
	//
	// fire_was_pressed is deliberately NOT touched: if a player is holding
	// the fire key when the round restarts, that has to survive, or the tank
	// would shoot by itself without the player releasing the key.
	_player->position_x = start_x;
	_player->position_y = start_y;
	_player->current_direction = start_direction;

	_player->current_frame = 0;
	_player->speed_counter = 0;
	_player->is_moving = 0;

	_player->bullet_direction = 0;
	_player->bullet_is_flying = 0;

	// The tank is whole again for the new round
	_player->is_exploding = 0;
	_player->explosion_counter = 0;
	_player->explosion_current_frame = 0;

	// Cannon tips for the new position, and the loaded bullet on the right
	// one, so the first frame of the new round has valid coordinates
	// instead of leftovers from the round that just ended.
	player_update_cannon_tip(_player);
	player_update_bullet_position(_player);

}


//===========================================================
// Blows this tank up: draw_to_buffer() now paints the explosion where the
// tank was. The tank is deliberately NOT moved: it stays where it was shot,
// which is where the explosion has to appear. It goes home later, when the
// pause ends and the round restarts.
//===========================================================
void player_start_explosion(struct player *_player){

	_player->is_exploding = 1;
	_player->explosion_counter = 0;
	_player->explosion_current_frame = 0;

	// A tank that has just been blown up is not moving, so its tracks must
	// not keep rolling underneath the explosion
	_player->is_moving = 0;

}


//===========================================================
// Swaps between the 2 explosion sprites so the fire flickers.
//
// Kept apart from update_player_animation() on purpose: the tracks only
// advance when the tank has moved, and an exploding tank by definition is
// not going anywhere.
//
// How LONG it lasts is not decided here, but by the pause counter in
// main.c, which is also the one that knows when to restart the round.
//===========================================================
void player_update_explosion(struct player *_player){

	if (_player->is_exploding == 0){
		return;
	}

	_player->explosion_counter = _player->explosion_counter + 1;

	if (_player->explosion_counter >= EXPLOSION_FRAME_DELAY){

		_player->explosion_counter = 0;

		_player->explosion_current_frame = _player->explosion_current_frame + 1;
		if (_player->explosion_current_frame >= EXPLOSION_TOTAL_SPRITES){
			_player->explosion_current_frame = 0;
		}

	}

}


void player_free(struct player *_player){
	
	free(_player->sprite_tank_up);	
	free(_player->sprite_tank_up_2);	
	
	free(_player->sprite_tank_down);
	free(_player->sprite_tank_down_2);
	
	
	free(_player->sprite_tank_left);
	free(_player->sprite_tank_left_2);
	
	
	free(_player->sprite_tank_right);
	free(_player->sprite_tank_right_2);
	
	free(_player->sprite_tank_bullet);
	free(_player->sprite_tank_bullet2);
	
	free(_player->sprite_tank_explosion);
	free(_player->sprite_tank_explosion2);
	
	
}