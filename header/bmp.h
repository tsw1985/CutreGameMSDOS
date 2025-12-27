/* HEADER bmphanler */
#ifndef BMP_HANDLER
#define BMP_HANDLER

#define WIDTH 320
#define HEIGHT 200

#include <stdio.h>

void init_buffers();
void load_background_game(char *fichero);
void revert_bmp(char *bmp_data);
void load_pallete_data(char *buffer_data_dest, FILE *file);
void write_pallete_data_into_dac(char *pallete_data);
void load_image_data_from_file(char *buffer_data_dest, FILE *file);
void paint_image_data_to_vga(char *buffer_image_data);






#endif