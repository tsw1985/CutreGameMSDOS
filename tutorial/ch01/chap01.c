//===========================================================
// CAPITULO 1 - El modo 13h y la memoria de video
//
// Lo mas pequeno que se puede hacer: poner la tarjeta en modo grafico y
// escribir pixeles a mano, sin ficheros, sin sprites, sin nada.
//
// Lo que se aprende aqui:
//
//   1. Como se pide un modo grafico a la BIOS.
//   2. Que la pantalla NO es una funcion que dibuja: es MEMORIA. Escribes
//      un byte en la direccion correcta y aparece un pixel.
//   3. Que ese byte NO es un color: es un numero del 0 al 255 que apunta a
//      una tabla de colores llamada paleta.
//
// Del juego real se usa la variable global "vga", que vive en src/bmp.c y
// esta declarada en header/bmp.h. No se copia nada.
//===========================================================

#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include "header\bmp.h"


//===========================================================
// Pedir un modo de video a la BIOS.
//
// La interrupcion 0x10 es la de video. Con AH=0 (que queda dentro de
// AX=0x0013) se le pide "cambia a este modo", y el numero de modo va en AL.
//
//   0x13 = 320x200, 256 colores, un byte por pixel   <- el que usa el juego
//   0x03 = texto de 80x25, el modo normal de DOS
//
// El juego de verdad hace esto mismo desde ensamblador, en src/video.asm,
// porque asi estaba escrito desde el principio. En C es lo mismo y se lee
// mejor.
//===========================================================
static void set_video_mode(unsigned int mode){

	union REGS regs;

	regs.x.ax = mode;
	int86(0x10, &regs, &regs);

}


int main(){

	unsigned int x;
	unsigned int y;
	unsigned int offset;
	unsigned char color;

	printf("\n");
	printf("CAPITULO 1 - El modo 13h y la memoria de video\n");
	printf("\n");
	printf("Voy a poner la pantalla en 320x200 con 256 colores y a pintar\n");
	printf("los 256 colores de la paleta por defecto de la VGA.\n");
	printf("\n");
	printf("Pulsa una tecla para empezar, y otra para volver a DOS.\n");
	getch();

	set_video_mode(0x0013);

	//-------------------------------------------------------
	// A partir de aqui, "vga" apunta a la pantalla.
	//
	// Es un puntero a la direccion A000:0000, que es donde la tarjeta VGA
	// coloca su memoria en este modo. Escribir ahi es dibujar. No hay
	// ninguna llamada a ninguna funcion de dibujo: es memoria y ya esta.
	//
	// El pixel (x, y) esta en la posicion  y * 320 + x.
	//
	// Ese "* 320" tiene nombre: se llama el STRIDE o paso de fila, y es
	// cuantos bytes hay que avanzar para bajar una fila. Aqui coincide con
	// el ancho de la pantalla. En el capitulo del mapa grande dejara de
	// coincidir, y ahi es donde la gente se equivoca.
	//-------------------------------------------------------

	// La mitad de arriba: los 256 colores de la paleta, en una rejilla de
	// 16 x 16. Cada casilla mide 20 x 6 pixeles.
	//
	// Esto es lo que ensena que el byte no es un color. El byte 4 no es
	// "rojo": es "el color que haya en la posicion 4 de la paleta". Cambias
	// la paleta y el mismo byte pinta otra cosa.
	for (y = 0; y < 96; y++){

		for (x = 0; x < 320; x++){

			color = (unsigned char)(((y / 6) * 16) + (x / 20));

			offset = (y * WIDTH) + x;
			vga[offset] = color;

		}

	}

	// Una raya negra de separacion
	for (y = 96; y < 104; y++){
		for (x = 0; x < 320; x++){
			offset = (y * WIDTH) + x;
			vga[offset] = 0;
		}
	}

	// La mitad de abajo: los 64 grises que la VGA trae de serie en las
	// posiciones 16 a 31... y luego lo que haya. Sirve para ver que la
	// paleta por defecto no esta ordenada de ninguna forma util.
	for (y = 104; y < 200; y++){

		for (x = 0; x < 320; x++){

			color = (unsigned char)(x / 5);

			offset = (y * WIDTH) + x;
			vga[offset] = color;

		}

	}

	getch();

	set_video_mode(0x0003);

	printf("\n");
	printf("De vuelta en DOS.\n");
	printf("\n");
	printf("Lo que acabas de ver son 64000 bytes escritos a mano en\n");
	printf("A000:0000. Eso es todo lo que es una pantalla en modo 13h.\n");
	printf("\n");

	return 0;

}
