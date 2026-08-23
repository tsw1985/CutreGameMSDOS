#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include "header\util.h"
#include "header\bmp.h"
#include "header\players.h"

#define PIXEL_TO_MOVE 		2
#define FRAMES_COUNTER	3


// The main loop runs once per vertical retrace (see wait_retrace()), and
// VGA mode 13h (320x200, 256 colors) refreshes at approximately 70 Hz.
// So waiting for 70 loop iterations is roughly one second. This is only
// approximate, since it depends on the real refresh rate of the video
// card being used, not on an actual clock.
#define LOG_INTERVAL_FRAMES 70

#define MOVE_UP 				1
#define MOVE_DOWN 			2
#define MOVE_LEFT 			3
#define MOVE_RIGHT 			4

#define SCREEN_SIZE 			64000

// Keyboards Directions
#define DIRECTION_UP			0
#define DIRECTION_DOWN	1
#define DIRECTION_LEFT		2
#define DIRECTION_RIGHT		3

// Keyboard hardware ports
#define KEY_BUFFER 0x60

// Scan codes
#define KEY_UP			0x48
#define KEY_DOWN		0x50
#define KEY_LEFT		0x4B
#define KEY_RIGHT		0x4D
#define KEY_ESC			0x01
#define IRQ_KEYBOARD	9

// The scan codes are 128 .
// Create a array with 128 positions. If 
unsigned char keys[128] = {0};


// function pointer to save the old keyboard handler
void interrupt far (*old_kbd_handler)();
void interrupt far new_kbd_handler();

extern void hola();
extern void reset_pic();

extern set_vga_320_200_mode();
void setup_screen();
void init_graphics();
void init_players();
void wait_retrace();
void update_game(int direction);
void move_sprite(int direction);
void draw_to_buffer();
void update_keyboard();
void process_input();

/*  Players */
struct player player1;
struct player player2;

//=====================
// Frame counter:
//
// This variable is used to count how many retraces are in screen

//=====================
int frame_counter;

//=====================
// Log frame counter:
//
// This variable is separate from frame_counter above, so that throttling
// the debug log does not affect the speed of the animation. It counts how
// many loop iterations have passed since the last time we wrote to the log.
//=====================
int log_frame_counter;

// Install our custom interruption vector
void install_kbd()   { 
	old_kbd_handler = getvect(IRQ_KEYBOARD); 
	setvect(IRQ_KEYBOARD, new_kbd_handler); 
}

void uninstall_kbd() { 
	setvect(IRQ_KEYBOARD, old_kbd_handler); 
}


void interrupt far new_kbd_handler() {

	unsigned char scancode;

    asm {
	    in  al, 0x60      /* Read scan code by 0x60 Hardware Port */
        mov scancode, al
    }

    // When a key is pressed, the scancode is between 0 and 128
    // When a key is released the scancode is > 128.
    if (scancode & 0x80){ // if bit 7 is 1 , means key released
    	// is need substract 128 to access to the correct index and set a 0
        keys[scancode - 128] = 0;  /* Set to 0 like key released */
	}else{
        keys[scancode] = 1;          /* Set to 1 like pressed key */
    }

    /* Reset 8042 Controller and weak PIC */
    reset_pic();
    
}



int main(){

	// Buffer used to build the text of each log line before sending it to tanks_log()
	char log_message_text[64];

	// Start each run with a fresh, empty log file, instead of mixing
	// lines from this run with lines left over from the previous run
	tanks_log_clear();

	tanks_log("Starting game ...");

	
	// Instal custom Vector ( INT 9 ) keyboard
	install_kbd();
	
	// Init Players and buffers	
	setup_screen();
	init_players();
	init_graphics();
	
	//main loop
	
    do{
	    
		if (keys[KEY_UP]){
			player1.is_moving = 1;
       		move_sprite(MOVE_UP);
       }
       
    	if (keys[KEY_DOWN]){
	    	player1.is_moving = 1;
	    	move_sprite(MOVE_DOWN);
	    }
	     
    	if (keys[KEY_LEFT]){    
	    	player1.is_moving = 1;
	    	move_sprite(MOVE_LEFT);
    	}
    	
    	if (keys[KEY_RIGHT]){ 
	    	player1.is_moving = 1;
	    	move_sprite(MOVE_RIGHT);
		}
		
		// 2. Update logic game
   		update_game(0);
  		
   		// 3. Double buffering
   		draw_to_buffer();

   		// 4. Wait vertial retrace
   		wait_retrace();
   		
   		// 5. Show new map in screen
   		bmp_paint_image_data_to_vga(buffer_background_image_data);


   		// 6. Log the current direction, but only once every LOG_INTERVAL_FRAMES
   		//    frames, so we do not flood tanks.log thousands of times per second
   		log_frame_counter = log_frame_counter + 1;
   		if (log_frame_counter >= LOG_INTERVAL_FRAMES){
   			log_frame_counter = 0;

   			if (player1.current_direction == MOVE_UP){
   				sprintf(log_message_text, "Direction: UP");
   			}else if (player1.current_direction == MOVE_DOWN){
   				sprintf(log_message_text, "Direction: DOWN");
   			}else if (player1.current_direction == MOVE_LEFT){
   				sprintf(log_message_text, "Direction: LEFT");
   			}else{
   				sprintf(log_message_text, "Direction: RIGHT");
   			}

   			tanks_log(log_message_text);
   		}


    }while(!keys[KEY_ESC]);
    
    
	player_free(&player1);
	bmp_delete_buffers();
	bmp_close_files();

	uninstall_kbd();  /* NEVER REMOVE  */		
	
	return 0;
}

void update_game(int direction){
	
	//Animation
	player1.speed_counter = player1.speed_counter + 1;
	
	frame_counter++;
	// 3
    if (frame_counter >= FRAMES_COUNTER) {
        frame_counter = 0;
        
        // increment speed
		if (player1.speed_counter >= player1.speed_total){
			player1.speed_counter = 0;
			
			//check if player is moving
			//If player is in moving, then change frames
			if(player1.is_moving == 1){
				
				player1.current_frame = player1.current_frame + 1;
				if(player1.current_frame >= player1.total_frames){
					player1.current_frame = 0;
				}	
				
				// set to 0 player
				player1.is_moving = 0;
				
			}
		}
    }
    
}


void move_sprite(int direction){

	// Remember facing direction so draw_to_buffer() can pick the right sprite
	player1.current_direction = direction;

	if (direction == MOVE_UP){
		if (player1.position_y >= PIXEL_TO_MOVE){
			player1.position_y = player1.position_y - PIXEL_TO_MOVE;
		}else{
			player1.position_y = 0;
		}
	}else if (direction == MOVE_DOWN){
		if (player1.position_y + PIXEL_TO_MOVE <= HEIGHT - TANK_HEIGHT){
			player1.position_y = player1.position_y + PIXEL_TO_MOVE;
		}else{
			player1.position_y = HEIGHT - TANK_HEIGHT;
		}
	}else if (direction == MOVE_LEFT){
		if (player1.position_x >= PIXEL_TO_MOVE){
			player1.position_x = player1.position_x - PIXEL_TO_MOVE;
		}else{
			player1.position_x = 0;
		}
	}else if (direction == MOVE_RIGHT){
		if (player1.position_x + PIXEL_TO_MOVE <= WIDTH - TANK_WIDTH){
			player1.position_x = player1.position_x + PIXEL_TO_MOVE;
		}else{
			player1.position_x = WIDTH - TANK_WIDTH;
		}
	}

}

void draw_to_buffer(){

	char *sprite_to_draw;

	// Copy again original map to current buffer to show in screen
	memcpy(buffer_background_image_data,buffer_original_background_bmp,SCREEN_SIZE);


	/* DRAW FRAME of each animation list, according to the direction the player is facing */

	if ( player1.current_direction == MOVE_UP ){

		if ( player1.current_frame == 0 ){
			sprite_to_draw = player1.sprite_tank_up;
		}else{
			sprite_to_draw = player1.sprite_tank_up_2;
		}

	}else if ( player1.current_direction == MOVE_DOWN ){

		if ( player1.current_frame == 0 ){
			sprite_to_draw = player1.sprite_tank_down;
		}else{
			sprite_to_draw = player1.sprite_tank_down_2;
		}

	}else if ( player1.current_direction == MOVE_LEFT ){

		if ( player1.current_frame == 0 ){
			sprite_to_draw = player1.sprite_tank_left;
		}else{
			sprite_to_draw = player1.sprite_tank_left_2;
		}

	}else { // if ( player1.current_direction == MOVE_RIGHT ){

		if ( player1.current_frame == 0 ){
			sprite_to_draw = player1.sprite_tank_right;
		}else{
			sprite_to_draw = player1.sprite_tank_right_2;
		}

	}

	// Put the tank in new position
	draw_sprite_to_buffer(sprite_to_draw,
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
	bmp_extract_sprite(buffer_sprites_data,  2, 6 , TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_up);
	bmp_extract_sprite(buffer_sprites_data, 75, 6 , TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_up_2);
	
	
	// ============================
    // Fill player 1 with animation TANK_DOWN and
    // ============================
	bmp_extract_sprite(buffer_sprites_data, 37  ,6, TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_down);
	bmp_extract_sprite(buffer_sprites_data, 110 ,6, TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_down_2);
	
	// ============================
    // Fill player 1 with animation TANK_LEFT and
    // ============================
	bmp_extract_sprite(buffer_sprites_data, 56  ,6, TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_left);
	bmp_extract_sprite(buffer_sprites_data, 129,6, TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_left_2);
	
	// ============================
    // Fill player 1 with animation TANK_RIGHT and
    // ============================
	bmp_extract_sprite(buffer_sprites_data, 20 ,6, TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_right);
	bmp_extract_sprite(buffer_sprites_data, 93 ,6, TANK_WIDTH, TANK_HEIGHT, player1.sprite_tank_right_2);

	
	
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
	//printf("Players Initialization ... !!\n");
	// Init Player 1
	player_init(&player1);
}

void wait_retrace(void)
{
    while (inp(0x3DA) & 0x08);   // wait current retrace
    while (!(inp(0x3DA) & 0x08)); // wait to start next retrace
}