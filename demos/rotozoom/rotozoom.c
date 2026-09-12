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


//-----------------------------------------------------------
// EN QUE TRAMO DE x SE CUMPLE   0 <= inicio + x*paso < limite
//
// Esta funcioncita es toda la optimizacion.
//
// El valor crece (o decrece) en linea recta con x, asi que los x que
// cumplen la condicion son UN TRAMO SEGUIDO: un principio y un final, sin
// agujeros en medio. En vez de preguntarlo 320 veces por fila, se calcula
// donde empieza y donde acaba, y dentro ya no hay nada que preguntar.
//
// Todas las divisiones son de positivo entre positivo, a proposito: en C89
// el redondeo de una division con negativos queda a gusto del compilador,
// y eso es exactamente el tipo de cosa que funciona en gcc y hace otra
// cosa en Turbo C. Cuando el paso es negativo se le da la vuelta al signo
// y se resuelve el mismo problema al reves.
//
// Devuelve 0 si no hay ningun x que valga.
//-----------------------------------------------------------
static int rotozoom_span(long start, long step, long limit,
                         int *out_first, int *out_last)
{
	long first;
	long last;
	long down;

	if (step == 0){

		// No se mueve: o vale toda la fila o no vale ninguna
		if (start >= 0 && start < limit){
			*out_first = 0;
			*out_last  = DEMO_WIDTH - 1;
			return 1;
		}
		return 0;

	}

	if (step > 0){

		// Sube. Entra por abajo y sale por arriba.
		if (start >= 0){
			first = 0;
		}else{
			// el primer x con start + x*step >= 0, redondeando hacia arriba
			first = ((-start) + step - 1) / step;
		}

		if (start > limit - 1){
			return 0;			// ya empieza pasado del limite y sigue subiendo
		}
		last = (limit - 1 - start) / step;

	}else{

		// Baja. Se le da la vuelta al paso para dividir en positivo.
		down = -step;

		if (start < 0){
			return 0;			// ya empieza por debajo y sigue bajando
		}
		last = start / down;

		if (start <= limit - 1){
			first = 0;
		}else{
			first = ((start - (limit - 1)) + down - 1) / down;
		}

	}

	if (first < 0){
		first = 0;
	}
	if (last > DEMO_WIDTH - 1){
		last = DEMO_WIDTH - 1;
	}
	if (first > last){
		return 0;
	}

	*out_first = (int)first;
	*out_last  = (int)last;

	return 1;

}


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

	int first_u, last_u;
	int first_v, last_v;
	int first, last;
	int visible;

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

			//-----------------------------------------------
			// EL TRAMO VISIBLE DE ESTA FILA.
			//
			// La imagen girada es un cuadrilatero, y una recta horizontal
			// lo corta en UN solo trozo. Ese trozo es la interseccion del
			// tramo donde u esta dentro con el tramo donde lo esta v.
			//
			// Lo de fuera se pinta con memset, que mueve bytes de 2 en 2 o
			// de 4 en 4, en vez de con el bucle de arriba. Y en un
			// rotozoom con la imagen girada y alejada, lo de fuera son la
			// mayoria de los pixeles de la pantalla.
			//-----------------------------------------------
			visible = 0;
			first   = 0;
			last    = -1;

			if (rotozoom_span(row_u, du_dx, (long)DEMO_WIDTH  << DEMO_SHIFT, &first_u, &last_u) == 1){
				if (rotozoom_span(row_v, dv_dx, (long)DEMO_HEIGHT << DEMO_SHIFT, &first_v, &last_v) == 1){

					first = first_u;
					if (first_v > first){
						first = first_v;
					}

					last = last_u;
					if (last_v < last){
						last = last_v;
					}

					if (first <= last){
						visible = 1;
					}

				}
			}

			if (visible == 0){

				memset(screen + destination, 0, DEMO_WIDTH);

			}else{

				if (first > 0){
					memset(screen + destination, 0, first);
				}
				if (last < DEMO_WIDTH - 1){
					memset(screen + destination + last + 1, 0, DEMO_WIDTH - 1 - last);
				}

				// Saltar de golpe a donde empieza lo visible
				u = row_u + (du_dx * first);
				v = row_v + (dv_dx * first);

				//-------------------------------------------
				// Y aqui dentro ya no se comprueba nada: por construccion
				// del tramo, todos estos pixeles caen dentro de la imagen.
				//-------------------------------------------
				for (x = first; x <= last; x++){

					sx = (int)(u >> DEMO_SHIFT);
					sy = (int)(v >> DEMO_SHIFT);

					screen[destination + x] = image[demo_row[sy] + (unsigned int)sx];

					u += du_dx;
					v += dv_dx;

				}

			}

			destination += DEMO_WIDTH;

			row_u += du_dy;
			row_v += dv_dy;

		}

		demo_show(screen);

		angle  = (angle  + ROTOZOOM_SPIN)   & DEMO_ANGLE_MASK;
		breath = (breath + ROTOZOOM_BREATH) & DEMO_ANGLE_MASK;

	}

	return 1;

}
