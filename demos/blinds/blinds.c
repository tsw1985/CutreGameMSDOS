//===========================================================
// BLINDS - la persiana veneciana
//
// La pantalla se parte en tiras horizontales y cada tira se abre desde su
// centro hasta descubrir la imagen entera. Luego se cierra otra vez.
//
// Es una transicion clasica de presentacion, de las que llevaban los
// programas de diapositivas de la epoca. Y es practicamente gratis: por
// cada tira hay un memcpy de la parte abierta y un memset de la cerrada.
//
// Se abren todas a la vez pero con un pequeno retraso entre una y la
// siguiente, que es lo que le da el aire de persiana de verdad en vez de
// parecer un telon.
//===========================================================

#include "demos\demolib.h"
#include "demos\blinds\blinds.h"

// Alto de cada tira. 10 cabe 20 veces en 200 sin resto.
#define BLINDS_SLAT 		10
#define BLINDS_COUNT 		(DEMO_HEIGHT / BLINDS_SLAT)

// Retraso entre una tira y la siguiente, en pasos de angulo
#define BLINDS_STAGGER 		6

// Pasos de angulo por frame
#define BLINDS_SPEED 		3


int demo_blinds(unsigned char *image,
                unsigned char *screen,
                unsigned char *palette,
                unsigned long end_tick)
{
	int phase;
	int slat;
	int line;
	int y;
	int open;
	int half;
	unsigned int row;

	(void)palette;

	phase = 0;

	while (demo_now() < end_tick){

		if (demo_escape_pressed() == 1){
			return 0;
		}

		for (slat = 0; slat < BLINDS_COUNT; slat++){

			//-----------------------------------------------
			// Cuanto esta abierta ESTA tira, de 0 a la mitad del ancho.
			//
			// Cada tira va un poco por detras de la de arriba, y de ahi
			// sale la ola que recorre la persiana.
			//-----------------------------------------------
			half = (((demo_sin(phase + (slat * BLINDS_STAGGER)) + DEMO_ONE)
			         * (DEMO_WIDTH / 2)) / (DEMO_ONE * 2));

			if (half < 0){
				half = 0;
			}
			if (half > DEMO_WIDTH / 2){
				half = DEMO_WIDTH / 2;
			}

			open = half * 2;

			for (line = 0; line < BLINDS_SLAT; line++){

				y   = (slat * BLINDS_SLAT) + line;
				row = demo_row[y];

				// Lo cerrado, a negro, a los dos lados
				memset(screen + row, 0, (DEMO_WIDTH / 2) - half);
				memset(screen + row + (DEMO_WIDTH / 2) + half, 0, (DEMO_WIDTH / 2) - half);

				// Y lo abierto, la imagen, por el centro
				memcpy(screen + row + (DEMO_WIDTH / 2) - half,
				       image  + row + (DEMO_WIDTH / 2) - half,
				       open);

			}

		}

		demo_show(screen);

		phase = (phase + BLINDS_SPEED) & DEMO_ANGLE_MASK;

	}

	return 1;

}
