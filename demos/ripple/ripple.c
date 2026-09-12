//===========================================================
// RIPPLE - la gota en el estanque
//
// Ondas que salen del centro de la pantalla hacia fuera. Se parece al
// wobble pero no es lo mismo, y la diferencia es la que hay entre una
// bandera y un charco:
//
//   wobble  el seno depende de la FILA, asi que las ondas viajan de
//           arriba abajo y todas tienen la misma fuerza
//
//   ripple  el seno depende de la DISTANCIA AL CENTRO, asi que las ondas
//           salen del medio, y ademas se van muriendo segun se alejan
//
// Ese apagarse con la distancia es lo que lo hace parecer agua de verdad y
// no una cortina. Sin eso el borde de la pantalla ondula igual que el
// centro y el ojo no se lo cree.
//
// Sigue siendo un calculo por FILA, no por pixel: barato.
//===========================================================

#include "demos\demolib.h"
#include "demos\ripple\ripple.h"

// Desplazamiento maximo, en el centro
#define RIPPLE_AMPLITUDE 	20

// Pasos de angulo por cada pixel de distancia: lo juntas que van las ondas
#define RIPPLE_DENSITY 		4

// Lo que avanzan las ondas por frame. NEGATIVO para que salgan del centro
// hacia fuera en vez de venirse hacia dentro.
#define RIPPLE_SPEED 		-5


int demo_ripple(unsigned char *image,
                unsigned char *screen,
                unsigned char *palette,
                unsigned long end_tick)
{
	int phase;
	int y;
	int distance;
	int amplitude;
	int shift;
	unsigned int row;

	(void)palette;

	phase = 0;

	while (demo_now() < end_tick){

		if (demo_escape_pressed() == 1){
			return 0;
		}

		for (y = 0; y < DEMO_HEIGHT; y++){

			// A que distancia esta esta fila del centro de la pantalla
			distance = y - (DEMO_HEIGHT / 2);
			if (distance < 0){
				distance = -distance;
			}

			//-----------------------------------------------
			// La onda se apaga con la distancia.
			//
			// En el centro vale RIPPLE_AMPLITUDE entera y en el borde
			// llega a 0, bajando en linea recta. Una caida cuadratica
			// seria mas fisica y costaria una multiplicacion mas por fila
			// para algo que el ojo no distingue.
			//-----------------------------------------------
			amplitude = (RIPPLE_AMPLITUDE * ((DEMO_HEIGHT / 2) - distance)) / (DEMO_HEIGHT / 2);

			if (amplitude < 0){
				amplitude = 0;
			}

			shift = (demo_sin(phase + (distance * RIPPLE_DENSITY)) * amplitude) >> DEMO_SHIFT;

			if (shift < 0){
				shift = shift + DEMO_WIDTH;
			}

			row = demo_row[y];

			memcpy(screen + row + shift, image + row, DEMO_WIDTH - shift);
			memcpy(screen + row, image + row + (DEMO_WIDTH - shift), shift);

		}

		demo_show(screen);

		phase = (phase + RIPPLE_SPEED) & DEMO_ANGLE_MASK;

	}

	return 1;

}
