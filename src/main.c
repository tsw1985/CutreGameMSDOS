#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include "header\util.h"
#include "header\bmp.h"
#include "header\players.h"

#define PIXEL_TO_MOVE 		1
#define FRAMES_COUNTER	1

#define KEY_UP 					72
#define KEY_DOWN 				80
#define KEY_LEFT 				75
#define KEY_RIGHT 				77
#define KEY_SCAPE 			27
#define MOVE_UP 				1
#define MOVE_DOWN 			2
#define MOVE_LEFT 			3
#define MOVE_RIGHT 			4
#define SCREEN_SIZE 			64000


// asm function
extern void hola();
extern set_vga_320_200_mode();

/* 
	GAME FOLDER IN BACK UP : CutreGameMSDOS
*/

void setup_screen();
void init_graphics();
void init_players();
int get_key_pressed();
void wait_retrace();
void update_game(int direction);
void move_sprite(int direction);
void draw_to_buffer();

/*  Players */
struct player player1;
struct player player2;

//=====================
// Frame counter:
//
// This variable is used to count how many retraces are in screen

//=====================
int frame_counter;

int main(){

	unsigned int key_pressed = 0;
	
	// Init Players and buffers	
	setup_screen();
	init_players();
	init_graphics();
	
	//main loop
	
    do{
	    
	    if( kbhit()){
			key_pressed = get_key_pressed();
			//printf("TECLA PULSADA %d\n",key_pressed);	
		}
		// 2. Update logic game
   		update_game(key_pressed);
  		// 3. Double buffering
   		draw_to_buffer();
   		// 4. Wait vertial retrace
   		wait_retrace();
   		// 5. Show new map in screen
   		bmp_paint_image_data_to_vga(buffer_background_image_data);
    }while(key_pressed != KEY_SCAPE);
    
    
    
    //getch();
	player_free(&player1);
	bmp_delete_buffers();
	bmp_close_files();
		
	
	return 0;
}

void update_game(int direction){
	
	frame_counter++;
	// 3
    if (frame_counter >= FRAMES_COUNTER) {
        frame_counter = 0;
        move_sprite(direction);
    }
}

void move_sprite(int direction){
	
	
	if (direction == MOVE_UP){
		//player1.position_y--;
		player1.position_y = player1.position_y - PIXEL_TO_MOVE;
	}else if (direction == MOVE_DOWN){
		//player1.position_y++;
		player1.position_y = player1.position_y + PIXEL_TO_MOVE;
	}else if (direction == MOVE_LEFT){
		//player1.position_x--;
		player1.position_x = player1.position_x - PIXEL_TO_MOVE;
	}else if (direction == MOVE_RIGHT){
		//player1.position_x++;
		player1.position_x = player1.position_x + PIXEL_TO_MOVE;
	}
}

void draw_to_buffer(){
	
	// Copy again original map to current buffer to show in screen
	memcpy(buffer_background_image_data,buffer_original_background_bmp,SCREEN_SIZE);
	
	// Pul the tank in new position
	draw_sprite_to_buffer(player1.sprite_tank_up, 
	                              TANK_WIDTH, 
	                              TANK_HEIGHT, 
	                              player1.position_x, 
	                              player1.position_y, 
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
	
	// ============================
	// Extract sprites from sprites.bmp
	// ============================
	bmp_fill_sprites_in_buffer("c:\\sprites.bmp");
    bmp_revert_bmp(buffer_sprites_data);
	
    // ============================
	// Extract the background_image data from background file
	// ============================
	bmp_fill_buffer_with_image_data_from_file(buffer_background_image_data, file_background_image_game);

	// ============================
    // Fill player 1 with animation TANK_UP and
    // ============================
	bmp_extract_sprite(buffer_sprites_data, 2,6, TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_up);
	
	// ============================
    // Fill player 1 with animation TANK_DOWN and
    // ============================
	bmp_extract_sprite(buffer_sprites_data, 37,6, TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_down);
	
	// ============================
    // Fill player 1 with animation TANK_LEFT and
    // ============================
	bmp_extract_sprite(buffer_sprites_data, 56,6, TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_left);
	
	// ============================
    // Fill player 1 with animation TANK_RIGHT and
    // ============================
	bmp_extract_sprite(buffer_sprites_data, 20,6, TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_right);

	
	
	// Put Sprite in background buffer . Set player in position
	draw_sprite_to_buffer(player1.sprite_tank_up, 
	                              TANK_WIDTH, 
	                              TANK_HEIGHT, 
	                              player1.position_x, 
	                              player1.position_y, 
	                              buffer_background_image_data);
	                              	                              

	/*	                              	                              	
	draw_sprite_to_buffer(player1.sprite_tank_down, 
	                              TANK_WIDTH, 
	                              TANK_HEIGHT, 
	                              300, 
	                              0, 
	                              buffer_background_image_data);
	                              
	draw_sprite_to_buffer(player1.sprite_tank_left, 
	                              TANK_WIDTH, 
	                              TANK_HEIGHT, 
	                              80, 
	                              30, 
	                              buffer_background_image_data);
	                              
	                              
	draw_sprite_to_buffer(player1.sprite_tank_right, 
	                              TANK_WIDTH, 
	                              TANK_HEIGHT, 
	                              40, 
	                              100, 
	                              buffer_background_image_data);
	                              */

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


int get_key_pressed() {
    int key;
    
    //if the pressed key is extended the first value is 0
    key = getch(); 
    if(key == 0 || key == 224) {
        key = getch();  
        switch(key) {
            case KEY_UP:     
            	 return MOVE_UP;
            case KEY_DOWN: 
            	 return MOVE_DOWN;
            case KEY_LEFT:
            	 return MOVE_LEFT;
            case KEY_RIGHT:
            	 return MOVE_RIGHT;
        }
    }
    
    return key;  // Normal key
}

void wait_retrace(void)
{
    while (inp(0x3DA) & 0x08);   // wait current retrace
    while (!(inp(0x3DA) & 0x08)); // wait to start next retrace
}