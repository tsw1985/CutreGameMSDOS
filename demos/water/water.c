//===========================================================
// WATER - la foto reflejada en el agua
//
// La mitad de arriba es la foto. La mitad de abajo es la misma foto del
// reves, ondulando, como si la escena estuviera al borde de un estanque.
//
// Es un efecto de dos lineas de idea y sale muy caro de creer. La parte
// que lo vende no es la ondulacion: es que el reflejo esta INVERTIDO y se
// aleja, o sea que las ondas mas lejanas (las de arriba del reflejo, que
// son las que estan mas cerca del horizonte) son mas pequenas y mas
// juntas que las de abajo.
//
//
// ES EL WOBBLE, CON TRES DIFERENCIAS
//
// [wobble](../wobble/wobble.c) desplaza cada fila segun un seno. Esto
// tambien, y por eso esta tan lejos de el en el recorrido: lo que cambia
// no es la tecnica, es lo que se hace con ella.
//
//   1. solo se ondula la mitad de abajo
//   2. esa mitad lee la foto AL REVES, de la fila 99 hacia arriba
//   3. la amplitud CRECE hacia abajo, en vez de ser constante
//
// La tercera es la que hace el trabajo. Con amplitud constante parece una
// bandera puesta boca abajo; creciendo, parece agua.
//===========================================================

#include "demos\demolib.h"
#include "demos\water\water.h"

// Donde esta la orilla. La mitad justa de la pantalla.
#define WATER_HORIZON 		(DEMO_HEIGHT / 2)

// Lo que se mueve la fila mas lejana del reflejo y la mas cercana, en
// pixeles. La de arriba casi no se mueve: es el horizonte.
#define WATER_AMPLITUDE_NEAR 	7
#define WATER_AMPLITUDE_FAR 	1

// Lo apretadas que van las ondas (pasos de angulo por fila) y lo deprisa
// que corren (pasos por frame)
#define WATER_DENSITY 		6
#define WATER_SPEED 		3


int demo_water(unsigned char *image,
               unsigned char *screen,
               unsigned char *palette,
               unsigned long end_tick)
{
	int phase;
	int y;
	int depth;
	int amplitude;
	int shift;
	int source_y;
	unsigned int destination;
	unsigned int source;

	(void)palette;

	phase = 0;

	//-------------------------------------------------------
	// La mitad de arriba no cambia NUNCA, asi que se copia una sola vez,
	// fuera del bucle. Son 32.000 bytes por frame que no se tocan.
	//-------------------------------------------------------
	memcpy(screen, image, (unsigned int)WATER_HORIZON * DEMO_WIDTH);

	while (demo_now() < end_tick){

		if (demo_escape_pressed() == 1){
			return 0;
		}

		destination = demo_row[WATER_HORIZON];

		for (y = WATER_HORIZON; y < DEMO_HEIGHT; y++){

			//-----------------------------------------------
			// Lo hondo que estamos dentro del reflejo, de 0 en la orilla a
			// 99 en el borde de abajo.
			//-----------------------------------------------
			depth = y - WATER_HORIZON;

			//-----------------------------------------------
			// LA AMPLITUD CRECE CON LA DISTANCIA A LA ORILLA, y esto es el
			// efecto entero.
			//
			// En la orilla vale WATER_AMPLITUDE_FAR (casi nada, porque el
			// reflejo de lo que esta lejos apenas se mueve) y abajo del
			// todo WATER_AMPLITUDE_NEAR. Es una recta, no un seno: lo que
			// tiene que crecer es cuanto se mueve, no como.
			//-----------------------------------------------
			amplitude = WATER_AMPLITUDE_FAR
			          + (((WATER_AMPLITUDE_NEAR - WATER_AMPLITUDE_FAR) * depth)
			             / (DEMO_HEIGHT - WATER_HORIZON));

			shift = (demo_sin(phase + (depth * WATER_DENSITY)) * amplitude) >> DEMO_SHIFT;

			//-----------------------------------------------
			// Y la fila de origen, contada HACIA ARRIBA desde la orilla.
			// Eso es lo que da la vuelta al reflejo.
			//-----------------------------------------------
			source_y = WATER_HORIZON - 1 - depth;

			if (source_y < 0){
				source_y = 0;
			}

			source = demo_row[source_y];

			//-----------------------------------------------
			// El desplazamiento horizontal, recortado. shift es int y
			// puede ser negativo: metido en un unsigned seria 65527 y el
			// memcpy se llevaria por delante lo que hubiera detras.
			//-----------------------------------------------
			if (shift >= 0){

				memset(screen + destination, 0, (unsigned int)shift);
				memcpy(screen + destination + shift, image + source,
				       (unsigned int)(DEMO_WIDTH - shift));

			}else{

				memcpy(screen + destination, image + source - shift,
				       (unsigned int)(DEMO_WIDTH + shift));
				memset(screen + destination + DEMO_WIDTH + shift, 0,
				       (unsigned int)(-shift));

			}

			destination += DEMO_WIDTH;

		}

		demo_show(screen);

		phase = (phase + WATER_SPEED) & DEMO_ANGLE_MASK;

	}

	return 1;

}
