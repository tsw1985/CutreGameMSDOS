//===========================================================
// TUNNEL - el tunel infinito
//
// Volar por dentro de un tubo forrado con la foto, sin salir nunca. Es EL
// efecto de las intros de los 90 junto con el rotozoom, y el que mejor
// ensena la idea que sostiene a media carpeta: cuando una cuenta es cara y
// no cambia, no se optimiza, se PRECALCULA.
//
//
// LA CUENTA QUE NO SE PUEDE HACER
//
// Para cada pixel de la pantalla hay que saber dos cosas del tubo:
//
//     angulo      donde cae alrededor del tubo   ->  atan2(dy, dx)
//     profundidad lo lejos que esta              ->  k / raiz(dx2 + dy2)
//
// Una arcotangente y una raiz cuadrada por pixel. Son 64.000 pixeles por
// frame a 70 frames por segundo: cuatro millones y medio de raices por
// segundo en un 386. No es que sea lento, es que no existe.
//
//
// LA IDEA: NINGUNA DE LAS DOS CAMBIA NUNCA
//
// El angulo y la profundidad de un pixel dependen de DONDE ESTA EL PIXEL,
// y el pixel no se mueve. Lo unico que cambia con el tiempo es por donde
// vamos del tubo y cuanto hemos girado, y eso son dos numeros para toda la
// pantalla, no dos numeros por pixel.
//
// Asi que se calculan las 128.000 cuentas UNA vez al entrar, se guardan en
// dos tablas de un byte por pixel, y a partir de ahi cada frame es:
//
//     screen[i] = texture[ ((depth[i] + avance) & 127) << 7
//                        + ((angle[i] + giro)   & 127) ];
//
// Dos lecturas de tabla, dos sumas, dos AND y una lectura de textura. Ni
// una raiz, ni una division, ni un producto. El movimiento entero del
// tunel son las dos variables "avance" y "giro".
//
// Cuesta 128.000 bytes de memoria, y ese es el trato: memoria por tiempo.
// Es el mismo trato que hace demo_row[] en demolib.h, solo que a lo bestia.
//
//
// Y LAS TABLAS SE MONTAN SIN math.h
//
// Aqui no entra ni un float, asi que la raiz es una raiz entera bit a bit
// y la arcotangente es una tabla de 65 numeros escrita a mano. Las dos
// estan explicadas donde se usan.
//===========================================================

#include "demos\demolib.h"
#include "demos\tunnel\tunnel.h"

// La vuelta entera al tubo son 128 pasos, no 256 como los angulos del
// resto de la carpeta, y no es un capricho: 128 es el ancho de la textura,
// asi que el angulo YA ES la columna de textura y no hay que escalarlo.
#define TUNNEL_TURN 		128
#define TUNNEL_QUARTER 		32
#define TUNNEL_MASK 		127

// Lo profundo que se ve. Sale de dividir esto entre el radio en pixeles:
// cerca del centro el radio es diminuto y la profundidad se dispara, que
// es exactamente lo que hace que el centro parezca infinitamente lejos.
//
// Con 3072 el borde de la pantalla cae sobre la profundidad 16 y el centro
// da mas de veinte vueltas a la textura: se ven muchos anillos apretados
// en el fondo y pocos y grandes en los bordes. Subirlo estira el tubo.
#define TUNNEL_DEPTH 		3072

// Lo que se avanza y se gira por frame. Que sean distintos y primos entre
// si es lo que hace que no se vea repetirse.
#define TUNNEL_SPEED 		2
#define TUNNEL_SPIN 		1

// Cada cuantas filas se alimenta la tarjeta mientras se montan las tablas.
// Las tablas tardan lo suyo y son 64.000 raices: sin esto, la musica se
// quedaria en bucle durante el montaje. Es la misma razon por la que
// demos.c llama a demo_sound() a los dos lados de cargar un BMP.
#define TUNNEL_SOUND_ROWS 	16


//-----------------------------------------------------------
// LA ARCOTANGENTE, DE 0 A UN OCTAVO DE VUELTA
//
// atan_table[k] es el angulo, en pasos de los 128 que da la vuelta, del
// punto cuya razon "lado corto / lado largo" es k/64.
//
// Solo hace falta un OCTAVO de vuelta: 16 de los 128 pasos. Todo lo demas
// sale por simetria, porque un circulo es el mismo octavo repetido ocho
// veces dado la vuelta. Eso convierte una funcion trascendente en 65
// numeros y cuatro comparaciones de signo.
//
// Los valores son round(atan(k/64) / 2pi * 128).
//-----------------------------------------------------------
static int tunnel_atan[65] = {
	  0,   0,   1,   1,   1,   2,   2,   2,   3,   3,   3,   3,   4,
	  4,   4,   5,   5,   5,   6,   6,   6,   6,   7,   7,   7,   8,
	  8,   8,   8,   9,   9,   9,   9,  10,  10,  10,  10,  11,  11,
	 11,  11,  12,  12,  12,  12,  12,  13,  13,  13,  13,  14,  14,
	 14,  14,  14,  14,  15,  15,  15,  15,  15,  16,  16,  16,  16
};


//-----------------------------------------------------------
// LA RAIZ CUADRADA ENTERA, bit a bit.
//
// El metodo de la escuela en base 2: se prueba a poner un bit en la raiz
// empezando por el mas alto, y se queda el que no se pase. Son 8 vueltas
// para un numero de 16 bits, contra las 189 que daria el "ir sumando uno
// hasta pasarse".
//
// Solo se llama al montar las tablas, o sea 64.000 veces en toda la vida
// del efecto y ni una durante un frame.
//-----------------------------------------------------------
static int tunnel_isqrt(long value){

	long rest;
	long root;
	long bit;

	rest = value;
	root = 0;
	bit  = 1L << 16;		// el mayor valor posible es 160*160 + 100*100 = 35600

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


//-----------------------------------------------------------
// EL ANGULO DE UN PUNTO, en pasos de los 128 de la vuelta.
//
// Se reduce al primer octavo, se mira en la tabla, y se deshace la
// reduccion con el cuadrante. Las dos divisiones son entre el lado LARGO,
// siempre, que es lo que garantiza que la razon cae en 0..64 y que no se
// divide entre cero salvo en el centro exacto, que se resuelve aparte.
//-----------------------------------------------------------
static int tunnel_angle(int dx, int dy){

	int ax, ay;
	int base;

	ax = dx;
	if (ax < 0){
		ax = -ax;
	}

	ay = dy;
	if (ay < 0){
		ay = -ay;
	}

	if (ax == 0 && ay == 0){

		base = 0;

	}else if (ay <= ax){

		// primer octavo: la tabla vale tal cual
		base = tunnel_atan[(ay * 64) / ax];

	}else{

		// segundo octavo: es el primero visto del otro lado de la diagonal
		base = TUNNEL_QUARTER - tunnel_atan[(ax * 64) / ay];

	}

	// Y el cuadrante. 128 & 127 es 0, asi que el ultimo caso cierra solo.
	if (dx >= 0 && dy >= 0){
		return base;
	}
	if (dx < 0 && dy >= 0){
		return (TUNNEL_TURN / 2) - base;
	}
	if (dx < 0 && dy < 0){
		return (TUNNEL_TURN / 2) + base;
	}

	return (TUNNEL_TURN - base) & TUNNEL_MASK;

}


int demo_tunnel(unsigned char *image,
                unsigned char *screen,
                unsigned char *palette,
                unsigned long end_tick)
{
	unsigned char *texture;
	unsigned char *depth;
	unsigned char *angle;

	int x, y;
	int dx, dy;
	int radius;
	long squared;
	unsigned int offset;
	int advance;
	int spin;
	int step_depth;
	int step_angle;

	(void)palette;

	//-------------------------------------------------------
	// Tres bloques: la textura y las dos tablas. Las tablas son de 64.000
	// justos, que cabe en un malloc de 16 bits por 1.535 bytes.
	//
	// Y se piden los tres antes de tocar ninguno: si el tercero falla hay
	// que devolver los dos primeros, y devolverlos EN ORDEN. Un bloque que
	// se queda colgado aqui es un agujero en el monton, y el agujero lo
	// paga el farmalloc de 256.000 del mapa cuando arranca la partida.
	//-------------------------------------------------------
	texture = (unsigned char *)malloc(DEMO_TEXTURE_BYTES);
	depth   = (unsigned char *)malloc(DEMO_SCREEN);
	angle   = (unsigned char *)malloc(DEMO_SCREEN);

	if (texture == NULL || depth == NULL || angle == NULL){

		if (angle != NULL){
			free(angle);
		}
		if (depth != NULL){
			free(depth);
		}
		if (texture != NULL){
			free(texture);
		}

		// Sin tablas no hay tunel; la diapositiva se ensena quieta.
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
	// LAS TABLAS. Lo unico caro de todo el efecto, y pasa una vez.
	//-------------------------------------------------------
	offset = 0;

	for (y = 0; y < DEMO_HEIGHT; y++){

		dy = y - (DEMO_HEIGHT / 2);

		for (x = 0; x < DEMO_WIDTH; x++){

			dx = x - (DEMO_WIDTH / 2);

			squared = ((long)dx * (long)dx) + ((long)dy * (long)dy);
			radius  = tunnel_isqrt(squared);

			// El centro exacto tendria radio 0 y profundidad infinita. Se
			// le pone 1, que lo deja en el pixel mas lejano de todos.
			if (radius < 1){
				radius = 1;
			}

			depth[offset] = (unsigned char)((TUNNEL_DEPTH / radius) & TUNNEL_MASK);
			angle[offset] = (unsigned char)(tunnel_angle(dx, dy) & TUNNEL_MASK);

			offset++;

		}

		// La tarjeta de sonido no espera a que terminemos de pensar.
		if ((y % TUNNEL_SOUND_ROWS) == 0){
			demo_sound();
		}

	}

	advance = 0;
	spin    = 0;

	while (demo_now() < end_tick){

		if (demo_escape_pressed() == 1){
			free(angle);
			free(depth);
			free(texture);
			return 0;
		}

		step_depth = advance & TUNNEL_MASK;
		step_angle = spin    & TUNNEL_MASK;

		//---------------------------------------------------
		// EL FRAME ENTERO. Un bucle plano sobre los 64.000, sin filas y
		// sin columnas: las tablas ya tienen la forma de la pantalla, asi
		// que no hay nada que recalcular al cambiar de fila.
		//---------------------------------------------------
		for (offset = 0; offset < DEMO_SCREEN; offset++){

			screen[offset] = texture[
				(unsigned int)(((depth[offset] + step_depth) & TUNNEL_MASK) << DEMO_TEXTURE_SHIFT)
			  + (unsigned int)((angle[offset] + step_angle) & TUNNEL_MASK)
			];

		}

		demo_show(screen);

		advance += TUNNEL_SPEED;
		spin    += TUNNEL_SPIN;

	}

	free(angle);
	free(depth);
	free(texture);

	return 1;

}
