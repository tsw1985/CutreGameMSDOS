/* HEADER bmphanler */
#ifndef BMP_HANDLER
#define BMP_HANDLER

//===========================================================
// Everything that touches a .bmp file or the screen.
//
// The BMPs are read by hand, without any library: 54 bytes of header, the
// palette at offset 54, and the pixels at offset 1078, one byte per pixel
// as an index into the palette. BMP rows are stored bottom-up, which is why
// bmp_revert_bmp() exists.
//===========================================================

// VGA mode 13h: 320x200, 256 colors, one byte per pixel
#define WIDTH 						320
#define HEIGHT 						200

// 320 * 200 = one full screen of pixels, and the size of every screen buffer
#define IMAGE_DATA_SIZE 		64000
#define SCREEN_SIZE 				64000

// 256 colors * 3 bytes (R,G,B) = 768 in the file, but only this much is
// loaded and written into the VGA DAC
#define PALLETA_DATA_SIZE 	309

#include <stdio.h>
#include "header\players.h"

// Global variables with EXTERN
extern unsigned char *vga;							// A000:0000, the real VGA memory
extern unsigned char *buffer_original_background_bmp;	// clean map, never drawn on: the copy to restore from
extern unsigned char *buffer_background_image_data;		// the frame being built, what gets sent to the VGA
extern unsigned char *buffer_palleta_data;				// the 256 colors, on their way to the DAC
extern unsigned char *buffer_sprites_data;				// the whole sprites.bmp sheet, sprites are cut out of here
extern unsigned char *buffer_map_collisions_data;		// cutrecol.bmp: the map that says where the walls are
extern FILE *file_background_image_game;
extern FILE *file_sprites_game;


//LOAD BACKGROUND
void bmp_init_buffers();							// reserves the 5 buffers above
void bmp_delete_buffers();
void bmp_close_files();
void bmp_fill_background_in_main_buffer(char *file);		// loads the map into buffer_original_background_bmp
void bmp_fill_background_collision_in_buffer(char *_file);	// loads cutrecol.bmp into buffer_map_collisions_data
void bmp_revert_bmp(char *bmp_data);				// flips the rows: BMP stores them bottom-up
void bmp_load_pallete_data(char *buffer_data_dest, FILE *file);
void bmp_write_pallete_data_into_dac(char *pallete_data);	// sends the colors to the VGA DAC
void bmp_fill_buffer_with_image_data_from_file(char *buffer_data_dest, FILE *file);
void bmp_paint_image_data_to_vga(char *buffer_image_data);	// dumps a whole buffer to the screen
void bmp_extract_pallete_from_file(char *_file);

// Reading one pixel. Three different sources, and picking the right one
// matters:
//   vga   -> what is on screen right now, tanks included
//   map   -> the clean map, no tanks on top
//   collision -> cutrecol.bmp, which is the ONLY one the collision checks
//                should use: it never has anything drawn over it
unsigned char bmp_get_vga_pixel(unsigned int x, unsigned int y);
unsigned char bmp_get_map_pixel(unsigned int x, unsigned int y);
unsigned char bmp_get_collision_pixel(unsigned int x, unsigned int y);


//LOAD SPRITES
void bmp_fill_sprites_in_buffer(char *file);

// Cuts a sprite_width x sprite_height rectangle out of the sheet, starting
// at (src_x, src_y), and packs it into sprite_dest with sprite_width as its
// row stride. Not every sprite is tank sized: the explosion is 13x13.
void bmp_extract_sprite(unsigned char *sprite_sheet,
                                 unsigned int src_x,
                                 unsigned int src_y,
                                 unsigned int sprite_width,
                                 unsigned int sprite_height,
                                 unsigned char *sprite_dest);

// Paints a sprite into a buffer at (dest_x, dest_y). Color 0 is
// transparent, so the map shows through the corners of the sprite box.
void draw_sprite_to_buffer(unsigned char *sprite,
			                         unsigned int sprite_width,
			                         unsigned int sprite_height,
			                         unsigned int dest_x,
			                         unsigned int dest_y,
			                         unsigned char *dest_buffer);





#endif
