//===========================================================
// BOUNCE - la imagen se pasea por la pantalla
//
// La foto entera se mueve sobre un fondo negro dibujando una figura de
// Lissajous: un seno para la horizontal y otro para la vertical, con
// velocidades distintas. Como las dos velocidades no son multiplos, el
// recorrido tarda muchisimo en repetirse y no parece un bucle.
//
// Es lo que hacian los logos que rebotaban por la pantalla, y cuesta
// practicamente nada: la imagen se copia entera desplazada, recortando lo
// que se sale. Un memcpy por fila.
//
// Lo unico que hay que hacer bien es el recorte, y hay que hacerlo con
// enteros CON SIGNO. Es la misma trampa que el recorte de sprites del
// juego: un desplazamiento de -9 metido en un unsigned es 65527 y el
// memcpy se va a escribir a la China.
//===========================================================

#include "demos\demolib.h"
#include "demos\bounce\bounce.h"

// Lo lejos que llega del centro, en pixeles
#define BOUNCE_RANGE_X 	90
#define BOUNCE_RANGE_Y 	60

// Velocidades distintas y sin divisores comunes: asi el recorrido se
// cierra tarde y no se ve el bucle
#define BOUNCE_SPEED_X 	3
#define BOUNCE_SPEED_Y 	5


int demo_bounce(unsigned char *image,
                unsigned char *screen,
                unsigned char *palette,
                unsigned long end_tick)
{
	int angle_x;
	int angle_y;
	int offset_x;
	int offset_y;
	int y;
	int source_y;
	int copy_width;
	int destination_x;
	int source_x;

	(void)palette;

	angle_x = 0;
	angle_y = 0;

	while (demo_now() < end_tick){

		if (demo_escape_pressed() == 1){
			return 0;
		}

		offset_x = (demo_sin(angle_x) * BOUNCE_RANGE_X) >> DEMO_SHIFT;
		offset_y = (demo_cos(angle_y) * BOUNCE_RANGE_Y) >> DEMO_SHIFT;

		// Fondo negro, que es lo que se ve por donde la imagen no llega
		memset(screen, 0, DEMO_SCREEN);

		for (y = 0; y < DEMO_HEIGHT; y++){

			// De que fila de la imagen viene esta fila de pantalla
			source_y = y - offset_y;

			if (source_y < 0 || source_y >= DEMO_HEIGHT){
				continue;		// esta fila se queda negra
			}

			//-----------------------------------------------
			// El recorte horizontal, en enteros CON SIGNO.
			//
			// Con la imagen desplazada a la derecha se copia menos y se
			// empieza mas alla; desplazada a la izquierda se empieza en 0
			// de la pantalla pero dentro de la imagen. Y si no queda nada
			// visible, no se copia nada.
			//-----------------------------------------------
			if (offset_x >= 0){
				destination_x = offset_x;
				source_x      = 0;
				copy_width    = DEMO_WIDTH - offset_x;
			}else{
				destination_x = 0;
				source_x      = -offset_x;
				copy_width    = DEMO_WIDTH + offset_x;
			}

			if (copy_width <= 0){
				continue;
			}

			memcpy(screen + demo_row[y] + (unsigned int)destination_x,
			       image  + demo_row[source_y] + (unsigned int)source_x,
			       copy_width);

		}

		demo_show(screen);

		angle_x = (angle_x + BOUNCE_SPEED_X) & DEMO_ANGLE_MASK;
		angle_y = (angle_y + BOUNCE_SPEED_Y) & DEMO_ANGLE_MASK;

	}

	return 1;

}
