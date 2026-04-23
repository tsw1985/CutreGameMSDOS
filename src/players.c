#include "header\players.h"
#include <alloc.h>

void player_init(struct player *_player){
	
	_player->position_y = 140;
	_player->position_x = 150;
	_player->sprite = (char*)malloc( (TANK_WIDTH * TANK_HEIGHT) * sizeof(char*));
	if(_player->sprite == NULL){
		printf("Error creating player_buffer_sprites_data\n");	
	}
	
}


void player_free(struct player *_player){
	free(_player->sprite);	
}