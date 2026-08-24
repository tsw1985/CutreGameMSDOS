#include "header\players.h"
#include <alloc.h>

void player_init(struct player *_player){
	
	
	/*	
	_player->position_y = 80;
	_player->position_x = 80;

	_player->current_frame = 0;
	_player->frame_counter = 0;
	_player->total_frames = 2;
	_player->speed_counter = 0;
	_player->speed_total = 2;
	*/
	
	
	
	
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


void player_update_future_cannon_tip(struct player *_player, int direction){

	// Tentative position, one PIXEL_TO_MOVE step further in "direction".
	// This mirrors the same clamping move_sprite() does, but without
	// actually committing it to _player->position_x/position_y.
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
		_player->future_cannon_tip_y = future_position_y + CANNON_TIP_OFFSET_UP_Y;

	}else if (direction == MOVE_DOWN){

		future_position_y = future_position_y + PIXEL_TO_MOVE;

		_player->future_cannon_tip_x = future_position_x + CANNON_TIP_OFFSET_DOWN_X;
		_player->future_cannon_tip_y = future_position_y + CANNON_TIP_OFFSET_DOWN_Y;

	}else if (direction == MOVE_LEFT){

		if (future_position_x >= PIXEL_TO_MOVE){
			future_position_x = future_position_x - PIXEL_TO_MOVE;
		}else{
			future_position_x = 0;
		}

		_player->future_cannon_tip_x = future_position_x + CANNON_TIP_OFFSET_LEFT_X;
		_player->future_cannon_tip_y = future_position_y + CANNON_TIP_OFFSET_LEFT_Y;

	}else{ // MOVE_RIGHT

		future_position_x = future_position_x + PIXEL_TO_MOVE;

		_player->future_cannon_tip_x = future_position_x + CANNON_TIP_OFFSET_RIGHT_X;
		_player->future_cannon_tip_y = future_position_y + CANNON_TIP_OFFSET_RIGHT_Y;

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
}