//===========================================================
// STRIPES - bandas que se desplazan a velocidades distintas
//
// La pantalla se corta en tiras horizontales y cada tira se mueve a su
// ritmo: unas rapido a la derecha, otras despacio a la izquierda. La
// imagen se descuartiza y se recompone sola cuando las velocidades
// coinciden.
//
// Es primo del parallax de los juegos de plataformas, donde el fondo
// lejano va mas lento que el suelo para dar sensacion de profundidad. Aqui
// se usa al reves, para romper la imagen.
//
// La velocidad de cada tira sale de un seno de su numero de tira, no de un
// numero al azar: asi las tiras vecinas van parecido y el conjunto ondula
// en vez de parecer television estropeada.
//===========================================================

#include "demos\demolib.h"
#include "demos\stripes\stripes.h"

// Alto de cada banda
#define STRIPES_HEIGHT 	8
#define STRIPES_COUNT 	(DEMO_HEIGHT / STRIPES_HEIGHT)

// Velocidad maxima de una banda, en pixeles por frame
#define STRIPES_SPEED 	6

// Cuantos pasos de angulo hay entre una banda y la siguiente
#define STRIPES_SPREAD 	11


int demo_stripes(unsigned char *image,
                 unsigned char *screen,
                 unsigned char *palette,
                 unsigned long end_tick)
{
	int offset[STRIPES_COUNT];
	int stripe;
	int line;
	int y;
	int speed;
	int shift;
	unsigned int row;

	(void)palette;

	for (stripe = 0; stripe < STRIPES_COUNT; stripe++){
		offset[stripe] = 0;
	}

	while (demo_now() < end_tick){

		if (demo_escape_pressed() == 1){
			return 0;
		}

		for (stripe = 0; stripe < STRIPES_COUNT; stripe++){

			// La velocidad de esta banda: fija durante todo el efecto, y
			// sacada de un seno para que las vecinas se parezcan
			speed = (demo_sin(stripe * STRIPES_SPREAD) * STRIPES_SPEED) >> DEMO_SHIFT;

			offset[stripe] = offset[stripe] + speed;

			// Dentro de 0..319 para los dos memcpy. Con while y no con if:
			// una banda parada suma 0 y una rapida puede sumar 6, pero
			// despues de muchos frames conviene no fiarse.
			while (offset[stripe] < 0){
				offset[stripe] = offset[stripe] + DEMO_WIDTH;
			}
			while (offset[stripe] >= DEMO_WIDTH){
				offset[stripe] = offset[stripe] - DEMO_WIDTH;
			}

			shift = offset[stripe];

			for (line = 0; line < STRIPES_HEIGHT; line++){

				y   = (stripe * STRIPES_HEIGHT) + line;
				row = demo_row[y];

				memcpy(screen + row + shift, image + row, DEMO_WIDTH - shift);
				memcpy(screen + row, image + row + (DEMO_WIDTH - shift), shift);

			}

		}

		demo_show(screen);

	}

	return 1;

}
