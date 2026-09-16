//===========================================================
// CARPET - el rotozoom que no se acaba nunca
//
// El hermano del rotozoom, y la diferencia cabe en una linea: donde aquel
// pregunta "me he salido de la imagen?" y pinta negro, este hace
// "& 127" y vuelve a entrar por el otro lado.
//
// El resultado no se parece en nada. El rotozoom es UNA foto girando sobre
// un fondo negro; esto es un suelo infinito de fotos repetidas que gira y
// se acerca, sin bordes y sin fondo, llenando los 64.000 pixeles siempre.
// Es el efecto de "alfombra" de las intros, y es el que sale en el
// rotozoom de root42 del que esta copiada la idea.
//
//
// POR QUE HACE FALTA UNA TEXTURA Y NO VALE LA FOTO
//
// Para dar la vuelta con un AND, el ancho tiene que ser una potencia de
// dos. La foto es de 320x200 y ninguno de los dos numeros lo es, asi que
// envolver con ella costaria dos divisiones por pixel: 128.000 divisiones
// por frame, que en un 386 es no arrancar.
//
// Asi que al entrar se encoge la foto a 128x128, que si lo es, y a partir
// de ahi el bucle interior no tiene ni una division ni una comparacion:
//
//     sx = (u >> 8) & 127;
//     sy = (v >> 8) & 127;
//     screen[...] = texture[(sy << 7) + sx];
//
// 16.384 bytes y un encogido de una vez, a cambio de quitar la division
// del sitio por el que se pasa 64.000 veces por frame.
//
//
// EL PRECIO: SE VE LA COSTURA
//
// La foto no encaja consigo misma, asi que donde se repite se ve el corte.
// Eso no es un fallo, es lo que hace este efecto y lo que hacia el
// original: lo que se ensena es un mosaico, y un mosaico tiene juntas.
//===========================================================

#include "demos\demolib.h"
#include "demos\carpet\carpet.h"

// Cuanto se acerca y se aleja, en 8.8. Va de 0,5 a 2,5 aumentos: mas
// recorrido que el rotozoom, porque aqui alejarse no ensena negro sino mas
// baldosas, y cuantas mas se ven mejor se entiende que aquello se repite.
#define CARPET_SCALE_MID 	((DEMO_ONE * 3) / 2)	// 1.5
#define CARPET_SCALE_AMP 	(DEMO_ONE)				// +/- 1.0

// Velocidad de giro, de respiracion y de paseo, en pasos de angulo por
// frame. Los tres son primos entre si a proposito: asi el efecto tarda
// 256*3*5 frames en repetirse exactamente, que son mas de los 490 que
// dura, y nunca se ve el bucle.
#define CARPET_SPIN 		1
#define CARPET_BREATH 		3
#define CARPET_DRIFT 		5

// Cuanto se pasea el centro sobre la alfombra, en pixeles de textura
#define CARPET_DRIFT_RANGE 	96


//-----------------------------------------------------------
// EL SESGO, y es la unica trampa de todo el fichero.
//
// El AND que da la vuelta solo funciona si el numero es positivo. Y u y v
// se van a negativo constantemente: la esquina de arriba a la izquierda de
// la pantalla cae a la izquierda del centro de la textura casi siempre.
//
// En C89, lo que hace un >> con un numero negativo lo decide el
// compilador. Turbo C desplaza con signo y saldria bien, gcc tambien, y
// aun asi esto es exactamente la clase de cosa que funciona en el banco de
// Linux y hace otra cosa en el hierro.
//
// Asi que no se le pregunta a nadie: se les suma una constante que los
// deja positivos siempre. Y tiene que ser un MULTIPLO EXACTO del tamano de
// la textura en punto fijo (128 * 256 = 32.768), o desplazaria la
// alfombra medio azulejo.
//
//     128 * 256 * 64 = 2.097.152 = 32.768 * 64     exacto
//
// 64 texturas de margen contra un recorrido maximo de 320 pixeles por
// 2,5 aumentos = 800, que son 6 texturas. Sobra de largo.
//-----------------------------------------------------------
#define CARPET_BIAS 	((long)DEMO_TEXTURE_SIZE * (long)DEMO_ONE * 64L)


int demo_carpet(unsigned char *image,
                unsigned char *screen,
                unsigned char *palette,
                unsigned long end_tick)
{
	unsigned char *texture;

	int angle;
	int breath;
	int drift;
	long scale;
	long du_dx, dv_dx;
	long du_dy, dv_dy;
	long row_u, row_v;
	long u, v;
	long center_u, center_v;
	int x, y;
	unsigned int destination;
	unsigned int source;

	(void)palette;

	//-------------------------------------------------------
	// La textura. Se pide al entrar y se suelta al salir, dentro de la
	// misma llamada: por encima de los tres bloques de demo_run() y por
	// debajo de nada, o sea que se devuelve en el orden contrario al que
	// se pidio y no deja agujero.
	//
	// Un array estatico seria mas comodo y costaria 16.384 bytes del .exe
	// SIEMPRE, se pida la intro o no. Ver demos.h: sin /demo no existe
	// nada, y eso incluye esto.
	//-------------------------------------------------------
	texture = (unsigned char *)malloc(DEMO_TEXTURE_BYTES);

	if (texture == NULL){

		// Sin textura no hay efecto, pero tampoco se va a dejar la
		// diapositiva en negro: se ensena la foto quieta lo que durase.
		while (demo_now() < end_tick){

			if (demo_escape_pressed() == 1){
				return 0;
			}

			memcpy(screen, image, DEMO_SCREEN);
			demo_show(screen);

		}

		return 1;

	}

	demo_shrink_to_texture(image, texture);

	angle  = 0;
	breath = 0;
	drift  = 0;

	while (demo_now() < end_tick){

		if (demo_escape_pressed() == 1){
			free(texture);
			return 0;
		}

		scale = (long)CARPET_SCALE_MID
		      + (((long)demo_sin(breath) * (long)CARPET_SCALE_AMP) >> DEMO_SHIFT);

		if (scale < 32){
			scale = 32;
		}

		//---------------------------------------------------
		// Los cuatro incrementos, una vez por frame. Identico al rotozoom:
		// la cuenta es lineal, asi que avanzar un pixel a la derecha suma
		// siempre lo mismo a u y a v.
		//---------------------------------------------------
		du_dx = ((long)demo_cos(angle) * scale) >> DEMO_SHIFT;
		dv_dx = ((long)demo_sin(angle) * scale) >> DEMO_SHIFT;
		du_dy = -dv_dx;
		dv_dy =  du_dx;

		//---------------------------------------------------
		// Y el centro se pasea, que es lo que convierte "una foto girando"
		// en "un suelo por el que se vuela". Dos senos a distinta
		// velocidad dan una figura de Lissajous, igual que en bounce.
		//---------------------------------------------------
		center_u = ((long)(DEMO_TEXTURE_SIZE / 2) << DEMO_SHIFT)
		         + (((long)demo_sin(drift) * (long)CARPET_DRIFT_RANGE));
		center_v = ((long)(DEMO_TEXTURE_SIZE / 2) << DEMO_SHIFT)
		         + (((long)demo_cos(drift * 2) * (long)CARPET_DRIFT_RANGE));

		row_u = center_u + CARPET_BIAS
		      - (du_dx * (DEMO_WIDTH / 2)) - (du_dy * (DEMO_HEIGHT / 2));
		row_v = center_v + CARPET_BIAS
		      - (dv_dx * (DEMO_WIDTH / 2)) - (dv_dy * (DEMO_HEIGHT / 2));

		destination = 0;

		for (y = 0; y < DEMO_HEIGHT; y++){

			u = row_u;
			v = row_v;

			//-----------------------------------------------
			// Y aqui no hay ni un if. Ni recorte, ni tramo visible, ni
			// memset de los bordes: TODOS los pixeles de la pantalla caen
			// dentro de la textura, porque el AND se encarga de que caigan.
			//
			// Es la diferencia con el rotozoom, que se gasta media funcion
			// en calcular donde empieza y acaba lo visible de cada fila.
			//-----------------------------------------------
			for (x = 0; x < DEMO_WIDTH; x++){

				//-------------------------------------------
				// La fila de la textura, en DOS operaciones en vez de
				// tres.
				//
				// Lo que hace falta es ((v >> 8) & 127) << 7. Pero bajar
				// 8 y subir 7 es bajar 1, y los siete bits de abajo que
				// eso deja sucios los limpia la misma mascara si se
				// corre siete sitios:
				//
				//     ((v >> 8) & 127) << 7   ==   (v >> 1) & 0x3F80
				//
				// Un desplazamiento menos por pixel, y son 64.000 pixeles
				// por frame. Funciona porque v es SIEMPRE positivo, que es
				// lo que compra el sesgo de arriba.
				//-------------------------------------------
				source = (unsigned int)((v >> 1) & 0x3F80L)
				       + (unsigned int)((u >> DEMO_SHIFT) & DEMO_TEXTURE_MASK);

				screen[destination + x] = texture[source];

				u += du_dx;
				v += dv_dx;

			}

			destination += DEMO_WIDTH;

			row_u += du_dy;
			row_v += dv_dy;

		}

		demo_show(screen);

		angle  = (angle  + CARPET_SPIN)   & DEMO_ANGLE_MASK;
		breath = (breath + CARPET_BREATH) & DEMO_ANGLE_MASK;
		drift  = (drift  + CARPET_DRIFT)  & DEMO_ANGLE_MASK;

	}

	free(texture);

	return 1;

}
