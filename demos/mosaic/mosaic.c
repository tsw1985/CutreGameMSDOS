//===========================================================
// MOSAIC - la imagen se rompe en cuadrados y se recompone
//
// El pixelado que se abre y se cierra. Empieza en bloques enormes, en los
// que no se distingue nada, y va bajando hasta el pixel de verdad. Luego
// vuelve a subir.
//
// Cada bloque toma el color del pixel que hay en su esquina de arriba a la
// izquierda, que es como se hacia: ni medias, ni promedios, ni nada que
// cueste. Un color y se rellena.
//
// Y el relleno es un memset por fila de bloque, no un pixel cada vez.
//===========================================================

#include "demos\demolib.h"
#include "demos\mosaic\mosaic.h"

// El bloque mas gordo, en pixeles. 40 divide a 320 y a 200, asi que en el
// tamano maximo la pantalla queda en cuadros exactos sin restos raros.
#define MOSAIC_MAX_BLOCK 	40

// Pasos de angulo por frame: lo rapido que abre y cierra
#define MOSAIC_SPEED 		2


int demo_mosaic(unsigned char *image,
                unsigned char *screen,
                unsigned char *palette,
                unsigned long end_tick)
{
	int phase;
	int block;
	int last_block;
	int x, y;
	int by;
	int width, height;
	unsigned char color;
	unsigned int source;

	(void)palette;

	phase = 0;

	// El tamano del frame anterior. -1 la primera vez, que no coincide con
	// ningun tamano posible y obliga a pintar.
	last_block = -1;

	while (demo_now() < end_tick){

		if (demo_escape_pressed() == 1){
			return 0;
		}

		//---------------------------------------------------
		// El tamano del bloque de este frame.
		//
		// demo_sin() da de -256 a 256; sumandole 256 va de 0 a 512, y de
		// ahi sale un numero entre 1 y MOSAIC_MAX_BLOCK. Nunca 0: un
		// bloque de tamano 0 seria un bucle infinito.
		//---------------------------------------------------
		block = 1 + (((demo_sin(phase) + DEMO_ONE) * (MOSAIC_MAX_BLOCK - 1)) / (DEMO_ONE * 2));

		if (block < 1){
			block = 1;
		}

		//---------------------------------------------------
		// Y si el bloque mide lo mismo que en el frame anterior, NO HAY
		// NADA QUE HACER: el dibujo seria identico, y ya esta en screen.
		//
		// Esto no es una microoptimizacion, es la diferencia entre correr
		// y arrastrarse. El tamano solo cambia unas 40 veces en los 8
		// segundos que dura el efecto, y a 70 frames por segundo eso son
		// 560 frames: el 93% de ellos estaban repintando pixel a pixel un
		// resultado que ya tenian delante.
		//
		// Medido antes de esto: 86 microsegundos por frame contra los 8 de
		// los efectos baratos.
		//---------------------------------------------------
		if (block == last_block){
			demo_show(screen);
			phase = (phase + MOSAIC_SPEED) & DEMO_ANGLE_MASK;
			continue;
		}

		last_block = block;

		//---------------------------------------------------
		// Con bloques de 1 pixel no hay mosaico que valga: es la imagen
		// tal cual, y un memcpy hace el trabajo de 64000 memset de un byte.
		//---------------------------------------------------
		if (block == 1){

			memcpy(screen, image, DEMO_SCREEN);
			demo_show(screen);
			phase = (phase + MOSAIC_SPEED) & DEMO_ANGLE_MASK;
			continue;

		}

		for (y = 0; y < DEMO_HEIGHT; y += block){

			// El ultimo bloque de la columna casi nunca cabe entero
			height = block;
			if (y + height > DEMO_HEIGHT){
				height = DEMO_HEIGHT - y;
			}

			for (x = 0; x < DEMO_WIDTH; x += block){

				width = block;
				if (x + width > DEMO_WIDTH){
					width = DEMO_WIDTH - x;
				}

				// Un solo color para todo el cuadro: el de su esquina
				source = demo_row[y] + (unsigned int)x;
				color  = image[source];

				for (by = 0; by < height; by++){
					memset(screen + demo_row[y + by] + (unsigned int)x, color, width);
				}

			}

		}

		demo_show(screen);

		phase = (phase + MOSAIC_SPEED) & DEMO_ANGLE_MASK;

	}

	return 1;

}
