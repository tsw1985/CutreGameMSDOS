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
//
//
// THE WORLD AND THE SCREEN ARE NOT THE SAME THING ANY MORE
// -------------------------------------------------------
// The screen is always 320x200: that is what mode 13h gives us and it never
// changes. The WORLD can be bigger. With /bigmap it is 640x400, four screens
// worth of map, and what you see is a 320x200 window into it that follows
// your tank around.
//
// Two coordinate systems, and mixing them up is the one mistake that will
// cost you an afternoon:
//
//   WORLD  - where things really are. Everything in the game lives here:
//            tank positions, bullets, the collision map. 0..map_width-1.
//   SCREEN - where a thing gets painted this frame. Only the drawing code
//            uses it, and it is always world minus camera. 0..319.
//
// The rule that keeps the network game working: THE CAMERA IS NOT PART OF
// THE GAME. Each machine follows its own tank, so camera_x is different on
// the two machines and that is correct. It must never end up in the
// checksum and no game decision may ever read it. If a value is worked out
// by bmp_camera_follow(), it is decoration.
//
// That is also why a bullet fired in a part of the map you cannot see still
// arrives: both machines simulate the whole world, the bullet has a world
// position all along, and it becomes visible the moment that position falls
// inside your window. Nothing special is done for it.
//===========================================================

// VGA mode 13h: 320x200, 256 colors, one byte per pixel. This is the SCREEN,
// and it is fixed for ever. For the size of the map use map_width/map_height.
#define WIDTH 						320
#define HEIGHT 						200

// 320 * 200 = one full screen of pixels, and the size of the screen buffer
#define IMAGE_DATA_SIZE 		64000
#define SCREEN_SIZE 				64000

// The biggest world this build can load. Only used to size the one-row
// scratch buffer the loader reads into, so raising it costs 
// (new value - 640) bytes and nothing else.
#define MAP_MAX_WIDTH 			640

// Palette index that marks a wall in the collision bitmap. Both cutrecol.bmp
// and bigcol.bmp use just 2 colors: 3 (floor) and 252 (wall). Only this one
// is looked at, so anything that is not exactly 252 is floor.
#define MAP_WALL_COLOR 			252

// 256 colors * 3 bytes (R,G,B) = 768 in the file, but only this much is
// loaded and written into the VGA DAC
#define PALLETA_DATA_SIZE 	309

// How close to the edge of the screen the tank may get before the camera
// starts pushing. Inside this margin the camera does not move AT ALL, which
// is the whole point: in normal play the picture is still, and it only
// scrolls when you are really heading somewhere.
//
// Both have to stay below half of their screen size minus the tank, or the
// two edges of the dead zone would cross over and the camera would fight
// itself: X below 151, Y below 91.
#define CAMERA_DEAD_ZONE_X 	100
#define CAMERA_DEAD_ZONE_Y 	 70

#include <stdio.h>
#include "header\players.h"

// Global variables with EXTERN

// ---- The world ----
// Size of the loaded map in pixels. 320x200 in the normal game, so the
// window below covers it exactly and the camera can never move: everything
// behaves like it always did. 640x400 with /bigmap.
extern int map_width;
extern int map_height;

// Top-left corner of the visible window, in world pixels. Always 0,0 when
// the map is one screen big. LOCAL to this machine, see the note above.
extern int camera_x;
extern int camera_y;

extern unsigned char *vga;							// A000:0000, the real VGA memory

// The whole map, at map_width x map_height. This one can be bigger than
// 64 KB (256000 bytes for 640x400), which is why it is farmalloc'd and
// declared huge: a plain far pointer wraps round at the end of its segment
// instead of carrying into the next one.
extern unsigned char huge *buffer_original_background_bmp;

extern unsigned char *buffer_background_image_data;		// the frame being built, 320x200, what gets sent to the VGA

// The collision map of the WHOLE world, ONE BIT PER PIXEL.
//
// It has to cover everything, not just what you can see: both machines
// simulate both tanks, so this machine has to answer "did the other tank hit
// a wall" about a part of the map it is not even showing. If that answer
// differed between the two machines the game would desync on the spot.
//
// One bit is enough because the only question ever asked is wall or not, and
// it is what makes the whole thing fit: 640x400 packed is 32000 bytes, half
// of what the old one-screen-of-bytes map used to cost.
extern unsigned char *buffer_collision_mask;

extern unsigned char *buffer_palleta_data;				// the 256 colors, on their way to the DAC
extern unsigned char *buffer_sprites_data;				// the whole sprites.bmp sheet, sprites are cut out of here
extern FILE *file_background_image_game;
extern FILE *file_sprites_game;


//LOAD BACKGROUND

// Reserves everything, sized for a map of width x height. Call it before any
// other bmp function: it is what sets map_width and map_height.
void bmp_init_buffers(int width, int height);

void bmp_delete_buffers();
void bmp_close_files();

// Loads the map picture into buffer_original_background_bmp. The file has to
// be map_width x map_height, 8 bits per pixel, uncompressed.
void bmp_fill_background_in_main_buffer(char *file);

// Loads the collision bitmap and packs it into buffer_collision_mask. Same
// size rules as above.
void bmp_fill_background_collision_in_buffer(char *_file);

void bmp_revert_bmp(char *bmp_data);				// flips the rows of a 320x200 buffer: BMP stores them bottom-up
void bmp_load_pallete_data(char *buffer_data_dest, FILE *file);
void bmp_write_pallete_data_into_dac(char *pallete_data);	// sends the colors to the VGA DAC
void bmp_fill_buffer_with_image_data_from_file(char *buffer_data_dest, FILE *file);
void bmp_paint_image_data_to_vga(char *buffer_image_data);	// dumps a whole screen buffer to the screen
void bmp_extract_pallete_from_file(char *_file);

// Hands back the 64000 bytes of the sprite sheet. Every sprite has been cut
// out of it by then, and it is never read again, so holding on to it is
// 64000 bytes of nothing. That is most of what the bigger map costs.
void bmp_free_sprite_sheet();


// ---- The camera ----

// Moves the window if, and only if, the target has left the dead zone, and
// then by exactly as much as it has left it by: if the tank walks 2 pixels,
// the camera walks 2 pixels. No jump, no lag, and it stops the instant the
// tank stops. Always clamped so the window stays inside the map.
void bmp_camera_follow(int target_x, int target_y, int target_width, int target_height);

// Puts the target in the middle of the screen right now, with no easing.
// For the start of a round, where there is nothing to follow smoothly from.
void bmp_camera_snap(int target_x, int target_y, int target_width, int target_height);

// Copies the visible 320x200 window out of the world and into a screen
// buffer. This is the line that used to be a single memcpy of the whole map.
void bmp_draw_world_window(unsigned char *destination);


// Reading one pixel. Three different sources, and picking the right one
// matters:
//   vga   -> what is on screen right now, tanks included. SCREEN coordinates.
//   map   -> the clean map picture, no tanks on top. WORLD coordinates.
//   wall  -> the collision mask, which is the ONLY one the collision checks
//            should use: it never has anything drawn over it, and it covers
//            the whole world. WORLD coordinates.
unsigned char bmp_get_vga_pixel(unsigned int x, unsigned int y);
unsigned char bmp_get_map_pixel(int x, int y);

// 1 if (x,y) is wall, 0 if it is floor. Anything outside the map counts as
// wall: the maps are drawn with a solid border so it should never come up,
// but if one ever has a hole in it the tank stops at the edge instead of
// walking off into memory that is not ours.
int bmp_is_wall(int x, int y);


//LOAD SPRITES
void bmp_fill_sprites_in_buffer(char *file);

// Cuts a sprite_width x sprite_height rectangle out of the sheet, starting
// at (src_x, src_y), and packs it into sprite_dest with sprite_width as its
// row stride. Not every sprite is tank sized: the explosion is 13x13.
//
// The sheet is 320 wide and always will be, so this one is not affected by
// the size of the map.
void bmp_extract_sprite(unsigned char *sprite_sheet,
                                 unsigned int src_x,
                                 unsigned int src_y,
                                 unsigned int sprite_width,
                                 unsigned int sprite_height,
                                 unsigned char *sprite_dest);

// Paints a sprite into a screen buffer at (dest_x, dest_y). Color 0 is
// transparent, so the map shows through the corners of the sprite box.
//
// dest_x and dest_y are SCREEN coordinates and they are SIGNED, which is not
// a detail: they come from "world position minus camera", so a tank halfway
// off the left edge arrives here as -9. When they were unsigned that -9 was
// 65527 and the write landed a long way outside the buffer. Anything that
// falls outside 0..319 / 0..199 is clipped away, per pixel.
void draw_sprite_to_buffer(unsigned char *sprite,
			                         unsigned int sprite_width,
			                         unsigned int sprite_height,
			                         int dest_x,
			                         int dest_y,
			                         unsigned char *dest_buffer);





#endif
