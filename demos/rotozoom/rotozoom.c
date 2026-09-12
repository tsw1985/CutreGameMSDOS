//===========================================================
// ROTOZOOM - girar y hacer zoom a la vez
//
// EL efecto de las intros de los 90. La imagen da vueltas sobre negro
// mientras se acerca y se aleja, y todo con enteros.
//
// La idea es al reves de lo que uno piensa. No se coge la imagen y se
// gira: se recorre la PANTALLA pixel a pixel y, para cada pixel de
// pantalla, se pregunta "de que punto de la imagen vengo yo". Asi no
// quedan agujeros: cada pixel de la pantalla se rellena exactamente una
// vez.
//
// La cuenta para un pixel (x,y) medido desde el centro es:
//
//     u =  x*cos(a)*escala + y*sin(a)*escala
//     v = -x*sin(a)*escala + y*cos(a)*escala
//
// Que son cuatro multiplicaciones por pixel, o sea 256.000 por frame. En
// un 8086 eso no va ni en broma.
//
// El truco de la epoca: la cuenta es LINEAL, asi que al avanzar un pixel a
// la derecha u y v siempre crecen lo mismo. Se calculan esos dos
// incrementos UNA vez por frame y dentro del bucle solo hay dos sumas:
//
//     u += du_dx;
//     v += dv_dx;
//
// De 256.000 multiplicaciones a 2 sumas por pixel.
//===========================================================

#include "demos\demolib.h"
#include "demos\rotozoom\rotozoom.h"

// Cuanto se acerca y se aleja: la escala va de 1/2 a 2 aumentos. Se guarda
// como el punto medio mas una amplitud, en punto fijo 8.8.
#define ROTOZOOM_SCALE_MID 	((DEMO_ONE * 5) / 4)	// 1.25
#define ROTOZOOM_SCALE_AMP 	((DEMO_ONE * 3) / 4)	// +/- 0.75

// Velocidad de giro y de respiracion, en pasos de angulo por frame
#define ROTOZOOM_SPIN 		2
#define ROTOZOOM_BREATH 	1


int demo_rotozoom(unsigned char *image,
                  unsigned char *screen,
                  unsigned char *palette,
                  unsigned long end_tick)
{
	int angle;
	int breath;
	long scale;
	long du_dx, dv_dx;
	long du_dy, dv_dy;
	long row_u, row_v;
	long u, v;
	int x, y;
	int sx, sy;
	unsigned int destination;

	// palette no se toca aqui: este efecto no juega con los colores
	(void)palette;

	angle  = 0;
	breath = 0;

	while (demo_now() < end_tick){

		if (demo_escape_pressed() == 1){
			return 0;
		}

		//---------------------------------------------------
		// La escala de este frame. demo_sin() ya viene en 8.8, asi que
		// multiplicar por la amplitud y bajar 8 bits deja otro 8.8.
		//---------------------------------------------------
		scale = (long)ROTOZOOM_SCALE_MID
		      + (((long)demo_sin(breath) * (long)ROTOZOOM_SCALE_AMP) >> DEMO_SHIFT);

		if (scale < 32){
			scale = 32;		// nunca 0: seria dividir el mundo entre nada
		}

		//---------------------------------------------------
		// Los cuatro incrementos, calculados UNA vez.
		//
		// Dos productos 8.8 seguidos darian 16.16, de ahi el >> DEMO_SHIFT
		// para dejarlo otra vez en 8.8.
		//---------------------------------------------------
		du_dx = ((long)demo_cos(angle) * scale) >> DEMO_SHIFT;
		dv_dx = ((long)demo_sin(angle) * scale) >> DEMO_SHIFT;
		du_dy = -dv_dx;
		dv_dy =  du_dx;

		//---------------------------------------------------
		// Donde cae la esquina de arriba a la izquierda de la pantalla.
		//
		// Se parte del centro de la imagen y se retrocede media pantalla
		// en las dos direcciones giradas. Sin esto, la imagen giraria
		// alrededor de su esquina en vez de su centro.
		//---------------------------------------------------
		row_u = ((long)(DEMO_WIDTH  / 2) << DEMO_SHIFT)
		      - (du_dx * (DEMO_WIDTH / 2)) - (du_dy * (DEMO_HEIGHT / 2));
		row_v = ((long)(DEMO_HEIGHT / 2) << DEMO_SHIFT)
		      - (dv_dx * (DEMO_WIDTH / 2)) - (dv_dy * (DEMO_HEIGHT / 2));

		destination = 0;

		for (y = 0; y < DEMO_HEIGHT; y++){

			u = row_u;
			v = row_v;

			for (x = 0; x < DEMO_WIDTH; x++){

				sx = (int)(u >> DEMO_SHIFT);
				sy = (int)(v >> DEMO_SHIFT);

				//-------------------------------------------
				// Fuera de la imagen se pinta negro.
				//
				// Las dos comparaciones son en unsigned a proposito: un sx
				// negativo se convierte en un numero enorme y falla el
				// "menor que 320" de una vez, asi que una sola comparacion
				// hace de las dos. Es el truco de siempre para recortar.
				//-------------------------------------------
				if ((unsigned int)sx < DEMO_WIDTH && (unsigned int)sy < DEMO_HEIGHT){
					screen[destination] = image[demo_row[sy] + (unsigned int)sx];
				}else{
					screen[destination] = 0;
				}

				destination++;

				u += du_dx;
				v += dv_dx;

			}

			row_u += du_dy;
			row_v += dv_dy;

		}

		demo_show(screen);

		angle  = (angle  + ROTOZOOM_SPIN)   & DEMO_ANGLE_MASK;
		breath = (breath + ROTOZOOM_BREATH) & DEMO_ANGLE_MASK;

	}

	return 1;

}
