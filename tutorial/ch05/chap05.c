//===========================================================
// CAPITULO 5 - El teclado, de verdad
//
// Mover un sprite parece trivial hasta que intentas leer el teclado. Las
// funciones normales de C no valen para un juego, y este capitulo explica
// por que y que se hace en su lugar.
//
// Lo que se aprende aqui:
//
//   1. Por que getch() y kbhit() no sirven.
//   2. Que es un scancode y en que se diferencia de un caracter.
//   3. Como se instala un manejador de interrupcion propio (INT 9).
//   4. Como se consigue leer VARIAS teclas a la vez.
//
// Este es el unico capitulo que reescribe codigo del juego: el manejador de
// teclado vive dentro de src/main.c y no se puede enlazar. Pero es que
// ademas ES el tema del capitulo, asi que aqui toca verlo entero.
//===========================================================

#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <alloc.h>

#include "header\bmp.h"
#include "header\players.h"
#include "..\tutlib.h"



int main(){

	unsigned char *tank_up;
	unsigned char *tank_down;
	unsigned char *tank_left;
	unsigned char *tank_right;
	unsigned char *sprite_now;

	int tank_x;
	int tank_y;

	printf("\n");
	printf("CAPITULO 5 - El teclado\n");
	printf("\n");
	printf("Flechas para mover. ESC para salir.\n");
	printf("\n");
	printf("Prueba a mantener DOS flechas a la vez: veras que el tanque solo\n");
	printf("hace caso a una. Eso no es el teclado, es una decision del juego,\n");
	printf("y esta explicada en el doc.\n");
	printf("\n");
	printf("Pulsa una tecla para empezar.\n");
	getch();

	bmp_init_buffers(WIDTH, HEIGHT);
	bmp_fill_background_in_main_buffer("..\\..\\res\\cutre.bmp");
	bmp_extract_pallete_from_file("..\\..\\res\\cutre.bmp");

	tank_up    = (unsigned char*)malloc(TANK_WIDTH * TANK_HEIGHT);
	tank_down  = (unsigned char*)malloc(TANK_WIDTH * TANK_HEIGHT);
	tank_left  = (unsigned char*)malloc(TANK_WIDTH * TANK_HEIGHT);
	tank_right = (unsigned char*)malloc(TANK_WIDTH * TANK_HEIGHT);

	bmp_open_sprite_sheet("..\\..\\res\\sprites.bmp");
	bmp_extract_sprite(  2,  5, TANK_WIDTH, TANK_HEIGHT, tank_up);
	bmp_extract_sprite( 43, 10, TANK_WIDTH, TANK_HEIGHT, tank_down);
	bmp_extract_sprite( 83,  8, TANK_WIDTH, TANK_HEIGHT, tank_left);
	bmp_extract_sprite(124,  8, TANK_WIDTH, TANK_HEIGHT, tank_right);
	bmp_close_sprite_sheet();

	tank_x = 150;
	tank_y = 90;
	sprite_now = tank_up;

	install_kbd();
	set_video_mode(0x0013);
	bmp_write_pallete_data_into_dac(buffer_palleta_data);

	//-------------------------------------------------------
	// EL BUCLE PRINCIPAL. Este es el esqueleto de cualquier juego:
	//
	//   1. leer la entrada
	//   2. actualizar el estado
	//   3. dibujar
	//   4. esperar al monitor y volcar
	//
	// Aqui no hay ninguna espera a que pulses nada. El bucle gira sin parar
	// y en cada vuelta MIRA como esta el teclado. Por eso el tanque se mueve
	// mientras mantienes la flecha.
	//-------------------------------------------------------
	do {

		// 1 y 2: una sola direccion por vuelta.
		//
		// El if / else if es a proposito: si mantienes arriba y derecha, solo
		// entra la primera. Eso es lo que impide que el tanque vaya en
		// diagonal, y el juego real hace exactamente lo mismo.
		if (keys[KEY_UP]){
			tank_y = tank_y - 2;
			sprite_now = tank_up;
		}else if (keys[KEY_DOWN]){
			tank_y = tank_y + 2;
			sprite_now = tank_down;
		}else if (keys[KEY_LEFT]){
			tank_x = tank_x - 2;
			sprite_now = tank_left;
		}else if (keys[KEY_RIGHT]){
			tank_x = tank_x + 2;
			sprite_now = tank_right;
		}

		// Que no se escape de la pantalla. En el capitulo 7 esto lo hara el
		// mapa de colisiones, que es mucho mejor.
		if (tank_x < 0){ tank_x = 0; }
		if (tank_y < 0){ tank_y = 0; }
		if (tank_x > WIDTH  - TANK_WIDTH ){ tank_x = WIDTH  - TANK_WIDTH;  }
		if (tank_y > HEIGHT - TANK_HEIGHT){ tank_y = HEIGHT - TANK_HEIGHT; }

		// 3: dibujar
		bmp_draw_world_window(buffer_background_image_data);
		draw_sprite_to_buffer(sprite_now, TANK_WIDTH, TANK_HEIGHT,
		                      tank_x, tank_y, buffer_background_image_data);

		// 4: volcar
		wait_retrace();
		bmp_paint_image_data_to_vga(buffer_background_image_data);

	} while (!keys[KEY_ESC]);

	//-------------------------------------------------------
	// MUY IMPORTANTE: devolver el manejador original antes de salir.
	//
	// Si no lo haces, DOS se queda apuntando a una funcion que ya no existe
	// porque tu programa ha terminado. El siguiente tecleo cuelga la
	// maquina.
	//-------------------------------------------------------
	uninstall_kbd();
	set_video_mode(0x0003);

	bmp_close_files();
	bmp_delete_buffers();
	free(tank_up); free(tank_down); free(tank_left); free(tank_right);

	printf("\n");
	printf("Manejador de teclado devuelto. Si se te olvida eso, la maquina\n");
	printf("se cuelga en cuanto toques una tecla.\n");
	printf("\n");

	return 0;

}
