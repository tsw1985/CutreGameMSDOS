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
int i = 0;
int a = 0;

int main(){

	//set_vga_320_200_mode();
	//load_background_game("c:\\cutre.bmp");
	
	init_game_loop();
	
	
	getch();
	return 0;
}

