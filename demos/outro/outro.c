//===========================================================
// OUTRO - la pantalla de despedida
//
// La ultima cosa que se ve antes de jugar: un cartel que respira
// suavemente y espera a que pulses una tecla.
//
//
// PARA QUE SIRVE, QUE NO ES DECORACION
//
// Sin esto, la intro se apaga y el juego arranca en el mismo instante. En
// una presentacion delante de gente eso es un salto: no da tiempo a decir
// "bueno, pues esto es el juego" ni a que nadie se prepare.
//
// El epilogo convierte ese corte en una parada: la intro acaba, aparece el
// cartel, y el juego empieza cuando TU quieres.
//
//
// POR QUE EL ZOOM ES TAN CORTO
//
// El resto de la carpeta deforma la imagen a base de bien, que es de lo
// que va. Aqui no se puede: esto es un cartel y un cartel hay que poder
// LEERLO. Un rotozoom sobre un texto lo hace ilegible en dos frames.
//
// Asi que se coge el zoom, que es la unica deformacion que NO retuerce
// nada -todo crece igual en las dos direcciones, las letras siguen siendo
// letras- y se le recorta el recorrido a casi nada: de 1,00 a 1,09
// aumentos. Lo justo para que la pantalla no parezca congelada.
//
//
// Y SOLO HACIA DENTRO, NUNCA HACIA FUERA
//
// La escala nunca pasa de 256 (el 1:1). Si se alejara, la imagen ocuparia
// menos de 320 pixeles y apareceria un marco negro pulsando alrededor del
// cartel, que es justo el tipo de cosa que distrae de lo que pone.
//
// Acercandose siempre, la pantalla esta llena siempre, no hay bordes que
// rellenar y el bucle interior se queda sin un solo memset. El zoom
// normal se gasta media funcion en eso.
//===========================================================

#include "demos\demolib.h"
#include "demos\outro\outro.h"

//-----------------------------------------------------------
// De 1,00 a 1,09 aumentos. La escala es el paso de origen por pixel de
// pantalla en 8.8: 256 es 1:1 y menos de 256 agranda.
//
// 234 son 256/234 = 1,094 aumentos. Y ese numero no se puede subir a la
// ligera: el texto de fin.bmp esta dibujado dentro de una zona segura, y a
// 1,10 aumentos solo se ve el 91% central de la imagen. Mas zoom empieza a
// comerse el marco y despues las letras.
//-----------------------------------------------------------
#define OUTRO_SCALE_MID 	245
#define OUTRO_SCALE_AMP 	11

// Lo despacio que respira: un ciclo entero cada 256 frames, casi cuatro
// segundos. Es un cartel, no un efecto.
#define OUTRO_BREATH 		1


int demo_outro(unsigned char *image,
               unsigned char *screen,
               unsigned char *palette)
{
	//-------------------------------------------------------
	// La tabla de columnas del zoom: en un zoom sin giro, la columna de
	// origen de un pixel depende SOLO de su x, asi que son 320 cuentas por
	// frame y no 64.000. Ver zoom.c, de donde sale la idea.
	//-------------------------------------------------------
	unsigned int column[DEMO_WIDTH];

	int breath;
	long scale;
	long start_u, start_v;
	long u, v;
	int x, y;
	int sx, sy;
	unsigned int destination;
	unsigned int source_row;

	(void)palette;

	breath = 0;

	while (demo_any_key_pressed() == 0){

		scale = (long)OUTRO_SCALE_MID
		      + (((long)demo_sin(breath) * (long)OUTRO_SCALE_AMP) >> DEMO_SHIFT);

		//---------------------------------------------------
		// La esquina de arriba a la izquierda, medida desde el centro,
		// para que el zoom sea alrededor del centro del cartel y no de su
		// esquina.
		//---------------------------------------------------
		start_u = ((long)(DEMO_WIDTH  / 2) << DEMO_SHIFT) - (scale * (DEMO_WIDTH  / 2));
		start_v = ((long)(DEMO_HEIGHT / 2) << DEMO_SHIFT) - (scale * (DEMO_HEIGHT / 2));

		u = start_u;

		for (x = 0; x < DEMO_WIDTH; x++){

			sx = (int)(u >> DEMO_SHIFT);

			//-----------------------------------------------
			// El recorte no hace falta con las constantes de arriba: con
			// la escala siempre en 234..256, sx se queda entre 13 y 306.
			//
			// Esta de todas formas, y son 320 comparaciones por frame
			// contra una pantalla llena de basura el dia que alguien suba
			// OUTRO_SCALE_AMP sin echar la cuenta. Aqui la comprobacion
			// cuesta 320 y en el bucle de pixeles costaria 64.000: este es
			// el sitio donde ponerla.
			//-----------------------------------------------
			if (sx < 0){
				sx = 0;
			}
			if (sx > DEMO_WIDTH - 1){
				sx = DEMO_WIDTH - 1;
			}

			column[x] = (unsigned int)sx;

			u += scale;

		}

		destination = 0;
		v = start_v;

		for (y = 0; y < DEMO_HEIGHT; y++){

			sy = (int)(v >> DEMO_SHIFT);

			if (sy < 0){
				sy = 0;
			}
			if (sy > DEMO_HEIGHT - 1){
				sy = DEMO_HEIGHT - 1;
			}

			source_row = demo_row[sy];

			//-----------------------------------------------
			// Y aqui no hay ni un if ni un memset: acercandose siempre, la
			// pantalla se llena entera.
			//-----------------------------------------------
			for (x = 0; x < DEMO_WIDTH; x++){
				screen[destination + x] = image[source_row + column[x]];
			}

			destination += DEMO_WIDTH;
			v += scale;

		}

		demo_show(screen);

		breath = (breath + OUTRO_BREATH) & DEMO_ANGLE_MASK;

	}

	return 1;

}
