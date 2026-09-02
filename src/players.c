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
	
	 _player->sprite_tank_bullet    = (char*)malloc(TANK_BULLET_WIDTH * TANK_BULLET_HEIGHT);
     _player->sprite_tank_bullet2 = (char*)malloc(TANK_BULLET_WIDTH * TANK_BULLET_HEIGHT);	
	
	
	
	
	
	
}


void player_update_cannon_tip(struct player *_player){

	// Recompute where the cannon tip would be for each of the 4 directions,
	// from the tank's current position. Only the one matching
	// current_direction is actually the tip right now, but keeping all 4
	// up to date means whoever reads them later does not need to guess
	// which one is valid.
	_player->canonn_head_top_up_x = _player->position_x + CANNON_TIP_OFFSET_UP_X;
	_player->canonn_head_top_up_y = _player->position_y + CANNON_TIP_OFFSET_UP_Y;

	_player->canonn_head_top_down_x = _player->position_x + CANNON_TIP_OFFSET_DOWN_X;
	_player->canonn_head_top_down_y = _player->position_y + CANNON_TIP_OFFSET_DOWN_Y;

	_player->canonn_head_top_left_x = _player->position_x + CANNON_TIP_OFFSET_LEFT_X;
	_player->canonn_head_top_left_y = _player->position_y + CANNON_TIP_OFFSET_LEFT_Y;

	_player->canonn_head_top_right_x = _player->position_x + CANNON_TIP_OFFSET_RIGHT_X;
	_player->canonn_head_top_right_y = _player->position_y + CANNON_TIP_OFFSET_RIGHT_Y;

}


void player_update_bullet_position(struct player *_player){

	// Place the bullet sprite on the cannon tip of the direction the tank
	// is facing right now. player_update_cannon_tip() has already worked
	// out the 4 tips for the tank's current position, so here we only pick
	// the pair that matches current_direction.
	//
	// That tip is the very same pixel the collision check reads, so the
	// bullet is drawn exactly where a shot would come out of the cannon.
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


void player_fire_bullet(struct player *_player){

	// Only one bullet can be in the air at a time: while the previous shot
	// is still flying, the space bar does nothing.
	if (_player->bullet_is_flying == 1){
		return;
	}

	// Freeze the direction the tank is facing right now. From here on the
	// bullet ignores current_direction, so it keeps going the same way even
	// if the tank turns around straight after firing.
	_player->bullet_direction = _player->current_direction;

	// Recompute the cannon tips from the tank's position at this very
	// moment, so the shot starts exactly at the mouth of the cannon and not
	// one frame behind it.
	player_update_cannon_tip(_player);
	player_update_bullet_position(_player);

	_player->bullet_is_flying = 1;

}


void player_move_bullet(struct player *_player){

	// Advance the bullet one BULLET_PIXEL_TO_MOVE step in the direction it
	// was fired. This only deals with geometry: whether the bullet is still
	// on screen. Hitting a wall is decided by the caller, which is the one
	// that can read the collision map.
	//
	// The bounds are checked BEFORE moving, exactly like move_sprite() does
	// for the tank: bullet_position_x/y are "unsigned int", so subtracting
	// past 0 would wrap around to 65535 and the collision read that follows
	// would land far outside the buffer.
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

	// Tentative position, one PIXEL_TO_MOVE step further in "direction".
	// This mirrors the same clamping move_sprite() does, but without
	// actually committing it to _player->position_x/position_y.
	//
	// From that tentative position, the 3 points that are checked against
	// the collision map are worked out: the cannon tip (the middle of the
	// front of the tank) and the 2 tracks (the two ends of that front).
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
	
	
}