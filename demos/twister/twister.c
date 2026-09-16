//===========================================================
// TWISTER - la columna retorcida
//
// Una columna de cuatro caras que gira, y que ademas esta retorcida sobre
// si misma: la de arriba no mira donde mira la de abajo. La foto va pegada
// a las cuatro caras, asi que se ve pasar entera segun gira.
//
// Es de los efectos mas reconocibles de la epoca (el de Second Reality) y
// es de los mas baratos que hay, por un motivo que merece entenderse.
//
//
// ES UN EFECTO 3D QUE NO TIENE 3D
//
// Un cubo de verdad hay que rotarlo, proyectarlo y rellenarlo (ver
// texcube). Una columna vertical no: por mucho que gire, un punto NUNCA
// cambia de fila. Una fila de la pantalla solo puede ensenar lo que hay a
// esa altura de la columna.
//
// O sea que el problema entero se cae a UNA dimension. Por cada fila:
//
//   1. el angulo de la columna a esa altura   (una suma)
//   2. las cuatro esquinas, que en pantalla son cuatro x
//   3. rellenar los tramos entre esquina y esquina con la foto
//
// No hay matriz, no hay z, no hay division por profundidad. Cuatro senos y
// un relleno de tramos por fila.
//
//
// Y EL RETORCIDO SON TRES PALABRAS
//
//     angle = phase + (y * TWISTER_TWIST);
//
// Si se le quita el "y *", las cuatro esquinas caen en la misma x en todas
// las filas y lo que queda es una caja girando, que ya no impresiona a
// nadie. Ese producto por la fila es TODO el efecto.
//===========================================================

#include "demos\demolib.h"
#include "demos\twister\twister.h"

// El radio de la columna, en pixeles. 88 deja una columna de 176 de ancho
// en su momento mas abierto, que sobre 320 es poco mas de la mitad.
#define TWISTER_RADIUS 		88

// Cuanto se retuerce: pasos de angulo por FILA. Con 1, las 200 filas
// barren 200 de los 256 pasos, o sea tres cuartos de vuelta de arriba a
// abajo. Subirlo a 2 da una columna que da vuelta y media y se ve
// demasiado apretada; a 0 deja de ser un twister.
#define TWISTER_TWIST 		1

// Lo que gira por frame
#define TWISTER_SPIN 		2

// El ancho de foto que se pega en cada cara. Cuatro caras, 320 pixeles: la
// foto entera da la vuelta a la columna exactamente una vez.
#define TWISTER_FACE_WIDTH 	(DEMO_WIDTH / 4)


int demo_twister(unsigned char *image,
                 unsigned char *screen,
                 unsigned char *palette,
                 unsigned long end_tick)
{
	int corner_x[5];

	int phase;
	int angle;
	int corner;
	int face;
	int y, x;
	int left, right;
	int span;
	long texture_u;
	long texture_step;
	unsigned int destination;
	unsigned int source_row;

	(void)palette;

	phase = 0;

	while (demo_now() < end_tick){

		if (demo_escape_pressed() == 1){
			return 0;
		}

		//---------------------------------------------------
		// El fondo. Negro entero cada frame, porque la columna cambia de
		// ancho constantemente y lo que dejo de tapar el frame anterior
		// tiene que desaparecer.
		//
		// El indice 0 es negro por contrato en esta carpeta: lo dice
		// demos.h y lo cumplen las quince imagenes.
		//---------------------------------------------------
		memset(screen, 0, DEMO_SCREEN);

		destination = 0;

		for (y = 0; y < DEMO_HEIGHT; y++){

			//-----------------------------------------------
			// EL ANGULO DE ESTA FILA. Aqui esta el retorcido entero.
			//-----------------------------------------------
			angle = phase + (y * TWISTER_TWIST);

			//-----------------------------------------------
			// Las cuatro esquinas de la columna a esta altura, vistas
			// desde delante. Estan a un cuarto de vuelta una de otra, y un
			// cuarto de 256 son 64.
			//
			// Solo hace falta la x: la profundidad no se usa para dibujar,
			// solo decidiria quien tapa a quien, y de eso se encarga solo
			// el orden de las esquinas (ver abajo).
			//
			// La quinta es la primera otra vez, para que el bucle de caras
			// no tenga que preguntar si se ha dado la vuelta.
			//-----------------------------------------------
			for (corner = 0; corner < 5; corner++){

				corner_x[corner] = (DEMO_WIDTH / 2)
				                 + ((demo_sin(angle + (corner * 64)) * TWISTER_RADIUS) >> DEMO_SHIFT);

			}

			source_row = demo_row[y];

			for (face = 0; face < 4; face++){

				left  = corner_x[face];
				right = corner_x[face + 1];

				//-------------------------------------------
				// LA CARA QUE MIRA PARA OTRO LADO NO SE PINTA, y el test
				// son estos dos caracteres: left < right.
				//
				// Al girar la columna, las cuatro esquinas se mueven en
				// pantalla y las caras se dan la vuelta. Una cara cuya
				// esquina izquierda ha adelantado a la derecha es una cara
				// que estamos viendo por detras.
				//
				// Y como las cuatro se recorren en orden alrededor de la
				// columna, las que pasan el test NUNCA se solapan: no hace
				// falta ordenar por profundidad ni llevar un buffer de z.
				// Es el mismo argumento que usa texcube con su producto
				// vectorial, en una dimension menos.
				//-------------------------------------------
				if (left >= right){
					continue;
				}

				span = right - left;

				//-------------------------------------------
				// La foto, estirada sobre la cara.
				//
				// Cada cara se lleva un cuarto de la anchura de la foto, y
				// ese cuarto hay que meterlo en los "span" pixeles que
				// ocupa ahora mismo. Una division por cara y por fila, o
				// sea 800 como mucho por frame, y dentro del bucle de
				// pixeles solo queda una suma.
				//-------------------------------------------
				texture_step = ((long)TWISTER_FACE_WIDTH << DEMO_SHIFT) / (long)span;
				texture_u    = (long)(face * TWISTER_FACE_WIDTH) << DEMO_SHIFT;

				//-------------------------------------------
				// El recorte. Con el radio de arriba la columna no se sale
				// de la pantalla, pero el que suba TWISTER_RADIUS se
				// encontraria escribiendo fuera del buffer, y una x
				// negativa metida en un unsigned son 65.000 y pico.
				//-------------------------------------------
				if (left < 0){
					texture_u += texture_step * (long)(-left);
					left = 0;
				}
				if (right > DEMO_WIDTH){
					right = DEMO_WIDTH;
				}

				for (x = left; x < right; x++){

					screen[destination + (unsigned int)x] =
						image[source_row + (unsigned int)(texture_u >> DEMO_SHIFT)];

					texture_u += texture_step;

				}

			}

			destination += DEMO_WIDTH;

		}

		demo_show(screen);

		phase = (phase + TWISTER_SPIN) & DEMO_ANGLE_MASK;

	}

	return 1;

}
