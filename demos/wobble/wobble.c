//===========================================================
// WOBBLE - la imagen ondula como si fuera agua
//
// Cada fila de la pantalla se desplaza a los lados segun un seno, y el
// seno se mueve un poco cada frame. El resultado es una bandera, o el
// reflejo en un charco.
//
// Es de los efectos mas baratos que hay y de los que mas impresionan: no
// hay una sola multiplicacion por pixel, solo una consulta a la tabla de
// senos por FILA. 200 consultas por frame contra las 64000 operaciones de
// un rotozoom.
//
// El desplazamiento da la vuelta en vez de dejar hueco: lo que se sale por
// la derecha vuelve a entrar por la izquierda, asi que no hay bordes
// negros y la ondulacion parece infinita.
//===========================================================

#include "demos\demolib.h"
#include "demos\wobble\wobble.h"

// Cuantos pixeles se desplaza como mucho una fila
#define WOBBLE_AMPLITUDE 	24

// Cuantos pasos de angulo hay entre una fila y la siguiente. Cuanto mas
// alto, mas apretadas las ondas.
#define WOBBLE_DENSITY 		3

// Lo que avanza la onda por frame
#define WOBBLE_SPEED 		4


int demo_wobble(unsigned char *image,
                unsigned char *screen,
                unsigned char *palette,
                unsigned long end_tick)
{
	int phase;
	int y;
	int shift;
	unsigned int row;

	(void)palette;

	phase = 0;

	while (demo_now() < end_tick){

		if (demo_escape_pressed() == 1){
			return 0;
		}

		for (y = 0; y < DEMO_HEIGHT; y++){

			//-----------------------------------------------
			// El desplazamiento de esta fila. demo_sin() va de -256 a 256,
			// asi que multiplicar por la amplitud y bajar 8 bits deja un
			// numero entre -AMPLITUD y +AMPLITUD.
			//-----------------------------------------------
			shift = (demo_sin(phase + (y * WOBBLE_DENSITY)) * WOBBLE_AMPLITUDE) >> DEMO_SHIFT;

			// Dejarlo dentro de 0..319, que es lo que necesitan los dos
			// memcpy de abajo. Con la amplitud que hay nunca hace falta
			// mas de una vuelta, asi que no hay bucle.
			if (shift < 0){
				shift = shift + DEMO_WIDTH;
			}

			row = demo_row[y];

			//-----------------------------------------------
			// La fila desplazada, en dos trozos y sin mirar pixel a pixel.
			//
			//   imagen:    [ A A A A | B B B B B B B ]
			//   pantalla:  [ B B B B B B B | A A A A ]
			//
			// El trozo que se sale por la derecha es el que entra por la
			// izquierda. Dos memcpy y la fila esta hecha.
			//-----------------------------------------------
			memcpy(screen + row + shift, image + row, DEMO_WIDTH - shift);
			memcpy(screen + row, image + row + (DEMO_WIDTH - shift), shift);

		}

		demo_wait_retrace();
		bmp_paint_image_data_to_vga(screen);

		phase = (phase + WOBBLE_SPEED) & DEMO_ANGLE_MASK;

	}

	return 1;

}
