#include <stdio.h>
#include <conio.h>
#include "header\util.h"
#include "header\bmp.h"

// asm function
extern void hola();
extern set_vga_320_200_mode();

int i = 0;
int a = 0;

int main(){

	set_vga_320_200_mode();
	load_background_game("c:\\cutre.bmp");
	
	getch();
	return 0;
}

