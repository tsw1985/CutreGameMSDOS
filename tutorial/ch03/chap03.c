//===========================================================
// CAPITULO 3 - Doble buffer y retrazo vertical
//
// En el capitulo 2 la imagen era fija, asi que daba igual como llegara a la
// pantalla. En cuanto algo se mueve, deja de dar igual.
//
// Lo que se aprende aqui:
//
//   1. Por que dibujar directamente en la pantalla PARPADEA.
//   2. Que es el doble buffer: construir el frame aparte y volcarlo entero.
//   3. Que es el retrazo vertical y por que hay que esperarlo.
//
// El programa hace lo mismo tres veces, cada vez mejor, para que veas la
// diferencia con tus propios ojos.
//===========================================================

#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include "header\bmp.h"

//-------------------------------------------------------
// Estas dos las copia el capitulo, y es la UNICA copia que hay en todo el
// curso. No es por gusto: en el juego viven dentro de src/main.c
// (set_text_mode en la linea 691, wait_retrace al final del fichero), y
// main.c tiene su propio main(), asi que no se puede enlazar contra el.
//
// Las dos son de graficos y su sitio natural seria src/bmp.c. Si algun dia
// se mueven ahi, este trozo desaparece de los capitulos.
//-------------------------------------------------------
static void set_video_mode(unsigned int mode){

	union REGS regs;

	regs.x.ax = mode;
	int86(0x10, &regs, &regs);

}

// Espera al retrazo vertical: el instante en que el monitor ha terminado de
// dibujar la pantalla y el haz vuelve arriba. El puerto 0x3DA lleva un bit
// (el 0x08) que dice si esta en ese momento.
static void wait_retrace(void){

	while (inp(0x3DA) & 0x08);      // esperar a que termine el retrazo actual
	while (!(inp(0x3DA) & 0x08));   // esperar a que empiece el siguiente

}

// Dibuja una barra vertical en la posicion x, sobre el buffer que se le diga
static void draw_bar(unsigned char *target, int x){

	int row;
	int column;
	unsigned int offset;

	for (row = 40; row < 160; row++){

		for (column = x; column < x + 24; column++){

			if (column >= 0 && column < WIDTH){
				offset = (unsigned int)(row * WIDTH) + (unsigned int)column;
				target[offset] = 15;
			}

		}

	}

}


int main(){

	int x;
	int pass;

	printf("\n");
	printf("CAPITULO 3 - Doble buffer y retrazo\n");
	printf("\n");
	printf("Una barra blanca cruzando la pantalla, tres veces:\n");
	printf("\n");
	printf("  1. Dibujando DIRECTAMENTE en la VGA        -> parpadea\n");
	printf("  2. Con doble buffer, sin esperar al monitor -> se parte\n");
	printf("  3. Con doble buffer y retrazo               -> limpio\n");
	printf("\n");
	printf("Mira bien la barra en cada pasada. Pulsa una tecla.\n");
	getch();

	bmp_init_buffers(WIDTH, HEIGHT);
	bmp_fill_background_in_main_buffer("..\\..\\res\\cutre.bmp");
	bmp_extract_pallete_from_file("..\\..\\res\\cutre.bmp");

	set_video_mode(0x0013);
	bmp_write_pallete_data_into_dac(buffer_palleta_data);

	for (pass = 1; pass <= 3; pass++){

		for (x = -30; x < WIDTH + 4; x = x + 2){

			if (pass == 1){

				//-------------------------------------------
				// PASADA 1: a pelo sobre la pantalla.
				//
				// Hay que borrar lo anterior antes de pintar lo nuevo, y el
				// monitor esta leyendo la pantalla MIENTRAS TANTO. Asi que
				// hay instantes en que lo viejo ya no esta y lo nuevo
				// todavia no. Eso es el parpadeo.
				//-------------------------------------------
				bmp_paint_image_data_to_vga(buffer_original_background_bmp);
				draw_bar(vga, x);

			}else{

				//-------------------------------------------
				// PASADAS 2 y 3: el frame se construye aparte, en memoria,
				// donde nadie lo esta mirando. Se puede tardar lo que haga
				// falta. Cuando esta terminado, se vuelca de una vez.
				//-------------------------------------------
				bmp_draw_world_window(buffer_background_image_data);
				draw_bar(buffer_background_image_data, x);

				// PASADA 3: y ademas se espera al momento en que el monitor
				// no esta dibujando, para que el volcado no le pille a
				// medias.
				if (pass == 3){
					wait_retrace();
				}

				bmp_paint_image_data_to_vga(buffer_background_image_data);

			}

		}

		getch();

	}

	set_video_mode(0x0003);
	bmp_close_files();
	bmp_delete_buffers();

	printf("\n");
	printf("La 1 parpadea porque borras y pintas delante del espectador.\n");
	printf("La 2 ya no parpadea, pero el volcado puede pillar al monitor a\n");
	printf("medio dibujar y ves la barra partida (tearing).\n");
	printf("La 3 es la que usa el juego.\n");
	printf("\n");

	return 0;

}
