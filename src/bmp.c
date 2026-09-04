/* BMP HAndler */
#include "header\bmp.h"
#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <alloc.h>

//FILES
FILE *file_background_image_game = NULL;
FILE *file_sprites_game = NULL;


// INIT Global variables with EXTERN in bmp.h
unsigned char *vga = (unsigned char *) MK_FP(0xA000,0);
unsigned char *buffer_original_background_bmp = NULL; // Buffer with original IMAGE
unsigned char *buffer_background_image_data = NULL;
unsigned char *buffer_palleta_data = NULL;
unsigned char *buffer_sprites_data = NULL;
unsigned char *buffer_map_collisions_data = NULL;


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
		//fread(buffer_data_dest,SCREEN_SIZE,1,file);
		fread(buffer_data_dest,SCREEN_SIZE,1,file);
}

void bmp_paint_image_data_to_vga(char *buffer_image_data){
	memcpy(vga,buffer_image_data,SCREEN_SIZE);
}

//===========================================================
// Read the color/pallete index that is currently at pixel (x,y) in the
// real VGA video memory (segment 0xA000, the "vga" pointer).
//
// WARNING: by the time the main loop reads this, the tank sprite has
// already been drawn on top of the map for the current frame. So a
// coordinate that falls on the tank itself (like the cannon tip) will
// return the tank sprite's own color, not the map color underneath it.
// Do NOT use this for collision checks. Use bmp_get_map_pixel() instead,
// which reads the clean map without the tank drawn on it.
//===========================================================
unsigned char bmp_get_vga_pixel(unsigned int x, unsigned int y){

	unsigned int offset;

	offset = (y * WIDTH) + x;

	return vga[offset];

}

//===========================================================
// Read the color/pallete index at pixel (x,y) of the original map,
// from buffer_original_background_bmp. Unlike bmp_get_vga_pixel(), this
// buffer never has the tank (or anything else) drawn on top of it, so
// this is the correct one to use for collision detection.
//===========================================================
unsigned char bmp_get_map_pixel(unsigned int x, unsigned int y){

	unsigned int offset;

	offset = (y * WIDTH) + x;

	return buffer_original_background_bmp[offset];

}

//===========================================================
// Read the color/pallete index at pixel (x,y) of the dedicated collision
// map, from buffer_map_collisions_data (filled by
// bmp_fill_background_collision_in_buffer()). This is the buffer that
// collision checks should actually use: it is never touched by the tank
// sprite or anything else drawn on screen, so it always holds the raw
// map colors.
//===========================================================
unsigned char bmp_get_collision_pixel(unsigned int x, unsigned int y){

	unsigned int offset;

	offset = (y * WIDTH) + x;

	return buffer_map_collisions_data[offset];

}

void bmp_init_buffers(){
	
		
	//Create buffer bmp
	buffer_original_background_bmp = (char*)malloc(SCREEN_SIZE);
	if(buffer_original_background_bmp == NULL){
		printf("Error creating buffer_original_background_bmp\n");	
    }
    
    //Create buffer image data
    buffer_background_image_data = (char*)malloc(SCREEN_SIZE);
	if(buffer_background_image_data == NULL){
		printf("Error creating buffer_background_image_data\n");	
    }
    
    //Create buffer pallete data
    buffer_palleta_data = (char*)malloc(PALLETA_DATA_SIZE);
	if(buffer_palleta_data == NULL){
		printf("Error creating buffer_palleta_data\n");	
    }
    
    //Create buffer sprites data
    buffer_sprites_data = (char*)malloc(SCREEN_SIZE);
	if(buffer_sprites_data == NULL){
		printf("Error creating buffer_sprites_data\n");	
    }
    
    
    buffer_map_collisions_data = (char*)malloc(SCREEN_SIZE);
	if(buffer_map_collisions_data == NULL){
		printf("Error creating buffer_buffer_map_collisions_data\n");	
    }
    
    
    
    // Fill with 0 all buffers
	
}

//===========================================================
// Save original file in: buffer_original_background_bmp and fill pallete data
// in his buffer
//===========================================================
void bmp_fill_background_in_main_buffer(char *_file)
{
	
	file_background_image_game = fopen(_file,"rb"); //binario
	if(file_background_image_game == NULL ){
		printf("ERROR!!! I can not open the backound image file\n");
		// Do not touch fseek/fread with a NULL FILE pointer below: that
		// would read/write invalid memory and hang or crash the game.
		return;
	}

	//Fill ALL file data into buffer_bmp ( not reverted )
	fseek(file_background_image_game, 1078L, SEEK_SET);
    fread(buffer_original_background_bmp,SCREEN_SIZE,1,file_background_image_game);
    bmp_revert_bmp(buffer_original_background_bmp);
}


void bmp_fill_background_collision_in_buffer(char *_file)
{
	
	file_background_image_game = fopen(_file,"rb"); //binario
	if(file_background_image_game == NULL ){
		printf("ERROR!!! I can not open the backound image file\n");
		// Do not touch fseek/fread with a NULL FILE pointer below: that
		// would read/write invalid memory and hang or crash the game.
		return;
	}

	//Fill ALL file data into buffer_bmp ( not reverted )
	fseek(file_background_image_game, 1078L, SEEK_SET);
    fread(buffer_map_collisions_data,SCREEN_SIZE,1,file_background_image_game);
    bmp_revert_bmp(buffer_map_collisions_data);
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
	//fclose(file_background_image_game);

}


void bmp_fill_sprites_in_buffer(char *_file_sprites_game){

	file_sprites_game	= fopen(_file_sprites_game,"rb");
	if(file_sprites_game == NULL ){
		printf("ERROR!!! I can not open file_sprites_game file\n");
		// Do not call bmp_fill_buffer_with_image_data_from_file() below with
		// a NULL FILE pointer: that would read invalid memory and hang or
		// crash the game.
		return;
	}

	// Load the sprites data . Full sprites image
	bmp_fill_buffer_with_image_data_from_file(buffer_sprites_data , file_sprites_game);

}

void bmp_extract_sprite(unsigned char *sprite_sheet,  
	                             unsigned int src_x, 
	                          	 unsigned int src_y,
	                          	 unsigned int sprite_width,  
	                          	 unsigned int sprite_height,
	                          	 unsigned char *sprite_dest)
{
	
	
    unsigned int y, x;
    unsigned int src_offset, dest_offset;
    
    for(y = 0; y < sprite_height; y++) {
        for(x = 0; x < sprite_width; x++) {
    
            src_offset = ((src_y + y) * 320) + (src_x + x);
            
                dest_offset = (y * sprite_width) + x;
            
                sprite_dest[dest_offset] = sprite_sheet[src_offset];
        }
    }
    
}


void draw_sprite_to_buffer(unsigned char *sprite,      
			                         unsigned int sprite_width,   
			                         unsigned int sprite_height,  
			                         unsigned int dest_x,         
			                         unsigned int dest_y,        
			                         unsigned char *dest_buffer)
{
    unsigned int y, x;
    unsigned int src_offset, dest_offset;
    unsigned char pixel;
    
    for(y = 0; y < sprite_height; y++) {
        for(x = 0; x < sprite_width; x++) {
        
            // Posicion en el sprite (sprite_width de ancho)
            src_offset = (y * sprite_width) + x;
            
            // Posicion en el buffer destino (320 de ancho)
            dest_offset = ((dest_y + y) * 320) + (dest_x + x);
            
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
	}

	if (file_sprites_game != NULL){
		fclose(file_sprites_game);
	}

}

void bmp_delete_buffers(){
	
	free(buffer_original_background_bmp);
    free(buffer_background_image_data);
    free(buffer_palleta_data);
    free(buffer_sprites_data);
    free(buffer_map_collisions_data);
	
}