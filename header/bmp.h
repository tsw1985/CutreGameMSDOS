/* HEADER bmphanler */
#ifndef BMP_HANDLER
#define BMP_HANDLER

#define WIDTH 320
#define HEIGHT 200

#define IMAGE_DATA_SIZE 64000 //64456
#define PALLETA_DATA_SIZE 309

#include <stdio.h>
#include "header\players.h"

// Global variables with EXTERN
extern unsigned char *vga;
extern unsigned char *buffer_original_background_bmp;
extern unsigned char *buffer_background_image_data;
extern unsigned char *buffer_palleta_data;
extern unsigned char *buffer_sprites_data;
extern FILE *file_background_image_game;
extern FILE *file_sprites_game;


//LOAD BACKGROUND
void bmp_init_buffers();
void bmp_delete_buffers();
void bmp_close_files();
void bmp_fill_background_in_main_buffer(char *file);
void bmp_revert_bmp(char *bmp_data);
void bmp_load_pallete_data(char *buffer_data_dest, FILE *file);
void bmp_write_pallete_data_into_dac(char *pallete_data);
void bmp_fill_buffer_with_image_data_from_file(char *buffer_data_dest, FILE *file);
void bmp_paint_image_data_to_vga(char *buffer_image_data);
void bmp_extract_pallete_from_file(char *_file);


//LOAD SPRITES
void bmp_fill_sprites_in_buffer(char *file);

void bmp_extract_sprite(unsigned char *sprite_sheet,  
                                 unsigned int src_x, 
                                 unsigned int src_y,
                                 unsigned int sprite_width,  
                                 unsigned int sprite_height,
                                 unsigned char *sprite_dest);
                                 
void draw_sprite_to_buffer(unsigned char *sprite,      
			                         unsigned int sprite_width,   
			                         unsigned int sprite_height,  
			                         unsigned int dest_x,         
			                         unsigned int dest_y,        
			                         unsigned char *dest_buffer);
                                 
                                 
                                 
                                 

#endif