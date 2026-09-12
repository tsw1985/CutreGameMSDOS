//===========================================================
// ZOOM - acercarse y alejarse respirando
//
// El rotozoom sin el roto. Y por eso es MUCHO mas barato, no solo un poco:
// sin giro, la coordenada v no cambia a lo largo de una fila. Solo cambia
// al bajar de fila.
//
// O sea que dentro del bucle de columnas hay UNA suma en vez de dos, y la
// fila de origen se calcula una sola vez por fila en lugar de 320 veces.
// Es el mismo efecto que haria el rotozoom con angulo 0, escrito aparte
// porque puesto asi corre en un 8086.
//===========================================================

#include "demos\demolib.h"
#include "demos\zoom\zoom.h"

// De 0.6 a 2.2 aumentos, en punto fijo 8.8
#define ZOOM_SCALE_MID 	((DEMO_ONE * 7) / 5)	// 1.4
#define ZOOM_SCALE_AMP 	((DEMO_ONE * 4) / 5)	// +/- 0.8

// Pasos de angulo por frame: lo lento que respira
#define ZOOM_BREATH 	1


int demo_zoom(unsigned char *image,
              unsigned char *screen,
              unsigned char *palette,
              unsigned long end_tick)
{
	int breath;
	long scale;
	long step;
	long start_u, start_v;
	long u, v;
	int x, y;
	int sx, sy;
	unsigned int destination;
	unsigned int source_row;

	(void)palette;

	breath = 0;

	while (demo_now() < end_tick){

		if (demo_escape_pressed() == 1){
			return 0;
		}

		scale = (long)ZOOM_SCALE_MID
		      + (((long)demo_sin(breath) * (long)ZOOM_SCALE_AMP) >> DEMO_SHIFT);

		if (scale < 32){
			scale = 32;
		}

		// Cuanto avanza el origen por cada pixel de pantalla. Al ser un
		// zoom puro, es el mismo numero en las dos direcciones.
		step = scale;

		// La esquina de arriba a la izquierda, medida desde el centro de la
		// imagen, para que el zoom sea alrededor del centro y no de la
		// esquina.
		start_u = ((long)(DEMO_WIDTH  / 2) << DEMO_SHIFT) - (step * (DEMO_WIDTH  / 2));
		start_v = ((long)(DEMO_HEIGHT / 2) << DEMO_SHIFT) - (step * (DEMO_HEIGHT / 2));

		destination = 0;
		v = start_v;

		for (y = 0; y < DEMO_HEIGHT; y++){

			sy = (int)(v >> DEMO_SHIFT);

			//-----------------------------------------------
			// La fila entera de origen se decide AQUI, una vez.
			//
			// Y si esta fila cae fuera de la imagen, se pinta negra de
			// golpe con un memset en vez de mirar pixel a pixel. Cuando la
			// imagen esta lejos, la mayoria de las filas son estas.
			//-----------------------------------------------
			if ((unsigned int)sy >= DEMO_HEIGHT){

				memset(screen + destination, 0, DEMO_WIDTH);
				destination += DEMO_WIDTH;

			}else{

				source_row = demo_row[sy];
				u = start_u;

				for (x = 0; x < DEMO_WIDTH; x++){

					sx = (int)(u >> DEMO_SHIFT);

					if ((unsigned int)sx < DEMO_WIDTH){
						screen[destination] = image[source_row + (unsigned int)sx];
					}else{
						screen[destination] = 0;
					}

					destination++;
					u += step;

				}

			}

			v += step;

		}

		demo_show(screen);

		breath = (breath + ZOOM_BREATH) & DEMO_ANGLE_MASK;

	}

	return 1;

}
