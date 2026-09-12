//===========================================================
// SCROLL - la imagen se desplaza infinita y da la vuelta sola
//
// Se mueve en diagonal, y lo que se sale por un lado entra por el otro.
// No hay principio ni final: la imagen se comporta como si estuviera
// pegada a un cilindro en las dos direcciones.
//
// Es el efecto mas barato de la coleccion. Ni un pixel se toca de uno en
// uno: cada fila de la pantalla son dos memcpy, y la fila de origen sale
// de sumar y dar la vuelta. 400 memcpy por frame y ya esta.
//===========================================================

#include "demos\demolib.h"
#include "demos\scroll\scroll.h"

// Pixeles por frame en cada direccion. Distintos a proposito, y sin
// divisores comunes, para que el recorrido tarde mucho en repetirse.
#define SCROLL_SPEED_X 	1
#define SCROLL_SPEED_Y 	1

// Cada cuantos frames avanza el vertical. Mas alto = mas tumbado el
// recorrido, y se nota menos que se repite.
#define SCROLL_Y_EVERY 	3


int demo_scroll(unsigned char *image,
                unsigned char *screen,
                unsigned char *palette,
                unsigned long end_tick)
{
	int offset_x;
	int offset_y;
	int counter;
	int y;
	int source_y;
	unsigned int destination_row;
	unsigned int source_row;

	(void)palette;

	offset_x = 0;
	offset_y = 0;
	counter  = 0;

	while (demo_now() < end_tick){

		if (demo_escape_pressed() == 1){
			return 0;
		}

		for (y = 0; y < DEMO_HEIGHT; y++){

			// Que fila de la imagen toca, dando la vuelta por arriba
			source_y = y + offset_y;
			if (source_y >= DEMO_HEIGHT){
				source_y = source_y - DEMO_HEIGHT;
			}

			destination_row = demo_row[y];
			source_row      = demo_row[source_y];

			// Y la misma partida en dos que hace el wobble, pero con el
			// mismo desplazamiento para todas las filas
			memcpy(screen + destination_row + offset_x,
			       image + source_row,
			       DEMO_WIDTH - offset_x);

			memcpy(screen + destination_row,
			       image + source_row + (DEMO_WIDTH - offset_x),
			       offset_x);

		}

		demo_show(screen);

		offset_x = offset_x + SCROLL_SPEED_X;
		if (offset_x >= DEMO_WIDTH){
			offset_x = 0;
		}

		counter++;
		if (counter >= SCROLL_Y_EVERY){

			counter = 0;

			offset_y = offset_y + SCROLL_SPEED_Y;
			if (offset_y >= DEMO_HEIGHT){
				offset_y = 0;
			}

		}

	}

	return 1;

}
