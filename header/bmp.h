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
void bmp_init_buffers();
void bmp_delete_buffers();
void bmp_close_files();
void bmp_load_background_game(char *file);
void bmp_revert_bmp(char *bmp_data);
void bmp_load_pallete_data(char *buffer_data_dest, FILE *file);
void bmp_write_pallete_data_into_dac(char *pallete_data);
void bmp_load_image_data_from_file(char *buffer_data_dest, FILE *file);
void bmp_paint_image_data_to_vga(char *buffer_image_data);


//LOAD SPRITES
void bmp_load_sprites_images(char *file);
void bmp_load_tank_1(char *sprites_buffer,struct player *player_1, int y, int x);
void bmp_load_tank_2(char *sprites_buffer,struct player *player_2, int y, int x);
void bmp_draw_tank_1(char *game_buffer , struct player *player_1);
void bmp_draw_tank_2(char *game_buffer , struct player *player_2);





#endif