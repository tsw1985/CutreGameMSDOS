#include "header\players.h"
#include <alloc.h>

void player_init(struct player *_player){
	
	_player->position_y = 80;
	_player->position_x = 80;
	_player->sprite_tank_up = (char*)malloc( (TANK_WIDTH * TANK_HEIGHT) * sizeof(char*));
	if(_player->sprite_tank_up == NULL){
		printf("Error creating player_buffer_sprites_data\n");	
	}
	
}


void player_free(struct player *_player){
	free(_player->sprite_tank_up);	
}