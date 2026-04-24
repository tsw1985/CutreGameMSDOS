#include <stdio.h>
#include <conio.h>
#include "header\util.h"
#include "header\bmp.h"
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
	setup_screen();
	init_players();
	init_graphics();

	
	
   getch();
	
	player_free(&player1);
	bmp_delete_buffers();
	//bmp_close_files();
		
	
	return 0;
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
	bmp_fill_background_in_main_buffer("c:\\cutre.bmp");
	// Extract the pallete colors to save it un DAC
	bmp_extract_pallete_from_file("c:\\cutre.bmp");
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
	
	// Extract sprites from sprites.bmp
	bmp_fill_sprites_in_buffer("c:\\sprites.bmp");
	// Revert the sprites sheet	
	bmp_revert_bmp(buffer_sprites_data);
	
	// Extract the background_image data from background file
	bmp_fill_buffer_with_image_data_from_file(buffer_background_image_data, file_background_image_game);
    bmp_revert_bmp(buffer_background_image_data);

	bmp_fill_buffer_with_image_data_from_file(buffer_sprites_data, file_sprites_game);
    bmp_revert_bmp(buffer_sprites_data);
    
	// Put Sprite in background buffer
	bmp_extract_sprite(buffer_sprites_data, 0,0, 18,17, player1.sprite);

	
	draw_sprite_to_buffer(player1.sprite, 
	                              18, 
	                              17, 
	                              player1.position_x, 
	                              player1.position_y, 
	                              buffer_background_image_data);
	                              
	                              
	// 8 - Paint the background image in video memory
	bmp_paint_image_data_to_vga(buffer_background_image_data);
	//bmp_paint_image_data_to_vga(buffer_sprites_data);
	
}


void setup_screen(){
	//Init 320x200 VGA Mode	
	set_vga_320_200_mode();
}

void init_players(){

	printf("Players Initialization ... !!\n");
	
	// Init Player 1
	player_init(&player1);
	
	printf("-- PLAYER 1 Y COORD %d\n",player1.position_y);
	printf("-- PLAYER 1 X COORD %d\n",player1.position_x);	

	
	
}