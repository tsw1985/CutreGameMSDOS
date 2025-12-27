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
	load_back_ground_game("c:\\cutre.bmp");
	printf("Cutre GAME MSDOS VERSION\n");
	
	
	//hola();
	//hello_bmp();
	//carga_total("c:\\ibiza.bmp");

    //hello_util();
    
	/*for(i= 0; i < 3 ; i++) {
    	printf("Cargando mapa 320x200\n");
	}*/
	
	getch();
	return 0;
}

