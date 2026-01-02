/* HEADER bmphanler */
#ifndef BMP_HANDLER
#define BMP_HANDLER

#define WIDTH 320
#define HEIGHT 200
#define IMAGE_DATA_SIZE 64456
#define PALLETA_DATA_SIZE 309

#include <stdio.h>
#include "header\players.h"

//LOAD BACKGROUND
void init_buffers();
void delete_buffers();
void close_files();
void load_background_game(char *file);
void revert_bmp(char *bmp_data);
void load_pallete_data(char *buffer_data_dest, FILE *file);
void write_pallete_data_into_dac(char *pallete_data);
void load_image_data_from_file(char *buffer_data_dest, FILE *file);
void paint_image_data_to_vga(char *buffer_image_data);


//LOAD SPRITES
void load_sprites_images(char *file);
void load_tank_1(char *sprites_buffer,struct player *player_1, int y, int x);
void load_tank_2(char *sprites_buffer,struct player *player_2, int y, int x);
void draw_tank_1(char *game_buffer , struct player *player_1);
void draw_tank_2(char *game_buffer , struct player *player_2);





#endif