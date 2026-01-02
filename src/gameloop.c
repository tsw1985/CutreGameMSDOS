//Game loop

#include <stdio.h>
#include "header\gameloop.h"
#include "header\players.h"


void init_game_loop(){
	struct player player1;
	
	
	printf("Hello from main loop !!\n");
	
	//Create Player 1
	init_player(&player1);
	
	printf("PLAYER 1 Y %d\n",player1.position_y);
	printf("PLAYER 1 X %d\n",player1.position_x);
	
	
}


