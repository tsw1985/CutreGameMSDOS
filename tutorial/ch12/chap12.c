//===========================================================
// CAPITULO 12 - Reproducir un WAV
//
// Lo que se aprende aqui:
//
//   1. Que load_sound() y play_sound() son dos cosas distintas, y por que.
//   2. QUE ES EL DMA, y por que el sonido no se puede reproducir a mano.
//   3. Por que hay que llamar a sound_update() en cada vuelta del bucle.
//   4. Que pasa si NO lo llamas.
//
// Se usa src/sound.c, la libreria real.
//===========================================================

#include <stdio.h>
#include <conio.h>

#include "header\sound.h"


static void my_log(char *message){

	printf("    [sound] %s\n", message);

}


int main(){

	int fire;
	int died;
	int key;
	int update_enabled;

	printf("\n");
	printf("CAPITULO 12 - Reproducir un WAV\n");
	printf("\n");

	sound_set_log(my_log);

	if (sound_start() == 0){
		printf("\n  No hay tarjeta de sonido. Mira el capitulo 11.\n\n");
		return 1;
	}

	//-------------------------------------------------------
	// CARGAR no es SONAR.
	//
	// load_sound() abre el WAV, lo mete en memoria, lo convierte a 44100 Hz
	// si hiciera falta, y devuelve un NUMERO. Eso tarda: hay disco de por
	// medio.
	//
	// play_sound() coge ese numero y lo pone a sonar. Eso es instantaneo.
	//
	// Por eso se cargan todos los sonidos al arrancar y durante la partida
	// solo se llama a play_sound(). Si cargaras el WAV en el momento del
	// disparo, el juego se pararia medio segundo cada vez.
	//
	// Si devuelve -1 es que no pudo. Y fijate en que eso NO se comprueba
	// antes de cada play_sound(): play_sound(-1) no hace nada y ya esta.
	// Asi el juego suena igual de bien con los ficheros que falten.
	//-------------------------------------------------------
	printf("\n  Cargando WAVs ...\n");

	fire = load_sound("..\\..\\res\\fire.wav");
	died = load_sound("..\\..\\res\\died.wav");

	printf("  fire.wav -> id %d\n", fire);
	printf("  died.wav -> id %d\n", died);

	//-------------------------------------------------------
	// El volumen es por SONIDO, no global. Va de 0 a SOUND_VOLUME_MAX.
	//-------------------------------------------------------
	set_sound_volume(fire, 32);
	set_sound_volume(died, 40);

	printf("\n");
	printf("  1 = disparo     2 = explosion\n");
	printf("  U = activar/desactivar sound_update()   <- prueba esto\n");
	printf("  ESC = salir\n");
	printf("\n");

	update_enabled = 1;

	//-------------------------------------------------------
	// EL BUCLE, y la razon de que exista sound_update().
	//
	// La tarjeta no sabe leer de tus arrays. Lo que hace es DMA: acceso
	// directo a memoria. Tu le dices "empieza a leer en esta direccion,
	// tantos bytes, a esta velocidad" y a partir de ahi la tarjeta va
	// sacando esos bytes por el altavoz ELLA SOLA, sin el procesador.
	//
	// Ese buffer es pequeno: 4096 bytes, o sea menos de una decima de
	// segundo. Y esta partido en dos mitades.
	//
	// Cuando la tarjeta termina una mitad, dispara una interrupcion. La
	// rutina de sound.c apunta "toca rellenar la mitad 0" y se va
	// inmediatamente: NO mezcla ahi dentro, porque una interrupcion tiene
	// que durar lo minimo.
	//
	// Y entonces sound_update(), desde tu bucle normal, ve esa nota y
	// rellena la mitad que toque mezclando todos los sonidos que esten
	// sonando.
	//
	// Por eso HAY QUE LLAMARLO EN CADA VUELTA. Si no, la tarjeta vuelve a
	// leer lo que ya habia en el buffer y oyes el ultimo trocito repetido.
	// Pulsa U y escuchalo.
	//-------------------------------------------------------
	do {

		if (update_enabled == 1){
			sound_update();
		}

		if (kbhit()){

			key = getch();

			if (key == '1'){ play_sound(fire); }
			if (key == '2'){ play_sound(died); }

			if (key == 'u' || key == 'U'){
				update_enabled = !update_enabled;
				if (update_enabled == 1){
					printf("  sound_update() ACTIVADO\n");
				}else{
					printf("  sound_update() DESACTIVADO -> dispara y escucha\n");
				}
			}

			if (key == 27){ break; }

		}

	} while (1);

	sound_end();

	printf("\n");
	printf("  Reproducir sonido es rellenar un buffer pequeno antes de que\n");
	printf("  la tarjeta se lo acabe. Nada mas.\n");
	printf("\n");

	return 0;

}
