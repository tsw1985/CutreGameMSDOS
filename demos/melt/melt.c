//===========================================================
// MELT - la pantalla que se derrite
//
// Cada columna de la foto se descuelga por su cuenta, a su velocidad y
// empezando cuando le toca, hasta que no queda nada. Es el barrido con el
// que Doom cambiaba de pantalla en 1993, y sigue siendo el efecto de
// transicion mas reconocible que se ha hecho.
//
// Es tambien el mas simple de los veinte: no hay seno, ni punto fijo, ni
// tabla. Son 320 numeros que crecen.
//
//
// POR QUE VA POR COLUMNAS Y NO POR FILAS
//
// Todos los efectos baratos de esta carpeta mueven FILAS, porque una fila
// son 320 bytes seguidos y eso es un memcpy. Este no puede: cada columna
// esta caida una cantidad distinta, asi que una fila de la pantalla se
// compone de 320 trozos de 320 filas distintas de la foto.
//
// Recorrerlo por columnas significa escribir con un paso de 320 bytes, que
// es lo peor que se le puede pedir a una cache. En un 386 eso da igual -
// no hay cache que estropear - y en el banco de Linux se nota y sigue
// siendo barato, porque son 64.000 escrituras y ni una cuenta.
//
//
// EL AZAR SIN rand(), Y QUE NO SALGA UN PEINE
//
// Doom sorteaba el retraso de cada columna. Aqui no hay rand() ni hace
// falta: dos senos de periodos distintos, sumados, dan una sucesion que no
// se repite en 320 columnas y que ademas sale IGUAL en las dos maquinas de
// una partida en red. Un rand() no lo garantiza, y aunque el lockstep no
// mira la intro, es una costumbre barata de mantener.
//
// Pero el reparto no puede ser azar del todo, y esto costo una pasada por
// el banco. Con los dos senos de periodo corto, cada columna caia a un
// ritmo sin relacion con su vecina y lo que salia era un PEINE: 320 palos
// independientes, no una cortina.
//
// Doom no lo hacia asi: sorteaba la primera columna y a cada siguiente le
// dejaba separarse como mucho un paso de la anterior. El resultado es
// ondulado por abajo y con el borde mellado.
//
// Aqui se consigue lo mismo pesando las dos frecuencias: una LENTA (una
// onda cada 128 columnas) que manda y hace la cortina, y una rapida con un
// tercio del peso que solo le pone el borde mellado.
//===========================================================

#include "demos\demolib.h"
#include "demos\melt\melt.h"

// Lo que baja una columna por frame, como minimo y como maximo. El margen
// es estrecho a proposito: con velocidades muy dispares la cortina se
// despedaza a los dos segundos.
#define MELT_SPEED_MIN 		2
#define MELT_SPEED_MAX 		4

// Cuantos frames espera una columna antes de empezar a caer, como mucho
#define MELT_DELAY_MAX 		56

// La onda lenta manda (peso 3) y la rapida solo mella el borde (peso 1).
// 2 sobre 256 pasos es una onda cada 128 columnas; 37 es una cada siete.
#define MELT_SLOW 			2
#define MELT_FAST 			37
#define MELT_SLOW_WEIGHT 	3


int demo_melt(unsigned char *image,
              unsigned char *screen,
              unsigned char *palette,
              unsigned long end_tick)
{
	int offset[DEMO_WIDTH];
	int delay[DEMO_WIDTH];
	int speed[DEMO_WIDTH];

	int x, y;
	int mixed;
	int falling;
	unsigned int source;
	unsigned int destination;

	(void)palette;

	//-------------------------------------------------------
	// El reparto inicial. Se hace aqui y no en el bucle porque una columna
	// tiene que caer siempre a la misma velocidad: si se recalculara por
	// frame, la cortina temblaria en vez de caer.
	//-------------------------------------------------------
	for (x = 0; x < DEMO_WIDTH; x++){

		mixed = (demo_sin(x * MELT_SLOW) * MELT_SLOW_WEIGHT)
		      +  demo_sin(x * MELT_FAST);

		// mixed va de -1024 a 1024. Subirle 1024 lo deja en 0..2048 sin
		// usar un valor absoluto, que partiria la sucesion por la mitad.
		mixed += 1024;

		delay[x] = (mixed * MELT_DELAY_MAX) / 2048;

		// La velocidad sale de OTRA onda lenta, y desfasada: si saliera de
		// la misma, las columnas que empiezan tarde serian ademas las
		// lentas y la cortina se abriria siempre por el mismo sitio.
		speed[x] = MELT_SPEED_MIN
		         + (((demo_sin((x * MELT_SLOW) + 64) + 256)
		             * (MELT_SPEED_MAX - MELT_SPEED_MIN + 1)) / 513);

		if (speed[x] > MELT_SPEED_MAX){
			speed[x] = MELT_SPEED_MAX;
		}

		offset[x] = 0;

	}

	while (demo_now() < end_tick){

		if (demo_escape_pressed() == 1){
			return 0;
		}

		memset(screen, 0, DEMO_SCREEN);

		falling = 0;

		for (x = 0; x < DEMO_WIDTH; x++){

			//-----------------------------------------------
			// Mover esta columna. El retraso se gasta primero y solo
			// despues empieza a caer.
			//-----------------------------------------------
			if (delay[x] > 0){

				delay[x]--;
				falling = 1;

			}else if (offset[x] < DEMO_HEIGHT){

				offset[x] += speed[x];
				falling = 1;

			}

			if (offset[x] >= DEMO_HEIGHT){
				continue;		// esta columna ya se ha ido del todo
			}

			//-----------------------------------------------
			// Y pintarla: la fila 0 de la foto cae en la fila offset de la
			// pantalla, y lo que sobra por abajo se recorta solo porque el
			// bucle acaba en DEMO_HEIGHT.
			//-----------------------------------------------
			destination = demo_row[offset[x]] + (unsigned int)x;
			source      = (unsigned int)x;

			for (y = offset[x]; y < DEMO_HEIGHT; y++){

				screen[destination] = image[source];

				destination += DEMO_WIDTH;
				source      += DEMO_WIDTH;

			}

		}

		demo_show(screen);

		//---------------------------------------------------
		// Cuando no queda nada cayendo se vuelve a empezar. Siete segundos
		// dan para dos pasadas y pico, y dejar la pantalla en negro los
		// tres ultimos segundos seria una diapositiva perdida.
		//---------------------------------------------------
		if (falling == 0){

			for (x = 0; x < DEMO_WIDTH; x++){

				mixed = (demo_sin(x * MELT_SLOW) * MELT_SLOW_WEIGHT)
				      +  demo_sin(x * MELT_FAST) + 1024;
				delay[x]  = (mixed * MELT_DELAY_MAX) / 2048;
				offset[x] = 0;

			}

		}

	}

	return 1;

}
