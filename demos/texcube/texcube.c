//===========================================================
// TEXCUBE - el cubo con una foto en cada cara
//
// El cubo de siempre, pero relleno: seis caras y en cada una una de las
// fotos de la presentacion, girando en tres dimensiones. Es el efecto que
// todo el mundo recuerda de las intros y es, con diferencia, el mas largo
// de esta carpeta, por tres problemas que no tiene ningun otro.
//
//
// PROBLEMA 1: SEIS FOTOS Y UNA SOLA PALETA
//
// Y es el peor de los tres, aunque no lo parezca.
//
// El VGA en modo 13h tiene UN juego de 256 colores. Cada BMP trae el suyo.
// Medido sobre res\demo\: entre dos fotos cualesquiera de las quince
// coinciden 2,3 entradas de 256. O sea que pegar la foto 2 en una cara con
// la paleta de la foto 1 puesta no da "una foto con los colores un poco
// raros": da ruido de colores al azar.
//
// Es exactamente la misma trampa que el radar del juego, donde numbers.bmp
// y big.bmp difieren en 254 de las 256 entradas y por eso el radar no
// existe sin tema.
//
// La salida es la que usaban las demos de la epoca: REMAPEAR. Se queda la
// paleta de la foto que toca en la presentacion, y de las otras cinco se
// traduce cada uno de sus 256 colores al indice MAS PARECIDO de esa
// paleta. Las caras salen algo lavadas si las paletas no se parecen, y se
// reconocen perfectamente, que es de lo que se trata.
//
//
// PROBLEMA 2: LA PERSPECTIVA DEFORMA LAS TEXTURAS
//
// Rellenar un cuadrilatero interpolando la textura en linea recta (lo que
// se llama mapeado afin) esta MAL en cuanto hay perspectiva: la parte de
// la cara que esta lejos tiene que comprimirse mas deprisa de lo que una
// recta comprime. Se nota como un pliegue por la diagonal de la cara, que
// es el defecto por el que se reconocen los juegos de la primera
// PlayStation.
//
// Lo correcto es dividir por la profundidad en CADA pixel, y eso son
// 25.000 divisiones por frame que no caben.
//
// Lo que se hace aqui es lo que hacian ellos: partir cada cara en una
// rejilla de 4x4 y proyectar los 25 puntos de la rejilla CON perspectiva.
// Dentro de cada trocito el error de interpolar recto es tan pequeno que
// no se ve. Son 150 divisiones por frame en vez de 25.000, y el pliegue
// desaparece.
//
// Y los 25 puntos de la rejilla no hay que rotarlos: rotar es una
// operacion lineal, asi que interpolar entre las esquinas YA rotadas da
// exactamente lo mismo que rotar el punto interpolado. Se rotan 8 vertices
// y el resto sale de sumas.
//
//
// PROBLEMA 3: HAY QUE RELLENAR
//
// El cubo de alambre de cube.c pinta doce rectas. Este pinta unos 25.000
// pixeles con textura. Por eso hay ahi abajo un rasterizador de triangulos
// de verdad, con sus pendientes y sus gradientes, que es la pieza mas
// seria de toda la carpeta.
//
// Las caras que miran para atras no se pintan, y con eso basta para no
// necesitar un buffer de profundidad: un cubo es convexo, asi que las tres
// caras que se ven como mucho NUNCA se tapan entre ellas.
//===========================================================

#include "demos\demolib.h"
#include "demos\texcube\texcube.h"

#define TEXCUBE_FOCAL 		230
#define TEXCUBE_DISTANCE 	(DEMO_ONE * 4)

// Pasos de angulo por frame en cada eje. Tres velocidades distintas para
// que el cubo no repita postura en los siete segundos que dura.
#define TEXCUBE_SPIN_X 		1
#define TEXCUBE_SPIN_Y 		2
#define TEXCUBE_SPIN_Z 		3

// El pixel del modo 13h es un 20% mas alto que ancho. Ver cube.c.
#define TEXCUBE_ASPECT_NUM 	5
#define TEXCUBE_ASPECT_DEN 	6

// En cuantos trozos se parte cada cara por lado. 4 son 16 cuadritos y 25
// puntos de rejilla por cara. Con 1 (o sea sin partir) el pliegue de la
// diagonal se ve a simple vista; con 2 casi no; con 4 no se ve nada.
#define TEXCUBE_GRID 		4
#define TEXCUBE_POINTS 		((TEXCUBE_GRID + 1) * (TEXCUBE_GRID + 1))

#define TEXCUBE_FACES 		6

// Cada cuantas filas de textura se alimenta la tarjeta de sonido mientras
// se cargan las seis fotos. Cargar seis BMP y construir cinco tablas de
// remapeo es lo mas lento que hace esta carpeta, con diferencia.
#define TEXCUBE_SOUND_ROWS 	16


//-----------------------------------------------------------
// LA TABLA DE CUADRADOS.
//
// El remapeo compara 256 colores contra 256 colores para cada una de las
// cinco fotos: 327.680 distancias. Una distancia son tres restas, tres
// productos y dos sumas.
//
// Los canales de la paleta van de 0 a 63 (bmp_load_pallete_data divide por
// 4), asi que la diferencia entre dos va de -63 a 63 y su cuadrado cabe de
// sobra en un int. Una tabla de 64 numeros cambia los tres productos por
// tres lecturas, y en un 386 un producto de 16 bits cuesta lo que quince
// lecturas.
//-----------------------------------------------------------
static int texcube_square[64];

static void texcube_init_squares(void){

	int i;

	for (i = 0; i < 64; i++){
		texcube_square[i] = i * i;
	}

}


//-----------------------------------------------------------
// DE LA PALETA DE UNA FOTO A LA PALETA QUE HAY PUESTA
//
// Para cada uno de los 256 colores de "from", el indice de "to" que menos
// se le parece... el que MENOS distancia tiene, que es el que mas se le
// parece. Fuerza bruta, 65.536 comparaciones, una vez por foto.
//
// La distancia es la euclidea al cuadrado en RGB. No es la mejor forma de
// medir si dos colores se parecen (el ojo no pesa igual el verde que el
// azul), pero es la que usaban entonces y no se nota en una cara de cubo
// de 80 pixeles que esta girando.
//-----------------------------------------------------------
static void texcube_build_remap(unsigned char *from, unsigned char *to,
                                unsigned char *remap)
{
	int source;
	int target;
	int db, dg, dr;
	int distance;
	int best_distance;
	int best_index;

	for (source = 0; source <= 255; source++){

		best_distance = 32767;
		best_index    = 0;

		for (target = 0; target <= 255; target++){

			db = (int)from[source * 3 + 0] - (int)to[target * 3 + 0];
			if (db < 0){
				db = -db;
			}

			dg = (int)from[source * 3 + 1] - (int)to[target * 3 + 1];
			if (dg < 0){
				dg = -dg;
			}

			dr = (int)from[source * 3 + 2] - (int)to[target * 3 + 2];
			if (dr < 0){
				dr = -dr;
			}

			distance = texcube_square[db] + texcube_square[dg] + texcube_square[dr];

			if (distance < best_distance){
				best_distance = distance;
				best_index    = target;

				// Exacto: no hay nada mejor que buscar
				if (distance == 0){
					target = 255;
				}
			}

		}

		remap[source] = (unsigned char)best_index;

	}

}


//-----------------------------------------------------------
// CARGAR UNA FOTO DIRECTAMENTE COMO TEXTURA DE 128x128
//
// Sin pasar por un buffer de 64.000. Se lee el BMP FILA A FILA, se saca de
// cada fila las 128 columnas que hacen falta y se tira el resto.
//
// Es mas lento que leer los 64.000 de golpe (128 fseek por foto en vez de
// uno), y a cambio no pide ni un byte grande de memoria. Con seis fotos
// abiertas, la otra forma serian 64.000 bytes mas de monton en el momento
// exacto en el que ya hay 98.304 de texturas pedidos.
//
// Y hay que darle la vuelta: un BMP guarda la ultima fila primero. Por eso
// la fila de fichero es 199 - la fila de imagen, y no hace falta llamar a
// bmp_revert_bmp() ni tener las dos versiones en memoria a la vez.
//-----------------------------------------------------------
static int texcube_load_face(char *path, unsigned char *texture,
                             unsigned char *target_palette,
                             unsigned char *scratch_palette,
                             unsigned char *remap,
                             unsigned char *row,
                             int is_current)
{
	FILE *file;
	int texture_y;
	int texture_x;
	int source_y;
	long step_x;
	long source_x;
	unsigned int destination;

	// demo_image_path() devuelve NULL si nadie le dio la lista, que es lo
	// que pasa si alguien llama a este efecto sin pasar por demo_run().
	if (path == NULL){
		return 0;
	}

	file = fopen(path, "rb");

	if (file == NULL){
		return 0;
	}

	//-------------------------------------------------------
	// La paleta de ESTA foto, y la tabla que la traduce a la que hay
	// puesta en el DAC. La cara de la foto que se esta ensenando en la
	// presentacion se salta el remapeo entero: su paleta ES la puesta.
	//-------------------------------------------------------
	if (is_current == 1){

		for (texture_x = 0; texture_x <= 255; texture_x++){
			remap[texture_x] = (unsigned char)texture_x;
		}

	}else{

		bmp_load_pallete_data((char *)scratch_palette, file);
		texcube_build_remap(scratch_palette, target_palette, remap);

	}

	step_x      = ((long)DEMO_WIDTH << DEMO_SHIFT) / DEMO_TEXTURE_SIZE;
	destination = 0;

	for (texture_y = 0; texture_y < DEMO_TEXTURE_SIZE; texture_y++){

		// Que fila de la foto le toca a esta fila de textura, y donde esta
		// esa fila dentro del fichero contando que el BMP va del reves.
		source_y = (texture_y * DEMO_HEIGHT) / DEMO_TEXTURE_SIZE;

		fseek(file, 1078L + ((long)(DEMO_HEIGHT - 1 - source_y) * (long)DEMO_WIDTH), SEEK_SET);
		fread(row, DEMO_WIDTH, 1, file);

		source_x = 0;

		for (texture_x = 0; texture_x < DEMO_TEXTURE_SIZE; texture_x++){

			texture[destination + texture_x] =
				remap[row[(unsigned int)(source_x >> DEMO_SHIFT)]];

			source_x += step_x;

		}

		destination += DEMO_TEXTURE_SIZE;

		if ((texture_y % TEXCUBE_SOUND_ROWS) == 0){
			demo_sound();
		}

	}

	fclose(file);

	return 1;

}


//-----------------------------------------------------------
// UN TRIANGULO CON TEXTURA
//
// La pieza seria. Recibe tres vertices con su posicion en pantalla y su
// coordenada de textura en 8.8, y rellena.
//
// LOS GRADIENTES, que es lo que hace que el bucle interior sea barato.
//
// En un mapeado afin, cuanto avanza u al moverse un pixel a la derecha es
// el MISMO numero en todo el triangulo. No depende de la fila. Asi que se
// calcula una vez, con el determinante de los tres vertices, y dentro ya
// solo hay sumas:
//
//     du/dx = ((u1-u0)(y2-y0) - (u2-u0)(y1-y0)) / area
//
// Cuatro divisiones por triangulo (du/dx, du/dy, dv/dx, dv/dy) y ni una
// por pixel.
//
// Y LOS BORDES se recorren con pendientes en punto fijo, en dos tramos: de
// la punta de arriba al vertice de en medio, y de ahi a la punta de abajo.
// El borde largo va de arriba abajo del tiron en los dos tramos, que es
// por lo que hay un solo "xa" y dos "xb".
//-----------------------------------------------------------
static void texcube_triangle(unsigned char *screen, unsigned char *texture,
                             int *px, int *py, int *pu, int *pv)
{
	int order[3];
	int temporary;
	int top, middle, bottom;
	long area;
	long du_dx, du_dy, dv_dx, dv_dy;
	long edge_long, edge_short;
	long x_long, x_short;
	long u, v;
	int y, x;
	int left, right;
	int half;
	int start_y, end_y;
	unsigned int row;

	order[0] = 0;
	order[1] = 1;
	order[2] = 2;

	// Ordenar tres cosas por y: dos comparaciones y una tercera. A mano,
	// que un qsort para tres elementos es una broma.
	if (py[order[0]] > py[order[1]]){
		temporary = order[0]; order[0] = order[1]; order[1] = temporary;
	}
	if (py[order[1]] > py[order[2]]){
		temporary = order[1]; order[1] = order[2]; order[2] = temporary;
	}
	if (py[order[0]] > py[order[1]]){
		temporary = order[0]; order[0] = order[1]; order[1] = temporary;
	}

	top    = order[0];
	middle = order[1];
	bottom = order[2];

	// Triangulo sin altura: no hay ninguna fila que rellenar.
	if (py[bottom] == py[top]){
		return;
	}

	//-------------------------------------------------------
	// El area con signo. Si sale 0 el triangulo es una raya y no se puede
	// dividir por el: se sale.
	//-------------------------------------------------------
	area = ((long)(px[1] - px[0]) * (long)(py[2] - py[0]))
	     - ((long)(px[2] - px[0]) * (long)(py[1] - py[0]));

	//-------------------------------------------------------
	// Y no basta con que no sea 0: hay que exigirle un minimo.
	//
	// Los gradientes se dividen por el area, asi que un triangulo casi
	// degenerado (una cara que estamos viendo de canto, justo antes de que
	// el descarte se la lleve) da un du_dx enorme. Multiplicado luego por
	// la distancia al vertice 0 puede pasarse de un long de 32 bits, y lo
	// que sale de un desbordamiento es un pixel de un color que no toca.
	//
	// Con el area por encima de 16 (o sea 8 pixeles cuadrados), el peor
	// du_dx posible es 8128*600/16 = 305.000, que por los 500 de distancia
	// maxima son 1,5e8: queda un factor de catorce de margen.
	//
	// Y no se pierde nada visible: un triangulo de 8 pixeles cuadrados en
	// una cara de canto no se ve, y esa cara se va a descartar entera en
	// uno o dos frames.
	//-------------------------------------------------------
	if (area > -16 && area < 16){
		return;
	}

	du_dx = (((long)(pu[1] - pu[0]) * (long)(py[2] - py[0]))
	      -  ((long)(pu[2] - pu[0]) * (long)(py[1] - py[0]))) / area;

	du_dy = (((long)(pu[2] - pu[0]) * (long)(px[1] - px[0]))
	      -  ((long)(pu[1] - pu[0]) * (long)(px[2] - px[0]))) / area;

	dv_dx = (((long)(pv[1] - pv[0]) * (long)(py[2] - py[0]))
	      -  ((long)(pv[2] - pv[0]) * (long)(py[1] - py[0]))) / area;

	dv_dy = (((long)(pv[2] - pv[0]) * (long)(px[1] - px[0]))
	      -  ((long)(pv[1] - pv[0]) * (long)(px[2] - px[0]))) / area;

	//-------------------------------------------------------
	// El borde largo, de la punta de arriba a la de abajo. Su pendiente
	// vale para los dos tramos.
	//-------------------------------------------------------
	edge_long = (((long)(px[bottom] - px[top]) << DEMO_SHIFT)
	          / (long)(py[bottom] - py[top]));

	x_long = (long)px[top] << DEMO_SHIFT;

	for (half = 0; half < 2; half++){

		if (half == 0){

			start_y = py[top];
			end_y   = py[middle];

			if (end_y == start_y){
				continue;			// la punta de arriba es plana
			}

			edge_short = (((long)(px[middle] - px[top]) << DEMO_SHIFT)
			           / (long)(py[middle] - py[top]));

			x_short = (long)px[top] << DEMO_SHIFT;

		}else{

			start_y = py[middle];
			end_y   = py[bottom];

			if (end_y == start_y){
				continue;			// la punta de abajo es plana
			}

			edge_short = (((long)(px[bottom] - px[middle]) << DEMO_SHIFT)
			           / (long)(py[bottom] - py[middle]));

			x_short = (long)px[middle] << DEMO_SHIFT;

		}

		for (y = start_y; y < end_y; y++){

			//-----------------------------------------------
			// Saltarse las filas de fuera SIN dejar de avanzar los bordes:
			// si se hiciera un "continue" a secas, al volver a entrar en
			// pantalla los bordes estarian donde los dejo la ultima fila
			// visible y el triangulo saldria torcido.
			//-----------------------------------------------
			if (y >= 0 && y < DEMO_HEIGHT){

				left  = (int)(x_long  >> DEMO_SHIFT);
				right = (int)(x_short >> DEMO_SHIFT);

				if (left > right){
					temporary = left; left = right; right = temporary;
				}

				if (left < 0){
					left = 0;
				}
				if (right > DEMO_WIDTH){
					right = DEMO_WIDTH;
				}

				if (left < right){

					//-------------------------------------------
					// La textura en el primer pixel de la fila, a partir
					// del vertice 0 y de los cuatro gradientes. Dos
					// productos por fila, y dentro del bucle ni uno.
					//-------------------------------------------
					u = (long)pu[0]
					  + (du_dx * (long)(left - px[0]))
					  + (du_dy * (long)(y - py[0]));

					v = (long)pv[0]
					  + (dv_dx * (long)(left - px[0]))
					  + (dv_dy * (long)(y - py[0]));

					row = demo_row[y];

					for (x = left; x < right; x++){

						screen[row + (unsigned int)x] = texture[
							(unsigned int)(((v >> DEMO_SHIFT) & DEMO_TEXTURE_MASK) << DEMO_TEXTURE_SHIFT)
						  + (unsigned int)((u >> DEMO_SHIFT) & DEMO_TEXTURE_MASK)
						];

						u += du_dx;
						v += dv_dx;

					}

				}

			}

			x_long  += edge_long;
			x_short += edge_short;

		}

	}

}


int demo_texcube(unsigned char *image,
                 unsigned char *screen,
                 unsigned char *palette,
                 unsigned long end_tick)
{
	//-------------------------------------------------------
	// EL CUBO. Ocho vertices y seis caras, y cada cara son cuatro indices
	// EN ORDEN alrededor de ella, mirandola desde fuera.
	//
	// Ese orden es el que decide si una cara mira hacia nosotros o no: al
	// proyectarla, una cara que miramos de frente conserva el sentido del
	// giro y una que miramos por detras lo invierte. Ver mas abajo.
	//-------------------------------------------------------
	static int corner_x[8] = { -DEMO_ONE,  DEMO_ONE,  DEMO_ONE, -DEMO_ONE,
	                           -DEMO_ONE,  DEMO_ONE,  DEMO_ONE, -DEMO_ONE };
	static int corner_y[8] = { -DEMO_ONE, -DEMO_ONE,  DEMO_ONE,  DEMO_ONE,
	                           -DEMO_ONE, -DEMO_ONE,  DEMO_ONE,  DEMO_ONE };
	static int corner_z[8] = { -DEMO_ONE, -DEMO_ONE, -DEMO_ONE, -DEMO_ONE,
	                            DEMO_ONE,  DEMO_ONE,  DEMO_ONE,  DEMO_ONE };

	static int face[TEXCUBE_FACES][4] = {
		{ 0, 1, 2, 3 },		// delante
		{ 5, 4, 7, 6 },		// detras
		{ 4, 0, 3, 7 },		// izquierda
		{ 1, 5, 6, 2 },		// derecha
		{ 4, 5, 1, 0 },		// arriba
		{ 3, 2, 6, 7 }		// abajo
	};

	unsigned char *texture[TEXCUBE_FACES];
	unsigned char *scratch_palette;
	unsigned char *remap;
	unsigned char *row;

	long rotated_x[8], rotated_y[8], rotated_z[8];
	int  point_x[TEXCUBE_POINTS];
	int  point_y[TEXCUBE_POINTS];

	int triangle_x[3], triangle_y[3], triangle_u[3], triangle_v[3];

	int angle_x, angle_y, angle_z;
	int sin_x, cos_x, sin_y, cos_y, sin_z, cos_z;
	long temporary_x, temporary_y, temporary_z;
	long corner_rx[4], corner_ry[4], corner_rz[4];
	long top_x, top_y, top_z;
	long bottom_x, bottom_y, bottom_z;
	long point_rx, point_ry, point_rz;
	long cross;
	int index, which, i, j;
	int loaded;
	int current;
	int corner_a, corner_b, corner_c;
	int step_u;

	(void)image;

	texcube_init_squares();

	//-------------------------------------------------------
	// La memoria. Seis texturas de 16K, mas tres cosas pequenas.
	//
	// Los pequenos se piden DESPUES de los grandes a proposito: si se
	// pidieran antes, dejarian el monton picado y las seis peticiones de
	// 16.384 tendrian que buscarse la vida entre los huecos. Es la misma
	// regla del mapa de 256.000 del juego, a escala.
	//-------------------------------------------------------
	loaded = 0;

	for (index = 0; index < TEXCUBE_FACES; index++){

		texture[index] = (unsigned char *)malloc(DEMO_TEXTURE_BYTES);

		if (texture[index] != NULL){
			loaded++;
		}

	}

	scratch_palette = (unsigned char *)malloc(768);
	remap           = (unsigned char *)malloc(256);
	row             = (unsigned char *)malloc(DEMO_WIDTH);

	if (loaded < TEXCUBE_FACES || scratch_palette == NULL
	    || remap == NULL || row == NULL){

		if (row != NULL){
			free(row);
		}
		if (remap != NULL){
			free(remap);
		}
		if (scratch_palette != NULL){
			free(scratch_palette);
		}

		for (index = TEXCUBE_FACES - 1; index >= 0; index--){
			if (texture[index] != NULL){
				free(texture[index]);
			}
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

	//-------------------------------------------------------
	// LAS SEIS FOTOS.
	//
	// Se empieza por la que se esta ensenando ahora mismo en la
	// presentacion, asi que cada vez que el cubo sale en la intro lleva
	// seis fotos distintas. demo_image_path() envuelve sola, asi que con
	// menos de seis imagenes se repiten y no falla nada.
	//-------------------------------------------------------
	current = demo_image_current();

	for (index = 0; index < TEXCUBE_FACES; index++){

		if (texcube_load_face(demo_image_path(current + index),
		                      texture[index], palette, scratch_palette,
		                      remap, row,
		                      (index == 0)) == 0){

			// La foto no estaba. Se deja la cara en negro y se sigue: en
			// una intro es mejor una cara apagada que no arrancar.
			memset(texture[index], 0, DEMO_TEXTURE_BYTES);

		}

	}

	free(row);
	free(remap);
	free(scratch_palette);

	// La coordenada de textura de un punto de la rejilla, en 8.8. El
	// ultimo punto tiene que caer en 127 y no en 128, o el AND del
	// rasterizador lo devolveria al 0 y se veria una raya del otro borde.
	step_u = ((DEMO_TEXTURE_SIZE - 1) << DEMO_SHIFT) / TEXCUBE_GRID;

	angle_x = 0;
	angle_y = 0;
	angle_z = 0;

	while (demo_now() < end_tick){

		if (demo_escape_pressed() == 1){

			for (index = TEXCUBE_FACES - 1; index >= 0; index--){
				free(texture[index]);
			}

			return 0;

		}

		sin_x = demo_sin(angle_x);
		cos_x = demo_cos(angle_x);
		sin_y = demo_sin(angle_y);
		cos_y = demo_cos(angle_y);
		sin_z = demo_sin(angle_z);
		cos_z = demo_cos(angle_z);

		//---------------------------------------------------
		// LOS OCHO VERTICES, y solo los ocho. Todo lo demas del frame sale
		// de interpolar entre estos.
		//---------------------------------------------------
		for (index = 0; index < 8; index++){

			// Giro en Y
			temporary_x = (((long)corner_x[index] * cos_y)
			            +  ((long)corner_z[index] * sin_y)) >> DEMO_SHIFT;
			temporary_z = ((-(long)corner_x[index] * sin_y)
			            +  ((long)corner_z[index] * cos_y)) >> DEMO_SHIFT;
			temporary_y = (long)corner_y[index];

			// Giro en X
			rotated_y[index] = ((temporary_y * cos_x) - (temporary_z * sin_x)) >> DEMO_SHIFT;
			rotated_z[index] = ((temporary_y * sin_x) + (temporary_z * cos_x)) >> DEMO_SHIFT;
			rotated_x[index] = temporary_x;

			// Giro en Z. El tercer eje es el que hace que el cubo no
			// parezca que se balancea siempre igual.
			temporary_x = ((rotated_x[index] * cos_z) - (rotated_y[index] * sin_z)) >> DEMO_SHIFT;
			temporary_y = ((rotated_x[index] * sin_z) + (rotated_y[index] * cos_z)) >> DEMO_SHIFT;

			rotated_x[index] = temporary_x;
			rotated_y[index] = temporary_y;
			rotated_z[index] += TEXCUBE_DISTANCE;

		}

		memset(screen, 0, DEMO_SCREEN);

		for (which = 0; which < TEXCUBE_FACES; which++){

			//-----------------------------------------------
			// Las cuatro esquinas de esta cara, ya rotadas.
			//-----------------------------------------------
			for (index = 0; index < 4; index++){
				corner_a = face[which][index];
				corner_rx[index] = rotated_x[corner_a];
				corner_ry[index] = rotated_y[corner_a];
				corner_rz[index] = rotated_z[corner_a];
			}

			//-----------------------------------------------
			// LA REJILLA. Bilineal entre las cuatro esquinas: primero se
			// interpola el borde de arriba y el de abajo, y luego entre
			// esos dos.
			//
			// Y se proyecta cada punto con su propia division por z, que
			// es donde entra la perspectiva de verdad.
			//-----------------------------------------------
			index = 0;

			for (j = 0; j <= TEXCUBE_GRID; j++){

				for (i = 0; i <= TEXCUBE_GRID; i++){

					top_x = corner_rx[0] + (((corner_rx[1] - corner_rx[0]) * (long)i) / TEXCUBE_GRID);
					top_y = corner_ry[0] + (((corner_ry[1] - corner_ry[0]) * (long)i) / TEXCUBE_GRID);
					top_z = corner_rz[0] + (((corner_rz[1] - corner_rz[0]) * (long)i) / TEXCUBE_GRID);

					bottom_x = corner_rx[3] + (((corner_rx[2] - corner_rx[3]) * (long)i) / TEXCUBE_GRID);
					bottom_y = corner_ry[3] + (((corner_ry[2] - corner_ry[3]) * (long)i) / TEXCUBE_GRID);
					bottom_z = corner_rz[3] + (((corner_rz[2] - corner_rz[3]) * (long)i) / TEXCUBE_GRID);

					point_rx = top_x + (((bottom_x - top_x) * (long)j) / TEXCUBE_GRID);
					point_ry = top_y + (((bottom_y - top_y) * (long)j) / TEXCUBE_GRID);
					point_rz = top_z + (((bottom_z - top_z) * (long)j) / TEXCUBE_GRID);

					// La z nunca es 0: el cubo esta a 4 y su vertice mas
					// cercano a 4 - raiz(3) = 2,27.
					point_x[index] = (int)((point_rx * TEXCUBE_FOCAL) / point_rz)
					               + (DEMO_WIDTH / 2);

					point_y[index] = (int)((((point_ry * TEXCUBE_FOCAL) / point_rz)
					                 * TEXCUBE_ASPECT_NUM) / TEXCUBE_ASPECT_DEN)
					               + (DEMO_HEIGHT / 2);

					index++;

				}

			}

			//-----------------------------------------------
			// LA CARA QUE MIRA PARA ATRAS NO SE PINTA.
			//
			// El producto vectorial en 2D de dos lados seguidos de la cara
			// YA PROYECTADA. Si sale de un signo, las cuatro esquinas
			// estan en pantalla en el mismo sentido en que se escribieron
			// en la tabla, o sea que la vemos de frente. Si sale del otro,
			// la vemos por detras.
			//
			// Con esto no hace falta ordenar las caras ni llevar un buffer
			// de profundidad: en un cuerpo convexo, las caras que pasan
			// esta prueba no se tapan entre ellas NUNCA.
			//-----------------------------------------------
			corner_a = 0;
			corner_b = TEXCUBE_GRID;
			corner_c = TEXCUBE_POINTS - 1;

			cross = ((long)(point_x[corner_b] - point_x[corner_a])
			      *  (long)(point_y[corner_c] - point_y[corner_a]))
			      - ((long)(point_x[corner_c] - point_x[corner_a])
			      *  (long)(point_y[corner_b] - point_y[corner_a]));

			if (cross <= 0){
				continue;
			}

			//-----------------------------------------------
			// Y a rellenar: cada cuadrito de la rejilla son dos
			// triangulos.
			//-----------------------------------------------
			for (j = 0; j < TEXCUBE_GRID; j++){

				for (i = 0; i < TEXCUBE_GRID; i++){

					index = (j * (TEXCUBE_GRID + 1)) + i;

					// Triangulo de arriba a la izquierda
					triangle_x[0] = point_x[index];
					triangle_y[0] = point_y[index];
					triangle_u[0] = i * step_u;
					triangle_v[0] = j * step_u;

					triangle_x[1] = point_x[index + 1];
					triangle_y[1] = point_y[index + 1];
					triangle_u[1] = (i + 1) * step_u;
					triangle_v[1] = j * step_u;

					triangle_x[2] = point_x[index + TEXCUBE_GRID + 1];
					triangle_y[2] = point_y[index + TEXCUBE_GRID + 1];
					triangle_u[2] = i * step_u;
					triangle_v[2] = (j + 1) * step_u;

					texcube_triangle(screen, texture[which],
					                 triangle_x, triangle_y,
					                 triangle_u, triangle_v);

					// Triangulo de abajo a la derecha
					triangle_x[0] = point_x[index + 1];
					triangle_y[0] = point_y[index + 1];
					triangle_u[0] = (i + 1) * step_u;
					triangle_v[0] = j * step_u;

					triangle_x[1] = point_x[index + TEXCUBE_GRID + 2];
					triangle_y[1] = point_y[index + TEXCUBE_GRID + 2];
					triangle_u[1] = (i + 1) * step_u;
					triangle_v[1] = (j + 1) * step_u;

					triangle_x[2] = point_x[index + TEXCUBE_GRID + 1];
					triangle_y[2] = point_y[index + TEXCUBE_GRID + 1];
					triangle_u[2] = i * step_u;
					triangle_v[2] = (j + 1) * step_u;

					texcube_triangle(screen, texture[which],
					                 triangle_x, triangle_y,
					                 triangle_u, triangle_v);

				}

			}

		}

		demo_show(screen);

		angle_x = (angle_x + TEXCUBE_SPIN_X) & DEMO_ANGLE_MASK;
		angle_y = (angle_y + TEXCUBE_SPIN_Y) & DEMO_ANGLE_MASK;
		angle_z = (angle_z + TEXCUBE_SPIN_Z) & DEMO_ANGLE_MASK;

	}

	for (index = TEXCUBE_FACES - 1; index >= 0; index--){
		free(texture[index]);
	}

	return 1;

}
