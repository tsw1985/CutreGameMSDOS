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