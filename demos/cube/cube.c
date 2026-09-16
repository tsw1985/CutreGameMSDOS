//===========================================================
// CUBE - el cubo de alambre girando sobre la foto
//
// El primer efecto de la carpeta que es 3D de verdad: ocho vertices, doce
// aristas y una proyeccion en perspectiva, todo con enteros.
//
// Y es 3D barato a proposito. Un cubo de alambre no rellena nada: se
// calculan OCHO puntos por frame y se pintan doce rectas. Comparado con el
// rotozoom, que toca los 64.000 pixeles, esto es gratis. Lo caro de un 3D
// no son las matematicas, es el relleno; por eso los efectos 3D de la
// epoca empezaban todos por aqui.
//
// La foto se queda de fondo. Podria pintarse el cubo sobre negro, que es
// lo clasico, pero esta carpeta es una presentacion de fotos: ocho
// segundos de negro con un cubo encima serian ocho segundos sin ver la
// diapositiva.
//
//
// LA CADENA DE CUENTAS, de un vertice a un pixel
//
//   1. el cubo, ocho vertices en +/-1                    (constante)
//   2. girar en Y, luego en X                            (4 productos)
//   3. alejarlo, sumando a la z                          (1 suma)
//   4. dividir por la z: eso es la perspectiva           (2 divisiones)
//   5. escalar y centrar en la pantalla                  (2 sumas)
//
// Las divisiones son el unico sitio donde este efecto divide, y son 16 por
// frame. Un rotozoom no divide ni una vez, pero toca 64.000 pixeles: son
// dos formas opuestas de gastarse el presupuesto.
//===========================================================

#include "demos\demolib.h"
#include "demos\cube\cube.h"

// La distancia focal, en pixeles. Es lo que decide si el cubo se ve de
// cerca (perspectiva muy abierta, las caras se deforman mucho) o de lejos
// (casi una proyeccion plana). 200 deja un cubo que ocupa poco mas de un
// tercio de la pantalla de alto.
#define CUBE_FOCAL 			200

// Lo lejos que esta el centro del cubo, en 8.8. Cuatro veces su medio lado.
#define CUBE_DISTANCE 		(DEMO_ONE * 4)

// Pasos de angulo por frame en cada eje. Distintos a proposito: con el
// mismo numero en los dos el cubo gira siempre por la misma diagonal y se
// nota que se repite a los tres segundos.
#define CUBE_SPIN_X 		1
#define CUBE_SPIN_Y 		2

//-----------------------------------------------------------
// EL PIXEL NO ES CUADRADO.
//
// El modo 13h son 320x200 estirados sobre una pantalla de 4:3, o sea que
// cada pixel es un 20% mas alto que ancho. Si se proyecta la y con el
// mismo numero que la x, el "cubo" sale mas alto que ancho y parece una
// caja de zapatos de pie.
//
// 5/6 es 0,833, que es justo 1/1,2. Esto es lo unico de todo el efecto que
// no sale de la geometria: sale del monitor.
//-----------------------------------------------------------
#define CUBE_ASPECT_NUM 	5
#define CUBE_ASPECT_DEN 	6


//-----------------------------------------------------------
// EL COLOR DE LAS ARISTAS, y no se puede elegir a dedo.
//
// En un programa con la paleta por defecto del modo 13h, el 15 es blanco y
// se acabo. Aqui NO: cada foto trae su propia paleta de 256 colores, asi
// que el indice 15 de demo03.bmp es lo que le tocara a esa foto, que
// igual es marron.
//
// Asi que se busca. Los 768 bytes vienen en BGR y de 0 a 63, tal como los
// deja bmp_load_pallete_data(), y se elige el indice cuya suma de los tres
// canales sea mayor: el blanco de esa foto, sea cual sea su numero.
//
// El 0 no se mira, porque el 0 es negro por contrato en esta carpeta y es
// con lo que se borra el fondo.
//-----------------------------------------------------------
static unsigned char cube_brightest(unsigned char *palette){

	int index;
	int value;
	int best_value;
	int best_index;

	best_index = 1;
	best_value = -1;

	for (index = 1; index <= 255; index++){

		value = (int)palette[index * 3 + 0]
		      + (int)palette[index * 3 + 1]
		      + (int)palette[index * 3 + 2];

		if (value > best_value){
			best_value = value;
			best_index = index;
		}

	}

	return (unsigned char)best_index;

}


//-----------------------------------------------------------
// UNA RECTA, con el algoritmo de Bresenham.
//
// Nada de puntos flotantes ni de pendientes: se lleva un error acumulado y
// se decide con una comparacion si toca avanzar en x, en y o en las dos.
// Es de 1962 y sigue siendo la forma de pintar una recta con enteros.
//
// El recorte se hace pixel a pixel, mirando si cae dentro antes de
// escribir. Es lo mas lento posible y aqui da igual: son doce rectas de
// menos de 200 pixeles, no 64.000 pixeles. Recortar bien la recta ANTES de
// empezar seria mas codigo y no se notaria.
//
// Las coordenadas son int y no unsigned, y eso no es un descuido: un
// vertice del cubo se sale de la pantalla constantemente, y un -9 metido
// en un unsigned se convierte en 65527.
//-----------------------------------------------------------
static void cube_line(unsigned char *screen, int x0, int y0, int x1, int y1,
                      unsigned char color)
{
	int dx, dy;
	int step_x, step_y;
	int error, error2;

	dx = x1 - x0;
	if (dx < 0){
		dx = -dx;
	}

	dy = y1 - y0;
	if (dy < 0){
		dy = -dy;
	}

	if (x0 < x1){
		step_x = 1;
	}else{
		step_x = -1;
	}

	if (y0 < y1){
		step_y = 1;
	}else{
		step_y = -1;
	}

	error = dx - dy;

	while (1){

		if (x0 >= 0 && x0 < DEMO_WIDTH && y0 >= 0 && y0 < DEMO_HEIGHT){
			screen[demo_row[y0] + (unsigned int)x0] = color;
		}

		if (x0 == x1 && y0 == y1){
			return;
		}

		error2 = error * 2;

		if (error2 >= -dy){
			if (x0 == x1){
				return;
			}
			error -= dy;
			x0 += step_x;
		}

		if (error2 <= dx){
			if (y0 == y1){
				return;
			}
			error += dx;
			y0 += step_y;
		}

	}

}


int demo_cube(unsigned char *image,
              unsigned char *screen,
              unsigned char *palette,
              unsigned long end_tick)
{
	//-------------------------------------------------------
	// El cubo, en 8.8. Los ocho vertices son las ocho combinaciones de
	// +/-1, o sea +/-DEMO_ONE.
	//
	// Y las doce aristas son pares de indices. Escritas asi, en una tabla,
	// el bucle de pintar son cuatro lineas y no doce llamadas a mano.
	//-------------------------------------------------------
	static int corner_x[8] = { -DEMO_ONE, -DEMO_ONE,  DEMO_ONE,  DEMO_ONE,
	                           -DEMO_ONE, -DEMO_ONE,  DEMO_ONE,  DEMO_ONE };
	static int corner_y[8] = { -DEMO_ONE,  DEMO_ONE, -DEMO_ONE,  DEMO_ONE,
	                           -DEMO_ONE,  DEMO_ONE, -DEMO_ONE,  DEMO_ONE };
	static int corner_z[8] = { -DEMO_ONE, -DEMO_ONE, -DEMO_ONE, -DEMO_ONE,
	                            DEMO_ONE,  DEMO_ONE,  DEMO_ONE,  DEMO_ONE };

	static int edge[24] = {
		0,1,  1,3,  3,2,  2,0,
		1,5,  0,4,  2,6,  3,7,
		4,5,  5,7,  7,6,  6,4
	};

	int screen_x[8];
	int screen_y[8];

	int angle_x, angle_y;
	int sin_x, cos_x, sin_y, cos_y;
	long rotated_x, rotated_y, rotated_z;
	long temporary_y, temporary_z;
	int index;
	unsigned char color;

	color = cube_brightest(palette);

	angle_x = 0;
	angle_y = 0;

	while (demo_now() < end_tick){

		if (demo_escape_pressed() == 1){
			return 0;
		}

		//---------------------------------------------------
		// Los cuatro senos del frame, UNA vez.
		//
		// Dentro del bucle de vertices se usan ocho veces cada uno; leer
		// la tabla ocho veces no es caro, pero escribirlo asi deja claro
		// que el angulo no cambia dentro del frame.
		//---------------------------------------------------
		sin_x = demo_sin(angle_x);
		cos_x = demo_cos(angle_x);
		sin_y = demo_sin(angle_y);
		cos_y = demo_cos(angle_y);

		for (index = 0; index < 8; index++){

			//-----------------------------------------------
			// Giro alrededor de Y: la x y la z se mezclan, la y no se
			// entera. Todo en long porque un producto de dos 8.8 se sale
			// de un int de 16 bits antes de bajarlo de nuevo.
			//-----------------------------------------------
			rotated_x = (((long)corner_x[index] * cos_y) +
			             ((long)corner_z[index] * sin_y)) >> DEMO_SHIFT;

			rotated_z = ((-(long)corner_x[index] * sin_y) +
			              ((long)corner_z[index] * cos_y)) >> DEMO_SHIFT;

			rotated_y = (long)corner_y[index];

			//-----------------------------------------------
			// Y ahora alrededor de X, sobre lo que ha salido del anterior.
			// El orden importa: girar primero en X y luego en Y da otro
			// movimiento distinto, igual de valido y con otra pinta.
			//-----------------------------------------------
			temporary_y = ((rotated_y * cos_x) - (rotated_z * sin_x)) >> DEMO_SHIFT;
			temporary_z = ((rotated_y * sin_x) + (rotated_z * cos_x)) >> DEMO_SHIFT;

			rotated_y = temporary_y;
			rotated_z = temporary_z;

			//-----------------------------------------------
			// Alejarlo, y AQUI esta la perspectiva entera: dividir por la
			// z. Lo que esta lejos tiene z grande y se encoge; lo que esta
			// cerca tiene z pequena y se agranda. Eso es todo.
			//
			// La z nunca llega a cero porque el cubo esta a 4 y su vertice
			// mas cercano a 4 - raiz(3) = 2,27. Sin ese margen habria que
			// recortar contra el plano de la camara, que es bastante mas
			// trabajo que sumar una constante.
			//-----------------------------------------------
			rotated_z += CUBE_DISTANCE;

			screen_x[index] = (int)((rotated_x * CUBE_FOCAL) / rotated_z)
			                + (DEMO_WIDTH / 2);

			screen_y[index] = (int)((((rotated_y * CUBE_FOCAL) / rotated_z)
			                  * CUBE_ASPECT_NUM) / CUBE_ASPECT_DEN)
			                + (DEMO_HEIGHT / 2);

		}

		//---------------------------------------------------
		// La foto de fondo, entera, cada frame.
		//
		// Es un memcpy de 64.000 bytes y es lo mas caro del efecto con
		// diferencia: cuesta mas que los ocho vertices y las doce rectas
		// juntos. Repintar solo por donde paso el cubo el frame anterior
		// se puede hacer, pero son doce rectas que hay que recordar y
		// volver a recorrer, y a 64.000 bytes de memcpy no le gana.
		//---------------------------------------------------
		memcpy(screen, image, DEMO_SCREEN);

		for (index = 0; index < 24; index += 2){

			cube_line(screen,
			          screen_x[edge[index]],     screen_y[edge[index]],
			          screen_x[edge[index + 1]], screen_y[edge[index + 1]],
			          color);

		}

		demo_show(screen);

		angle_x = (angle_x + CUBE_SPIN_X) & DEMO_ANGLE_MASK;
		angle_y = (angle_y + CUBE_SPIN_Y) & DEMO_ANGLE_MASK;

	}

	return 1;

}
