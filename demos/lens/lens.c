//===========================================================
// LENS - la lupa que se pasea por la foto
//
// Un circulo de cristal que recorre la imagen y agranda lo que pilla
// debajo. Clasico de los 90 y de los salvapantallas de la epoca, y uno de
// los pocos efectos que deja ver la foto ENTERA todo el rato: el resto de
// la pantalla no se toca.
//
//
// LA IDEA: LA LUPA NO SE MUEVE, LA FOTO SI
//
// Lo que deforma una lupa depende solo de DONDE ESTA EL PIXEL DENTRO DE
// LA LUPA, no de por donde ande la lupa. El pixel del centro siempre lee
// del centro, el del borde siempre lee del borde.
//
// O sea que el desplazamiento de cada punto del cristal se calcula UNA
// vez, al entrar, y se guarda como un desplazamiento en bytes:
//
//     lens_delta[i] = (dy_origen * 320) + dx_origen
//
// Y mover la lupa es sumarle a todo eso la posicion del centro. Cada frame
// son 6.100 sumas y 6.100 lecturas, y ni una raiz ni una division.
//
// Es la misma idea del [tunel](../tunnel/tunnel.c) - precalcular lo que no
// cambia - aplicada a una superficie de 6.100 pixeles en vez de a los
// 64.000 de la pantalla. Por eso la tabla ocupa 12 KB y no 128.
//
//
// LA FORMA DEL CRISTAL
//
// Un radio r del cristal lee del radio:
//
//     origen = r * (1 - k * (1 - (r/R)^2))
//
// En el borde (r = R) el parentesis vale 1 y no desplaza nada, asi que la
// lupa se funde con la foto sin un corte. En el centro (r = 0) aumenta
// 1/(1-k), que con k = 0,4 son 1,67 aumentos.
//
// Lo importante es que en el borde NO desplace: una lupa que en el borde
// salta se ve como un recorte pegado, no como cristal.
//===========================================================

#include "demos\demolib.h"
#include "demos\lens\lens.h"

// El radio del cristal, en pixeles
#define LENS_RADIUS 		44
#define LENS_SIDE 			((LENS_RADIUS * 2) + 1)

// Lo que aumenta en el centro, en 8.8. 102/256 = 0,4, o sea 1,67 aumentos.
#define LENS_STRENGTH 		102

// El paseo: dos senos de velocidades primas entre si, como en bounce
#define LENS_SPEED_X 		3
#define LENS_SPEED_Y 		5


int demo_lens(unsigned char *image,
              unsigned char *screen,
              unsigned char *palette,
              unsigned long end_tick)
{
	int *delta;
	int first[LENS_SIDE];
	int last[LENS_SIDE];

	int lx, ly;
	int squared;
	int radius;
	int ratio;
	int factor;
	int source_radius;
	int angle_x, angle_y;
	int center_x, center_y;
	int range_x, range_y;
	int row;
	int x;
	unsigned int center_offset;
	unsigned int destination;
	unsigned int index;

	(void)palette;

	delta = (int *)malloc((unsigned int)LENS_SIDE * (unsigned int)LENS_SIDE * sizeof(int));

	if (delta == NULL){

		while (demo_now() < end_tick){

			if (demo_escape_pressed() == 1){
				return 0;
			}

			memcpy(screen, image, DEMO_SCREEN);
			demo_show(screen);

		}

		return 1;

	}

	//-------------------------------------------------------
	// LA TABLA DEL CRISTAL, una vez.
	//
	// Se recorre el cuadrado que contiene al circulo y de cada punto se
	// guarda de donde tiene que leer. Los puntos de fuera del circulo no
	// se guardan: en su lugar se apunta donde empieza y acaba la parte
	// util de cada fila, que es lo que deja el bucle de pintar sin un solo
	// "estoy dentro?".
	//
	// Es el mismo truco del tramo visible que usa el rotozoom.
	//-------------------------------------------------------
	index = 0;

	for (ly = -LENS_RADIUS; ly <= LENS_RADIUS; ly++){

		row        = ly + LENS_RADIUS;
		first[row] = LENS_SIDE;
		last[row]  = -1;

		for (lx = -LENS_RADIUS; lx <= LENS_RADIUS; lx++){

			squared = (lx * lx) + (ly * ly);

			if (squared <= LENS_RADIUS * LENS_RADIUS){

				if (first[row] == LENS_SIDE){
					first[row] = lx + LENS_RADIUS;
				}
				last[row] = lx + LENS_RADIUS;

				//---------------------------------------
				// La raiz entera, aqui a lo bruto: son 6.100 puntos UNA
				// vez en toda la vida del efecto, asi que no merece la
				// pena traerse el metodo bit a bit del tunel.
				//---------------------------------------
				radius = 0;
				while ((radius + 1) * (radius + 1) <= squared){
					radius++;
				}

				if (radius == 0){

					delta[index] = 0;		// el centro exacto no se mueve

				}else{

					// (r/R) en 8.8, y su cuadrado
					ratio  = (radius * DEMO_ONE) / LENS_RADIUS;
					ratio  = (ratio * ratio) >> DEMO_SHIFT;

					// 1 - k*(1 - (r/R)^2)
					factor = DEMO_ONE
					       - ((LENS_STRENGTH * (DEMO_ONE - ratio)) >> DEMO_SHIFT);

					source_radius = (radius * factor) >> DEMO_SHIFT;

					delta[index] = (((ly * source_radius) / radius) * DEMO_WIDTH)
					             +  ((lx * source_radius) / radius);

				}

			}

			index++;

		}

	}

	//-------------------------------------------------------
	// El centro se queda a un radio de los bordes SIEMPRE, y con eso el
	// origen nunca se sale del buffer.
	//
	// Es lo que permite que el bucle de pintar no compruebe limites: no
	// hay recorte porque no puede haber nada que recortar. La alternativa
	// seria un "esta dentro?" por pixel, 6.100 veces por frame.
	//-------------------------------------------------------
	range_x = (DEMO_WIDTH  / 2) - LENS_RADIUS - 1;
	range_y = (DEMO_HEIGHT / 2) - LENS_RADIUS - 1;

	angle_x = 0;
	angle_y = 0;

	while (demo_now() < end_tick){

		if (demo_escape_pressed() == 1){
			free(delta);
			return 0;
		}

		center_x = (DEMO_WIDTH  / 2) + ((demo_sin(angle_x) * range_x) >> DEMO_SHIFT);
		center_y = (DEMO_HEIGHT / 2) + ((demo_cos(angle_y) * range_y) >> DEMO_SHIFT);

		// La foto entera, sin tocar. Lo unico que cambia es el circulo.
		memcpy(screen, image, DEMO_SCREEN);

		center_offset = demo_row[center_y] + (unsigned int)center_x;

		for (row = 0; row < LENS_SIDE; row++){

			if (last[row] < first[row]){
				continue;
			}

			destination = center_offset
			            + (unsigned int)((row - LENS_RADIUS) * DEMO_WIDTH)
			            + (unsigned int)(first[row] - LENS_RADIUS);

			index = (unsigned int)row * (unsigned int)LENS_SIDE
			      + (unsigned int)first[row];

			for (x = first[row]; x <= last[row]; x++){

				//-------------------------------------------
				// El origen se cuenta desde el CENTRO de la lupa, no
				// desde el pixel que se esta pintando.
				//
				// lens_delta guarda "de donde lee el punto (lx,ly) del
				// cristal", y ese de-donde esta medido respecto al centro.
				// Sumarselo al pixel actual en vez de al centro lo duplica
				// todo y se sale del buffer por abajo.
				//-------------------------------------------
				screen[destination] = image[center_offset + (unsigned int)delta[index]];

				destination++;
				index++;

			}

		}

		demo_show(screen);

		angle_x = (angle_x + LENS_SPEED_X) & DEMO_ANGLE_MASK;
		angle_y = (angle_y + LENS_SPEED_Y) & DEMO_ANGLE_MASK;

	}

	free(delta);

	return 1;

}
