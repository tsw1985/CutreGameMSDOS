//===========================================================
// SPHERE - la foto envuelta en una bola que gira
//
// Un globo terraqueo con tu foto de mapa, girando sobre negro. Es el otro
// gran efecto de tabla de la epoca junto con el tunel, y sale del mismo
// razonamiento: la forma no cambia nunca, solo cambia cuanto ha girado.
//
//
// LA IDEA, QUE ES LA DEL TUNEL
//
// Para cada pixel de la pantalla hay que saber dos cosas de la bola:
//
//     latitud    a que altura del globo cae   ->  asin(dy / R)
//     longitud   por donde da la vuelta       ->  asin(dx / ancho_de_esa_fila)
//
// Dos arcosenos por pixel, que es tan imposible como las raices del tunel.
// Y otra vez: ninguno de los dos depende del tiempo. Se calculan al entrar
// y el frame se queda en una suma y un AND.
//
//     screen[i] = texture[ (latitud[i] << 7) + ((longitud[i] + giro) & 127) ]
//
//
// PERO AQUI CUESTA LA MITAD QUE EL TUNEL, Y ESO TIENE GRACIA
//
// El tunel necesita dos tablas de 64.000 bytes. Esta solo una, y el motivo
// merece pararse:
//
//     LA LATITUD DE UN PIXEL DEPENDE SOLO DE SU FILA.
//
// Todos los pixeles de la fila 40 estan a la misma altura del globo, se
// miren por donde se miren. O sea que la latitud son DOSCIENTOS numeros,
// uno por fila, y no 64.000.
//
// La longitud si es por pixel, porque depende de lo ancho que sea el globo
// a esa altura: cerca del polo, dos pixeles separados por poco abarcan
// muchos grados.
//
// 64.600 bytes en vez de 128.000, y de paso el bucle interior se ahorra
// una lectura de tabla y un desplazamiento por pixel, porque la fila de
// textura se calcula una sola vez por fila.
//
//
// EL CONTORNO SE RESUELVE UNA VEZ Y NO SE VUELVE A MIRAR
//
// La bola es un circulo, asi que cada fila de la pantalla la corta en UN
// trozo seguido. Se guarda donde empieza y donde acaba cada fila y el
// bucle de pintar no pregunta nunca si esta dentro: lo de fuera se hace
// con memset. Es el mismo argumento del tramo visible del rotozoom.
//===========================================================

#include "demos\demolib.h"
#include "demos\sphere\sphere.h"

// El radio de la bola en pixeles, medido a lo ancho
#define SPHERE_RADIUS 		88

// La vuelta entera son 128 pasos, como en el tunel y por lo mismo: 128 es
// el ancho de la textura, asi que la longitud YA ES la columna.
#define SPHERE_TURN 		128
#define SPHERE_QUARTER 		32
#define SPHERE_MASK 		127

// Lo que gira por frame
#define SPHERE_SPIN 		1

// El pixel del modo 13h es un 20% mas alto que ancho (ver cube.c), asi que
// un circulo de radio igual en las dos direcciones sale con forma de
// huevo. La y se estira 6/5 ANTES de entrar en las cuentas, y lo que se
// dibuja vuelve a salir redondo.
#define SPHERE_ASPECT_NUM 	6
#define SPHERE_ASPECT_DEN 	5


//-----------------------------------------------------------
// EL ARCOSENO, de 0 a un cuarto de vuelta.
//
// sphere_asin[k] es el angulo, en pasos de los 128 que da la vuelta, cuyo
// seno vale k/256. Va de 0 a 32, que es el cuarto de vuelta.
//
// Fijate en la cola: ..., 29, 29, 30, 32. No es un error de la tabla, es
// el arcoseno: cerca de 1 la curva se pone vertical y un cambio diminuto
// en el seno son muchos grados. En la bola eso cae justo en el borde, que
// es donde la textura se comprime hasta desaparecer y nadie lo nota.
//-----------------------------------------------------------
static int sphere_asin[257] = {
	 0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,
	 1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
	 2,  2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  3,  3,
	 3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4,
	 4,  4,  4,  4,  4,  5,  5,  5,  5,  5,  5,  5,  5,
	 5,  5,  5,  5,  6,  6,  6,  6,  6,  6,  6,  6,  6,
	 6,  6,  6,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
	 7,  7,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
	 9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9, 10,
	10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11,
	11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12,
	12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13,
	13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 15,
	15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 17, 17, 17, 17, 17, 17, 17, 17, 18,
	18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19,
	19, 19, 20, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21,
	21, 21, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23,
	23, 24, 24, 24, 24, 25, 25, 25, 25, 25, 26, 26, 26,
	27, 27, 27, 28, 28, 28, 29, 29, 30, 32
};


//-----------------------------------------------------------
// La raiz cuadrada entera, bit a bit. La misma del tunel, y esta aqui su
// propia copia por lo mismo que demolib.h declara a mano las funciones de
// bmp.c: un efecto es una carpeta que se puede llevar suelta.
//-----------------------------------------------------------
static int sphere_isqrt(long value){

	long rest;
	long root;
	long bit;

	rest = value;
	root = 0;
	bit  = 1L << 16;

	while (bit > rest){
		bit >>= 2;
	}

	while (bit != 0){

		if (rest >= root + bit){
			rest -= root + bit;
			root  = (root >> 1) + bit;
		}else{
			root >>= 1;
		}

		bit >>= 2;

	}

	return (int)root;

}


int demo_sphere(unsigned char *image,
                unsigned char *screen,
                unsigned char *palette,
                unsigned long end_tick)
{
	unsigned char *texture;
	unsigned char *longitude;

	unsigned char row_latitude[DEMO_HEIGHT];
	int first[DEMO_HEIGHT];
	int last[DEMO_HEIGHT];

	int x, y;
	int dx, dy;
	int stretched;
	int half;
	int value;
	int angle;
	int spin;
	int step;
	unsigned int row_base;
	unsigned int destination;
	unsigned int index;

	(void)palette;

	texture   = (unsigned char *)malloc(DEMO_TEXTURE_BYTES);
	longitude = (unsigned char *)malloc(DEMO_SCREEN);

	if (texture == NULL || longitude == NULL){

		if (longitude != NULL){
			free(longitude);
		}
		if (texture != NULL){
			free(texture);
		}

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

	//-------------------------------------------------------
	// LAS TABLAS
	//-------------------------------------------------------
	for (y = 0; y < DEMO_HEIGHT; y++){

		dy = y - (DEMO_HEIGHT / 2);

		// La y estirada, para que la bola salga redonda en un pixel que no
		// lo es. A partir de aqui se razona como si fuera un circulo.
		stretched = (dy * SPHERE_ASPECT_NUM) / SPHERE_ASPECT_DEN;

		if (stretched > SPHERE_RADIUS || stretched < -SPHERE_RADIUS){

			// Esta fila no toca la bola
			first[y]        = 1;
			last[y]         = 0;
			row_latitude[y] = 0;
			continue;

		}

		//---------------------------------------------------
		// Lo ancho que es el globo a esta altura. En el ecuador es el
		// radio entero y en los polos es cero.
		//---------------------------------------------------
		half = sphere_isqrt(((long)SPHERE_RADIUS * SPHERE_RADIUS)
		                  - ((long)stretched * stretched));

		first[y] = (DEMO_WIDTH / 2) - half;
		last[y]  = (DEMO_WIDTH / 2) + half;

		//---------------------------------------------------
		// LA LATITUD DE LA FILA. Un solo numero para las 320 columnas.
		//
		// asin(dy/R) da de -32 a 32, que es media vuelta: los 180 grados
		// que van de polo a polo. Subirle 32 lo deja en 0..64 y doblarlo
		// lo reparte por las 128 filas de la textura.
		//---------------------------------------------------
		value = stretched;
		if (value < 0){
			value = -value;
		}

		angle = sphere_asin[(value * 256) / SPHERE_RADIUS];

		if (stretched < 0){
			angle = -angle;
		}

		value = (angle + SPHERE_QUARTER) * 2;

		if (value > SPHERE_MASK){
			value = SPHERE_MASK;
		}

		row_latitude[y] = (unsigned char)value;

		//---------------------------------------------------
		// Y LA LONGITUD, esta si pixel a pixel.
		//---------------------------------------------------
		destination = demo_row[y];

		for (x = first[y]; x <= last[y]; x++){

			dx = x - (DEMO_WIDTH / 2);

			if (half == 0){

				// Fila del polo: un solo pixel, y todas las longitudes
				// valen lo mismo ahi. Sin esto se dividiria entre cero.
				angle = 0;

			}else{

				value = dx;
				if (value < 0){
					value = -value;
				}

				angle = sphere_asin[(value * 256) / half];

				if (dx < 0){
					angle = -angle;
				}

			}

			longitude[destination + (unsigned int)x] =
				(unsigned char)((angle + SPHERE_QUARTER) & SPHERE_MASK);

		}

		// Alimentar la tarjeta: 200 raices y 24.000 arcosenos no son
		// gratis, y el buffer del DMA no espera.
		if ((y % 16) == 0){
			demo_sound();
		}

	}

	spin = 0;

	while (demo_now() < end_tick){

		if (demo_escape_pressed() == 1){
			free(longitude);
			free(texture);
			return 0;
		}

		step        = spin & SPHERE_MASK;
		destination = 0;

		for (y = 0; y < DEMO_HEIGHT; y++){

			if (last[y] < first[y]){

				memset(screen + destination, 0, DEMO_WIDTH);

			}else{

				//-------------------------------------------
				// La fila de textura, UNA vez para las 320 columnas. Esto
				// es lo que compra separar la latitud de la longitud.
				//-------------------------------------------
				row_base = (unsigned int)row_latitude[y] << DEMO_TEXTURE_SHIFT;

				if (first[y] > 0){
					memset(screen + destination, 0, (unsigned int)first[y]);
				}
				if (last[y] < DEMO_WIDTH - 1){
					memset(screen + destination + last[y] + 1, 0,
					       (unsigned int)(DEMO_WIDTH - 1 - last[y]));
				}

				index = destination + (unsigned int)first[y];

				for (x = first[y]; x <= last[y]; x++){

					screen[index] = texture[row_base
					              + (unsigned int)((longitude[index] + step) & SPHERE_MASK)];

					index++;

				}

			}

			destination += DEMO_WIDTH;

		}

		demo_show(screen);

		spin += SPHERE_SPIN;

	}

	free(longitude);
	free(texture);

	return 1;

}
