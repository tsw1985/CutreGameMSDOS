//===========================================================
// CYCLE - la imagen quieta y los colores girando
//
// El efecto mas de la epoca de todos, y el unico de esta coleccion que no
// mueve ni un pixel: la imagen se pinta UNA sola vez y lo que gira es la
// PALETA. El color 1 pasa a mostrarse con lo que era el 2, el 2 con el 3,
// y asi.
//
// Con esto se hacian las cascadas que caian, el fuego que ardia y el agua
// que corria en juegos que no podian permitirse redibujar la pantalla. En
// una foto normal sale un efecto psicodelico; en un dibujo hecho a
// proposito, con los colores en rampa, parece animacion de verdad.
//
// Coste: cero pixeles por frame. Solo las 1024 escrituras al DAC de
// siempre, que ya se hacen en cualquier fundido.
//
// El color 0 NO gira. Es el negro del fondo en todas las paletas de este
// proyecto, y si girara, el fondo se pondria a parpadear de colores.
//===========================================================

#include "demos\demolib.h"
#include "demos\cycle\cycle.h"

// Cada cuantos frames se gira un puesto. 1 marea; 3 se ve bien.
#define CYCLE_EVERY 	3

// Bytes de una entrada de paleta: azul, verde, rojo
#define CYCLE_ENTRY 	3

// Cuantas entradas giran: de la 1 a la 255. La 0 se queda quieta.
#define CYCLE_FIRST 	1
#define CYCLE_COUNT 	255


int demo_cycle(unsigned char *image,
               unsigned char *screen,
               unsigned char *palette,
               unsigned long end_tick)
{
	unsigned char saved[CYCLE_ENTRY];
	int counter;
	int i;
	int first;
	int last;

	// La imagen se pinta una vez y no se vuelve a tocar en todo el efecto
	memcpy(screen, image, DEMO_SCREEN);
	demo_show(screen);

	counter = 0;

	while (demo_now() < end_tick){

		if (demo_escape_pressed() == 1){
			// Dejar la paleta como estaba, o la imagen siguiente heredaria
			// los colores girados a medias
			return 0;
		}

		demo_wait_retrace();

		counter++;


		if (counter >= CYCLE_EVERY){

			counter = 0;

			//-----------------------------------------------
			// Girar un puesto: se guarda la primera entrada, se arrastran
			// todas una posicion hacia abajo, y la guardada va al final.
			//
			// Es un memmove disfrazado, pero escrito a mano porque hay que
			// mover de tres en tres bytes y guardar la que se pisa.
			//-----------------------------------------------
			first = CYCLE_FIRST * CYCLE_ENTRY;
			last  = (CYCLE_FIRST + CYCLE_COUNT - 1) * CYCLE_ENTRY;

			saved[0] = palette[first + 0];
			saved[1] = palette[first + 1];
			saved[2] = palette[first + 2];

			for (i = first; i < last; i = i + CYCLE_ENTRY){
				palette[i + 0] = palette[i + CYCLE_ENTRY + 0];
				palette[i + 1] = palette[i + CYCLE_ENTRY + 1];
				palette[i + 2] = palette[i + CYCLE_ENTRY + 2];
			}

			palette[last + 0] = saved[0];
			palette[last + 1] = saved[1];
			palette[last + 2] = saved[2];

			bmp_write_pallete_data_into_dac(palette);

		}

		//---------------------------------------------------
		// Y a alimentar la tarjeta, DESPUES de escribir el DAC.
		//
		// Este efecto no puede usar demo_show(): no repinta nada, y lo
		// unico que hace en el frame es escribir la paleta, que tiene que
		// caer dentro del borrado vertical. Mezclar el sonido antes se
		// comeria esa ventana y la paleta entraria con el haz pintando.
		//---------------------------------------------------
		demo_sound();

	}

	return 1;

}
