/* BMP HAndler */
#include "header\bmp.h"
#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <alloc.h>
#include <string.h>

//FILES
FILE *file_background_image_game = NULL;
FILE *file_sprites_game = NULL;


// INIT Global variables with EXTERN in bmp.h
unsigned char *vga = (unsigned char *) MK_FP(0xA000,0);

// The world. See the long note in bmp.h about world versus screen.
int map_width  = WIDTH;
int map_height = HEIGHT;
int camera_x   = 0;
int camera_y   = 0;

unsigned char huge *buffer_original_background_bmp = NULL; // the whole map picture
unsigned char *buffer_background_image_data = NULL;        // one screen, the frame being built
unsigned char *buffer_collision_mask = NULL;               // the whole map, 1 bit per pixel
unsigned char *buffer_palleta_data = NULL;

// The sprite sheet is NOT held in memory. See bmp_open_sprite_sheet().
FILE *file_sprites_game_open = NULL;

// One row of the map, while it is being read off disk. A row of a 640 wide
// map is too big to keep putting on the stack, and it is needed twice (once
// for the picture, once for the collision map), so it lives here.
static unsigned char map_line[MAP_MAX_WIDTH];


//===========================================================
// Flips the rows of a 320x200 buffer, because BMP stores them bottom-up.
//
// Only the sprite sheet goes through this now. The map does not: its loader
// reads the rows straight into the right place, which is the same work and
// does not need a temporary row.
//===========================================================
void bmp_revert_bmp(char *buffer){
	
	unsigned char temp_line[320];  // temporary buffer for one line
    int y;
    unsigned char *up_line;
    unsigned char *bottom_line;
    
    for(y = 0; y < HEIGHT / 2; y++) {
        up_line = buffer + (y * WIDTH);
        bottom_line = buffer + ((HEIGHT - 1 - y) * WIDTH);
        
        memcpy(temp_line, up_line, WIDTH);
        memcpy(up_line, bottom_line, WIDTH);
        memcpy(bottom_line, temp_line, WIDTH);
    }
	
}

void bmp_write_pallete_data_into_dac(char *pallete_data){
	
	unsigned int buffer_data_index = 0;
	unsigned int color_counter = 0;
	unsigned char r,v,a;
	
	for(color_counter = 0 ; color_counter <= 255 ; color_counter++){
		
		a = pallete_data[buffer_data_index++];  // Blue
		v = pallete_data[buffer_data_index++];  // Green
		r = pallete_data[buffer_data_index++];  // Red

		outportb(0x3c8, color_counter);
  	   	outportb(0x3c9, r);
  	   	outportb(0x3c9, v);
  	   	outportb(0x3c9, a);
	}
}

void bmp_load_pallete_data(char *buffer_data_dest , FILE *_file){
	
	int buffer_data_index = 0;
	unsigned char value;
	unsigned char r,v,a;
	unsigned int color_counter = 0; 
	
	//point to starting pallete_data info in file
	fseek(_file, 54L, SEEK_SET);
	
	do{
		
		// BLUE	
  		fread(&value,1,1,_file);
		a = (value/4);
		buffer_data_dest[buffer_data_index++] = a;  

		// GREEN	
		fread(&value,1,1,_file);
		v = (value/4);
		buffer_data_dest[buffer_data_index++] = v;

		// RED
		fread(&value,1,1,_file);
		r = (value/4);
		buffer_data_dest[buffer_data_index++] = r;

		// EMPTY but we must read
		fread(&value,1,1,_file);

   		color_counter++;
    
	}while(color_counter <= 255);
}


void bmp_fill_buffer_with_image_data_from_file(char *buffer_data_dest, FILE *file){
		//set where the image data begin
		fseek(file, 1078L , SEEK_SET);
		fread(buffer_data_dest,SCREEN_SIZE,1,file);
}

void bmp_paint_image_data_to_vga(char *buffer_image_data){
	memcpy(vga,buffer_image_data,SCREEN_SIZE);
}

//===========================================================
// Read the color/pallete index that is currently at pixel (x,y) in the
// real VGA video memory (segment 0xA000, the "vga" pointer).
//
// SCREEN coordinates, 0..319 / 0..199, NOT world coordinates.
//
// WARNING: by the time the main loop reads this, the tank sprite has
// already been drawn on top of the map for the current frame. So a
// coordinate that falls on the tank itself (like the cannon tip) will
// return the tank sprite's own color, not the map color underneath it.
// Do NOT use this for collision checks. Use bmp_is_wall() instead.
//===========================================================
unsigned char bmp_get_vga_pixel(unsigned int x, unsigned int y){

	unsigned int offset;

	offset = (y * WIDTH) + x;

	return vga[offset];

}

//===========================================================
// Read the color/pallete index at pixel (x,y) of the map picture, in WORLD
// coordinates. This buffer never has the tank drawn on top of it.
//
// It is here for logging and debugging. Do not use it to decide anything:
// the map picture is decoration and it draws walls with dark mortar lines
// between the bricks, so a point can be inside a wall and still read as
// black. bmp_is_wall() is the one that knows.
//===========================================================
unsigned char bmp_get_map_pixel(int x, int y){

	unsigned long offset;

	if (x < 0 || y < 0 || x >= map_width || y >= map_height){
		return 0;
	}

	offset = ((unsigned long)y * (unsigned long)map_width) + (unsigned long)x;

	return buffer_original_background_bmp[offset];

}

//===========================================================
// Is (x,y) a wall? WORLD coordinates. 1 = wall, 0 = floor.
//
// Reads the packed mask: one bit per pixel of the whole world. Working out
// which bit is a multiply, a shift and an and, and it gets called 3 times
// per tank per frame plus once per bullet. On an 8086 the multiply of two
// 16 bit numbers into a 32 bit result is a single MUL, so this is cheaper
// than it looks.
//
// Off the map counts as wall. The maps are drawn with a solid border 16
// pixels thick so it should never happen, but this is the line that keeps a
// badly drawn map from turning into a read somewhere else in memory.
//===========================================================
int bmp_is_wall(int x, int y){

	unsigned long bit_index;
	unsigned int  byte_index;
	unsigned char bit;

	if (x < 0){
		return 1;
	}
	if (y < 0){
		return 1;
	}
	if (x >= map_width){
		return 1;
	}
	if (y >= map_height){
		return 1;
	}

	if (buffer_collision_mask == NULL){
		return 0;
	}

	bit_index  = ((unsigned long)y * (unsigned long)map_width) + (unsigned long)x;
	byte_index = (unsigned int)(bit_index >> 3);
	bit        = (unsigned char)(1 << (unsigned int)(bit_index & 7L));

	if ((buffer_collision_mask[byte_index] & bit) != 0){
		return 1;
	}

	return 0;

}


//===========================================================
// Keeps the window inside the map.
//
// When the map is exactly one screen big both limits come out 0, the camera
// is pinned at 0,0 for ever, and everything behaves like it did before any
// of this existed.
//===========================================================
static void bmp_camera_clamp(){

	int limit_x;
	int limit_y;

	limit_x = map_width  - WIDTH;
	limit_y = map_height - HEIGHT;

	if (limit_x < 0){
		limit_x = 0;
	}
	if (limit_y < 0){
		limit_y = 0;
	}

	if (camera_x < 0){
		camera_x = 0;
	}
	if (camera_y < 0){
		camera_y = 0;
	}
	if (camera_x > limit_x){
		camera_x = limit_x;
	}
	if (camera_y > limit_y){
		camera_y = limit_y;
	}

}

//===========================================================
// The dead zone.
//
// While the target stays inside a rectangle in the middle of the screen the
// camera does not move one pixel. That is the whole idea: a camera that is
// always centred means the world slides under you every single frame, and in
// a tank game that reads as "everything is moving except me", which is
// horrible. This way the picture is still almost all the time.
//
// When the target does leave the rectangle, the camera moves by EXACTLY how
// far it has left by. Tank walks 2 pixels, camera walks 2 pixels. So there is
// no jump to catch up and no lag behind, and the moment the tank stops the
// camera stops with it.
//
// if / else if and not two ifs: with a dead zone bigger than half the screen
// both edges would be on the wrong side of each other and the two
// corrections would fight. The header says how big they may get.
//===========================================================
void bmp_camera_follow(int target_x, int target_y, int target_width, int target_height){

	int screen_x;
	int screen_y;
	int right_edge;
	int bottom_edge;

	// Where the target is INSIDE the window right now
	screen_x = target_x - camera_x;
	screen_y = target_y - camera_y;

	right_edge  = WIDTH  - CAMERA_DEAD_ZONE_X - target_width;
	bottom_edge = HEIGHT - CAMERA_DEAD_ZONE_Y - target_height;

	if (screen_x < CAMERA_DEAD_ZONE_X){
		camera_x = camera_x - (CAMERA_DEAD_ZONE_X - screen_x);
	}else if (screen_x > right_edge){
		camera_x = camera_x + (screen_x - right_edge);
	}

	if (screen_y < CAMERA_DEAD_ZONE_Y){
		camera_y = camera_y - (CAMERA_DEAD_ZONE_Y - screen_y);
	}else if (screen_y > bottom_edge){
		camera_y = camera_y + (screen_y - bottom_edge);
	}

	bmp_camera_clamp();

}

//===========================================================
// Target straight to the middle, no easing. For the start of a round: there
// is nothing to follow smoothly from when the tank has just been teleported
// back to its corner.
//===========================================================
void bmp_camera_snap(int target_x, int target_y, int target_width, int target_height){

	camera_x = target_x + (target_width  / 2) - (WIDTH  / 2);
	camera_y = target_y + (target_height / 2) - (HEIGHT / 2);

	bmp_camera_clamp();

}

//===========================================================
// Copies the visible window out of the world map and into a screen buffer.
//
// This replaces what used to be one memcpy of the whole map, and it is the
// same number of bytes: 200 rows of 320. The rows of the window are not
// contiguous in the world (the world is map_width wide, the window 320), so
// it has to be a row at a time.
//
// source is rebuilt from scratch on every row rather than advanced by
// map_width, because it is a huge pointer: recomputing it from the base
// makes Turbo C normalize it, and a normalized pointer has an offset of at
// most 15, so the 320 byte memcpy that follows can never run off the end of
// its segment.
//===========================================================
void bmp_draw_world_window(unsigned char *destination){

	int row;
	unsigned char huge *source;
	unsigned int destination_offset;

	if (buffer_original_background_bmp == NULL){
		return;
	}

	destination_offset = 0;

	for (row = 0; row < HEIGHT; row++){

		source = buffer_original_background_bmp
		       + ((unsigned long)(camera_y + row) * (unsigned long)map_width)
		       + (unsigned long)camera_x;

		memcpy(destination + destination_offset, source, WIDTH);

		destination_offset = destination_offset + WIDTH;

	}

}


//===========================================================
// Reserves everything, for a map of width x height pixels.
//
// Sizes, for the two maps that exist today:
//
//                        320x200 map      640x400 map
//   map picture             64000           256000
//   collision mask           8000            32000
//   screen buffer           64000            64000
//   sprite sheet            64000            64000   (freed after the init)
//
// The map picture is the only thing here that can go over 64 KB, so it is
// the only one that needs farmalloc and a huge pointer. malloc cannot even
// be asked for 256000: its argument is a 16 bit size_t and stops at 65535.
//===========================================================
void bmp_init_buffers(int width, int height){

	unsigned long world_size;
	unsigned long mask_size;

	map_width  = width;
	map_height = height;
	camera_x   = 0;
	camera_y   = 0;

	world_size = (unsigned long)width * (unsigned long)height;
	mask_size  = (world_size + 7L) / 8L;

	// The map picture
	buffer_original_background_bmp = (unsigned char huge *)farmalloc(world_size);
	if(buffer_original_background_bmp == NULL){
		printf("Error creating buffer_original_background_bmp (%lu bytes)\n", world_size);
    }

    // The frame being built. Always one screen, never more.
    buffer_background_image_data = (unsigned char*)malloc(SCREEN_SIZE);
	if(buffer_background_image_data == NULL){
		printf("Error creating buffer_background_image_data\n");	
    }
    
    //Create buffer pallete data
    buffer_palleta_data = (unsigned char*)malloc(PALLETA_DATA_SIZE);
	if(buffer_palleta_data == NULL){
		printf("Error creating buffer_palleta_data\n");	
    }
    
    // The collision mask, one bit per pixel of the whole world
    buffer_collision_mask = (unsigned char*)malloc((unsigned int)mask_size);
	if(buffer_collision_mask == NULL){
		printf("Error creating buffer_collision_mask (%lu bytes)\n", mask_size);
    }else{
    	memset(buffer_collision_mask, 0, (unsigned int)mask_size);
    }
    
}


//===========================================================
// Loads the map picture into buffer_original_background_bmp.
//
// The rows are read straight into their final place instead of being read in
// order and flipped afterwards. BMP stores the bottom row first, so the loop
// counts rows DOWN while the file is read forwards.
//===========================================================
void bmp_fill_background_in_main_buffer(char *_file)
{
	FILE *file;
	int row;
	int padding;
	unsigned char huge *destination;

	if (buffer_original_background_bmp == NULL){
		printf("ERROR!!! bmp_init_buffers() has not been called\n");
		return;
	}

	file = fopen(_file,"rb"); //binario
	if(file == NULL ){
		printf("ERROR!!! I can not open the backound image file: %s\n", _file);
		// Do not touch fseek/fread with a NULL FILE pointer below: that
		// would read/write invalid memory and hang or crash the game.
		return;
	}

	// BMP pads every row up to a multiple of 4 bytes. 320 and 640 are both
	// multiples of 4 so this comes out 0 for the maps we have, but a map of,
	// say, 500 pixels wide would read crooked without it.
	padding = (4 - (map_width % 4)) % 4;

	fseek(file, 1078L, SEEK_SET);

	for (row = map_height - 1; row >= 0; row = row - 1){

		if (fread(map_line, 1, map_width, file) != (size_t)map_width){
			break;
		}

		if (padding > 0){
			fseek(file, (long)padding, SEEK_CUR);
		}

		destination = buffer_original_background_bmp
		            + ((unsigned long)row * (unsigned long)map_width);

		memcpy(destination, map_line, map_width);

	}

	fclose(file);
}


//===========================================================
// Loads the collision bitmap and packs it into buffer_collision_mask, one
// bit per pixel: 1 where the file has MAP_WALL_COLOR, 0 everywhere else.
//
// Reading it a row at a time and packing as it goes means a 640x400
// collision map never needs its 256000 bytes to exist anywhere, not even for
// a moment. Only the 32000 packed bytes are ever reserved.
//===========================================================
void bmp_fill_background_collision_in_buffer(char *_file)
{
	FILE *file;
	int row;
	int column;
	int padding;
	unsigned long bit_index;
	unsigned int  byte_index;

	if (buffer_collision_mask == NULL){
		printf("ERROR!!! bmp_init_buffers() has not been called\n");
		return;
	}

	file = fopen(_file,"rb"); //binario
	if(file == NULL ){
		printf("ERROR!!! I can not open the collision file: %s\n", _file);
		// Do not touch fseek/fread with a NULL FILE pointer below: that
		// would read/write invalid memory and hang or crash the game.
		return;
	}

	padding = (4 - (map_width % 4)) % 4;

	fseek(file, 1078L, SEEK_SET);

	for (row = map_height - 1; row >= 0; row = row - 1){

		if (fread(map_line, 1, map_width, file) != (size_t)map_width){
			break;
		}

		if (padding > 0){
			fseek(file, (long)padding, SEEK_CUR);
		}

		for (column = 0; column < map_width; column++){

			if (map_line[column] == MAP_WALL_COLOR){

				bit_index  = ((unsigned long)row * (unsigned long)map_width) + (unsigned long)column;
				byte_index = (unsigned int)(bit_index >> 3);

				buffer_collision_mask[byte_index] =
					buffer_collision_mask[byte_index] | (unsigned char)(1 << (unsigned int)(bit_index & 7L));

			}

		}

	}

	fclose(file);
}


void bmp_extract_pallete_from_file(char *_file){
	file_background_image_game = fopen(_file,"rb"); //binario
	if(file_background_image_game == NULL ){
		printf("ERROR!!! I can not open the backound image file\n");
		// Do not call bmp_load_pallete_data() below with a NULL FILE
		// pointer: that would read invalid memory and hang or crash the game.
		return;
	}

	//First step is load the PALLETE_DATA of image
	bmp_load_pallete_data(buffer_palleta_data , file_background_image_game);

}


//===========================================================
// Opens sprites.bmp and LEAVES IT OPEN. Nothing is read yet.
//
// The sheet used to be loaded whole into a 64000 byte buffer, and every
// sprite was cut out of that. It does not live in memory any more, and the
// reason is arithmetic: those 64000 bytes were alive at the same time as the
// 256000 byte map, and when they were finally handed back they left a 64000
// byte hole in the middle of the heap. Total free memory was never the
// problem. CONTIGUOUS free memory was: a 42090 byte WAV would not fit in
// what was left, and asking for the map after the hole existed failed too,
// because you cannot put a 256000 byte block in a 64000 byte gap.
//
// So the sprites are read straight out of the file instead, one row at a
// time. It happens 28 times at startup and never again, and it costs 64000
// bytes of nothing.
//===========================================================
void bmp_open_sprite_sheet(char *_file_sprites_game){

	file_sprites_game_open = fopen(_file_sprites_game,"rb");

	if(file_sprites_game_open == NULL ){
		printf("ERROR!!! I can not open %s\n", _file_sprites_game);
	}

}

//===========================================================
// Closes it, once every sprite has been cut out.
//===========================================================
void bmp_close_sprite_sheet(){

	if (file_sprites_game_open != NULL){
		fclose(file_sprites_game_open);
		file_sprites_game_open = NULL;
	}

}

//===========================================================
// Cuts a sprite_width x sprite_height rectangle out of sprites.bmp, starting
// at (src_x, src_y) counted from the TOP LEFT of the picture, and packs it
// into sprite_dest with sprite_width as its row stride.
//
// Read straight from the file, a row at a time, because the sheet is not in
// memory. See bmp_open_sprite_sheet() for why.
//
// BMP stores its rows bottom-up, so row Y of the picture is row
// (HEIGHT-1-Y) of the file. That is the same flip bmp_revert_bmp() used to
// do to the whole buffer, done here as one subtraction instead.
//
// The sheet is 320x200 and always will be. That has nothing to do with the
// size of the map.
//===========================================================
void bmp_extract_sprite(unsigned int src_x,
	                        unsigned int src_y,
	                        unsigned int sprite_width,
	                        unsigned int sprite_height,
	                        unsigned char *sprite_dest)
{

	unsigned int y;
	long file_offset;

	if (file_sprites_game_open == NULL){
		return;
	}

	for (y = 0; y < sprite_height; y++){

		file_offset = 1078L
		            + ((long)(HEIGHT - 1 - (src_y + y)) * (long)WIDTH)
		            + (long)src_x;

		fseek(file_sprites_game_open, file_offset, SEEK_SET);
		fread(sprite_dest + (y * sprite_width), 1, sprite_width, file_sprites_game_open);

	}

}


//===========================================================
// Paints a sprite into a screen buffer, clipped to the screen.
//
// dest_x and dest_y are SCREEN coordinates and they are SIGNED. They arrive
// as "world position minus camera", so a tank halfway off the left edge
// comes in here as -9, and a bullet flying in from the room next door spends
// several frames partly outside the screen. Back when these were unsigned,
// that -9 was 65527 and the write went a long way outside the buffer, which
// in DOS is either silent corruption or a hang.
//
// The rows and columns that fall outside are worked out once, before the
// loops, instead of testing every pixel: the clipped case is not rare here,
// it happens every time anybody goes near an edge.
//===========================================================
void draw_sprite_to_buffer(unsigned char *sprite,      
			                         unsigned int sprite_width,   
			                         unsigned int sprite_height,  
			                         int dest_x,         
			                         int dest_y,        
			                         unsigned char *dest_buffer)
{
    int y, x;
    int start_x, start_y;
    int end_x, end_y;
    unsigned int src_offset, dest_offset;
    unsigned char pixel;

    // Completely off the screen: nothing to do at all
    if (dest_x >= WIDTH){
    	return;
    }
    if (dest_y >= HEIGHT){
    	return;
    }
    if (dest_x + (int)sprite_width <= 0){
    	return;
    }
    if (dest_y + (int)sprite_height <= 0){
    	return;
    }

    // Which part of the sprite actually lands on the screen
    start_x = 0;
    if (dest_x < 0){
    	start_x = -dest_x;
    }

    start_y = 0;
    if (dest_y < 0){
    	start_y = -dest_y;
    }

    end_x = (int)sprite_width;
    if (dest_x + end_x > WIDTH){
    	end_x = WIDTH - dest_x;
    }

    end_y = (int)sprite_height;
    if (dest_y + end_y > HEIGHT){
    	end_y = HEIGHT - dest_y;
    }

    for(y = start_y; y < end_y; y++) {
        for(x = start_x; x < end_x; x++) {
        
            // Posicion en el sprite (sprite_width de ancho)
            src_offset = ((unsigned int)y * sprite_width) + (unsigned int)x;
            
            // Posicion en el buffer destino (320 de ancho)
            dest_offset = ((unsigned int)(dest_y + y) * WIDTH) + (unsigned int)(dest_x + x);
            
            pixel = sprite[src_offset];
            
            // Copy the pixel (with transparency: color 0 = transparent)
            if(pixel != 0) {
                dest_buffer[dest_offset] = pixel;
            }
        }
    }
}




void bmp_close_files(){

	// Only close files that were actually opened. If a bmp file failed
	// to open earlier, its FILE pointer is still NULL here, and calling
	// fclose() on a NULL pointer is invalid.
	if (file_background_image_game != NULL){
		fclose(file_background_image_game);
		file_background_image_game = NULL;
	}

	if (file_sprites_game != NULL){
		fclose(file_sprites_game);
		file_sprites_game = NULL;
	}

}

void bmp_delete_buffers(){
	
	if (buffer_original_background_bmp != NULL){
		farfree(buffer_original_background_bmp);
		buffer_original_background_bmp = NULL;
	}

	if (buffer_background_image_data != NULL){
		free(buffer_background_image_data);
		buffer_background_image_data = NULL;
	}

	if (buffer_palleta_data != NULL){
		free(buffer_palleta_data);
		buffer_palleta_data = NULL;
	}

	if (buffer_collision_mask != NULL){
		free(buffer_collision_mask);
		buffer_collision_mask = NULL;
	}
	
}
