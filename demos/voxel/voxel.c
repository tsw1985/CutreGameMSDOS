//===========================================================
// VOXEL - volar por encima de la foto convertida en paisaje
//
// La foto se lee como un mapa en relieve: lo claro es monte y lo oscuro es
// valle. Y luego se vuela por encima. Es el motor del Comanche (1992), que
// dejo a todo el mundo con la boca abierta porque parecia 3D y no habia ni
// un poligono.
//
//
// DE UNA FOTO A UN PAISAJE
//
// Hacen falta dos mapas: uno de ALTURAS y otro de COLORES. La foto da los
// dos, y gratis:
//
//     color  = el pixel, tal cual
//     altura = lo claro que es ese color
//
// Lo segundo no se puede mirar en el pixel, porque en 256 colores un pixel
// no es un color: es un INDICE. El 200 no es mas claro que el 3, es
// simplemente el que ocupa la casilla 200. Asi que hay que ir a la paleta,
// sumar sus tres canales y quedarse con eso. Son 256 sumas al entrar y
// deja una tabla que convierte indice en altura de un vistazo.
//
// Es la misma trampa que persigue a media carpeta: en color indexado, el
// numero del pixel no significa nada por si solo.
//
//
// EL ALGORITMO, QUE ES DE UNA LINEA
//
// Por cada COLUMNA de la pantalla se lanza un rayo hacia el horizonte y se
// va andando. En cada paso se mira la altura del terreno, se proyecta a
// que altura de pantalla le toca, y:
//
//     si sale MAS ARRIBA de lo mas alto pintado hasta ahora en esta
//     columna, se pinta el trozo nuevo. Si no, esta tapado y no se pinta.
//
// Recorriendo de CERCA a LEJOS, eso basta para que lo cercano tape a lo
// lejano sin buffer de profundidad, sin ordenar y sin repintar un solo
// pixel dos veces. Un numero por columna ("hasta donde he llegado") hace
// el trabajo que en un 3D de verdad hace un z-buffer de 64.000 bytes.
//
//
// Y EL TRUCO DE SIEMPRE: NI UNA DIVISION
//
// Proyectar es dividir por la distancia, y son 41.000 pasos por frame. Las
// distancias, en cambio, son siempre las mismas 128: se camina de 1 en 1.
// Asi que se precalculan los 128 inversos al entrar y la division se
// convierte en un producto y un desplazamiento.
//===========================================================

#include "demos\demolib.h"
#include "demos\voxel\voxel.h"

// Cuantos pasos se anda hacia el horizonte. 128 es justo una vuelta
// completa al mapa, asi que el paisaje se cierra sobre si mismo y nunca
// se acaba.
#define VOXEL_DEPTH 		128

// La linea del horizonte, en filas de pantalla
#define VOXEL_HORIZON 		70

// El monte mas alto del mapa, y a que altura vuela la camara. La camara
// tiene que quedar por encima del monte o se meteria dentro del terreno.
//
// 80 contra 63 deja poco margen a proposito: cuanto mas raso se vuela, mas
// se abren las distancias entre lo alto y lo bajo y mas relieve se ve. Con
// la camara a 200 el mismo paisaje parece un mapa pintado.
#define VOXEL_MAX_HEIGHT 	63
#define VOXEL_CAMERA 		80

// La distancia focal. Sale de querer que el paso 128, el mas lejano, caiga
// a unas ocho filas por debajo del horizonte: 80 * 13 / 128 = 8.
#define VOXEL_FOCAL 		13

// Lo que se avanza por frame, en 8.8, y el vaiven lateral
#define VOXEL_SPEED 		96
#define VOXEL_SWAY 			40
#define VOXEL_SWAY_SPEED 	2

// Lo abierto que es el angulo de vision, en 8.8. 256 es un pixel de mapa
// por pixel de pantalla a distancia 1.
#define VOXEL_SPREAD 		96


int demo_voxel(unsigned char *image,
               unsigned char *screen,
               unsigned char *palette,
               unsigned long end_tick)
{
	unsigned char *texture;

	int height_of[256];
	int inverse[VOXEL_DEPTH + 1];

	int index;
	int x, z;
	int lowest, loftiest;
	int sway;
	int screen_y;
	int highest;
	int terrain;
	long camera_x, camera_y;
	long map_x, map_y;
	long step_x;
	unsigned char colour;
	unsigned int destination;
	unsigned int sample;

	texture = (unsigned char *)malloc(DEMO_TEXTURE_BYTES);

	if (texture == NULL){

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
	// DE INDICE A ALTURA. Los 768 bytes vienen en BGR y de 0 a 63, tal
	// como los deja bmp_load_pallete_data(), asi que la suma de los tres
	// va de 0 a 189 y dividida por 3 vuelve a 0..63.
	//
	// Sin pesar los canales. El ojo ve el verde mucho mas claro que el
	// azul y lo correcto seria 30/59/11, pero aqui no se busca el brillo
	// que percibe un ojo: se busca un relieve con variedad, y la media a
	// pelo la da igual de bien y sin tres productos.
	//-------------------------------------------------------
	for (index = 0; index <= 255; index++){

		height_of[index] = ((int)palette[index * 3 + 0]
		                 +  (int)palette[index * 3 + 1]
		                 +  (int)palette[index * 3 + 2]) / 3;

	}

	//-------------------------------------------------------
	// Y AHORA SE ESTIRA EL RELIEVE AL RANGO QUE LA FOTO TENGA DE VERDAD.
	//
	// Esto no estaba y hubo que ponerlo despues de mirar el banco. Con las
	// alturas crudas, una foto de tonos parecidos (un cielo, un documento,
	// cualquier cosa clara) daba un llano perfecto: sus 256 colores caian
	// todos entre 48 y 55 y el paisaje no tenia ni un bache.
	//
	// Asi que se busca el mas bajo y el mas alto DE LOS QUE LA FOTO USA
	// -recorriendo la textura, no la paleta, porque media paleta puede
	// estar sin estrenar- y se reparte ese trozo por los 63 escalones. Una
	// foto plana sale con montanas suaves y una contrastada con
	// acantilados, pero ninguna sale como una mesa de billar.
	//
	// 16.384 lecturas al entrar. Es lo mas barato de todo el montaje.
	//-------------------------------------------------------
	lowest  = 255;
	loftiest = 0;

	for (sample = 0; sample < DEMO_TEXTURE_BYTES; sample++){

		terrain = height_of[texture[sample]];

		if (terrain < lowest){
			lowest = terrain;
		}
		if (terrain > loftiest){
			loftiest = terrain;
		}

	}

	// Una foto de un solo tono: no hay nada que estirar y dividir seria
	// dividir entre cero. Se deja todo a media altura.
	if (loftiest <= lowest){

		for (index = 0; index <= 255; index++){
			height_of[index] = VOXEL_MAX_HEIGHT / 2;
		}

	}else{

		for (index = 0; index <= 255; index++){

			terrain = height_of[index] - lowest;

			if (terrain < 0){
				terrain = 0;
			}

			terrain = (terrain * VOXEL_MAX_HEIGHT) / (loftiest - lowest);

			if (terrain > VOXEL_MAX_HEIGHT){
				terrain = VOXEL_MAX_HEIGHT;
			}

			height_of[index] = terrain;

		}

	}

	//-------------------------------------------------------
	// LOS 128 INVERSOS. Aqui esta la division que no se hace 41.000 veces
	// por frame.
	//-------------------------------------------------------
	inverse[0] = 0;

	for (z = 1; z <= VOXEL_DEPTH; z++){
		inverse[z] = (VOXEL_FOCAL * DEMO_ONE) / z;
	}

	camera_x = 0;
	camera_y = 0;
	sway     = 0;

	while (demo_now() < end_tick){

		if (demo_escape_pressed() == 1){
			free(texture);
			return 0;
		}

		//---------------------------------------------------
		// El cielo. Todo negro, y lo que quede sin pintar lo sigue siendo:
		// el terreno se dibuja de abajo hacia arriba y se para donde se
		// para.
		//---------------------------------------------------
		memset(screen, 0, DEMO_SCREEN);

		for (x = 0; x < DEMO_WIDTH; x++){

			//-----------------------------------------------
			// Cuanto se abre el rayo de esta columna por cada paso. Es
			// constante dentro de la columna, asi que andar es sumar.
			//-----------------------------------------------
			step_x = ((long)(x - (DEMO_WIDTH / 2)) * VOXEL_SPREAD) >> DEMO_SHIFT;

			map_x   = camera_x;
			map_y   = camera_y;
			highest = DEMO_HEIGHT;

			for (z = 1; z <= VOXEL_DEPTH; z++){

				map_x += step_x;
				map_y += DEMO_ONE;

				//-------------------------------------------
				// La casilla del mapa. Los AND dan la vuelta solos porque
				// la textura es de 128, potencia de dos: el paisaje se
				// repite hasta el infinito en las dos direcciones.
				//-------------------------------------------
				sample = (unsigned int)((((map_y >> DEMO_SHIFT) & DEMO_TEXTURE_MASK)
				                          << DEMO_TEXTURE_SHIFT)
				                      +  ((map_x >> DEMO_SHIFT) & DEMO_TEXTURE_MASK));

				colour  = texture[sample];
				terrain = height_of[colour];

				//-------------------------------------------
				// Proyectar: un producto y un desplazamiento, sin dividir.
				//-------------------------------------------
				screen_y = VOXEL_HORIZON
				         + (((VOXEL_CAMERA - terrain) * inverse[z]) >> DEMO_SHIFT);

				if (screen_y >= highest){
					continue;		// tapado por algo mas cercano
				}

				if (screen_y < 0){
					screen_y = 0;
				}

				//-------------------------------------------
				// Y pintar el trozo que asoma. Se escribe hacia arriba con
				// paso de 320, que es lo caro de este efecto y lo unico
				// que se puede hacer: una columna de pantalla no son
				// bytes seguidos.
				//-------------------------------------------
				destination = demo_row[highest - 1] + (unsigned int)x;

				for (index = highest - 1; index >= screen_y; index--){

					screen[destination] = colour;
					destination -= DEMO_WIDTH;

				}

				highest = screen_y;

				//-------------------------------------------
				// NADA PUEDE DIBUJARSE POR ENCIMA DEL HORIZONTE, asi que
				// cuando una columna llega ahi ya esta terminada y los
				// pasos que queden son tiempo tirado.
				//
				// Y es exacto, no una aproximacion: la camara vuela SIEMPRE
				// por encima del monte mas alto, o sea que
				// (VOXEL_CAMERA - terrain) es siempre positivo y screen_y
				// siempre cae por debajo de VOXEL_HORIZON. El mejor caso
				// posible, monte a tope en el paso mas lejano, da
				// HORIZON + 1.
				//
				// Con un paisaje llano no salta nunca; con una cresta cerca
				// se ahorra media columna.
				//-------------------------------------------
				if (highest <= VOXEL_HORIZON + 1){
					break;
				}

			}

		}

		demo_show(screen);

		//---------------------------------------------------
		// Avanzar, y bambolearse de lado para que no parezca un rail.
		//---------------------------------------------------
		camera_y += VOXEL_SPEED;
		sway      = (sway + VOXEL_SWAY_SPEED) & DEMO_ANGLE_MASK;
		camera_x  = ((long)demo_sin(sway) * VOXEL_SWAY);

	}

	free(texture);

	return 1;

}
