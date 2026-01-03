#include <stdio.h>
#include <conio.h>
#include "header\util.h"
#include "header\bmp.h"
#include "header\gameloop.h"

// asm function
extern void hola();
extern set_vga_320_200_mode();


/* 

	GAME FOLDER IN BACK UP : CutreGameMSDOS

*/


int main(){

	set_vga_320_200_mode();
	
	//Load original background
	bmp_load_background_game("c:\\cutre.bmp");
	bmp_load_sprites_images("c:\\sprites.bmp");
	
	//init_game_loop();
	
	
	getch();
	bmp_delete_buffers();
	bmp_close_files();
	
	return 0;
}

