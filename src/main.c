#include <stdio.h>
#include <conio.h>
#include "header\util.h"
#include "header\bmp.h"
//#include "header\gameloop.h"

#include "header\players.h"

// asm function
extern void hola();
extern set_vga_320_200_mode();


/* 

	GAME FOLDER IN BACK UP : CutreGameMSDOS

*/

void setup_screen();
void init_graphics();
void init_players();

/*  Players */
struct player player1;


int main(){

	// Init Players and buffers	
    init_players();
	
	//setup_screen();
	//init_graphics();

	//init_game_loop();
	
	player_free(&player1);
	bmp_delete_buffers();
	bmp_close_files();
		
	getch();
	return 0;
}

void setup_screen(){
	//Init 320x200 VGA Mode	
	set_vga_320_200_mode();
}

void init_graphics(){
	
	// Create all buffers	
	bmp_init_buffers();
	
	//bmp_load_background_game("c:\\sprites.bmp");
	bmp_load_background_game("c:\\cutre.bmp");
	
	// Load sprites and images from file
	bmp_load_sprites_images("c:\\sprites.bmp",&player1);

	bmp_delete_buffers();
	bmp_close_files();
	
}

void init_players(){

	printf("Players Initialization ... !!\n");
	
	// Init Player 1
	player_init(&player1);
	
	printf("-- PLAYER 1 Y COORD %d\n",player1.position_y);
	printf("-- PLAYER 1 X COORD %d\n",player1.position_x);	

	
	
}