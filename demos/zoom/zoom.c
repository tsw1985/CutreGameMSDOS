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
	//-------------------------------------------------------
	// LA TABLA DE COLUMNAS, y es de lo que va esta funcion.
	//
	// En un zoom sin giro, la columna de origen de un pixel depende SOLO
	// de su x: la fila 0 y la fila 199 leen exactamente las mismas 320
	// columnas. La version anterior calculaba esa cuenta 64000 veces por
	// frame, una por pixel, cuando hay 320 respuestas distintas.
	//
	// Aqui se calculan las 320 una vez al principio del frame y el bucle
	// interior se queda en "leer la tabla, sumar y copiar": ni una suma
	// larga, ni un desplazamiento, ni una comparacion de limites.
	//
	// 320 cuentas por frame en vez de 64000. Y de paso salen gratis las
	// dos columnas donde la imagen empieza y acaba, asi que los bordes
	// negros se rellenan con memset en lugar de pixel a pixel.
	//-------------------------------------------------------
	unsigned int column[DEMO_WIDTH];
	int first, last;

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

		//---------------------------------------------------
		// Las 320 columnas del frame, y de paso donde empieza y acaba la
		// parte visible.
		//
		// step es siempre positivo, asi que las columnas van creciendo y
		// la parte de dentro es un tramo seguido: un principio y un final,
		// sin agujeros en medio.
		//---------------------------------------------------
		first = DEMO_WIDTH;
		last  = -1;

		u = start_u;

		for (x = 0; x < DEMO_WIDTH; x++){

			sx = (int)(u >> DEMO_SHIFT);

			if ((unsigned int)sx < DEMO_WIDTH){
				column[x] = (unsigned int)sx;
				if (x < first){
					first = x;
				}
				last = x;
			}

			u += step;

		}

		destination = 0;
		v = start_v;

		for (y = 0; y < DEMO_HEIGHT; y++){

			sy = (int)(v >> DEMO_SHIFT);

			//-----------------------------------------------
			// Una fila fuera de la imagen se pinta negra de golpe. Cuando
			// la imagen esta lejos, la mayoria de las filas son estas.
			//-----------------------------------------------
			if ((unsigned int)sy >= DEMO_HEIGHT || last < first){

				memset(screen + destination, 0, DEMO_WIDTH);

			}else{

				source_row = demo_row[sy];

				// Los dos bordes negros, de una vez
				if (first > 0){
					memset(screen + destination, 0, first);
				}
				if (last < DEMO_WIDTH - 1){
					memset(screen + destination + last + 1, 0, DEMO_WIDTH - 1 - last);
				}

				// Y la parte visible, sin una sola comprobacion dentro
				for (x = first; x <= last; x++){
					screen[destination + x] = image[source_row + column[x]];
				}

			}

			destination += DEMO_WIDTH;
			v += step;

		}

		demo_show(screen);

		breath = (breath + ZOOM_BREATH) & DEMO_ANGLE_MASK;

	}

	return 1;

}
