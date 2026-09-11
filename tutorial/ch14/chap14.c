//===========================================================
// CAPITULO 14 - Musica: reproducir desde el disco
//
// Lo que se aprende aqui:
//
//   1. Por que una cancion NO se puede cargar como un efecto.
//   2. Que es el STREAMING y por que una cancion de un minuto cuesta lo
//      mismo que una de cinco segundos.
//   3. Por que la cancion tiene que estar EXACTAMENTE a 44100 Hz cuando los
//      efectos no.
//===========================================================

#include <stdio.h>
#include <conio.h>

#include "header\sound.h"


static void my_log(char *message){

	printf("    [sound] %s\n", message);

}


int main(){

	int fire;
	int key;

	printf("\n");
	printf("CAPITULO 14 - Musica\n");
	printf("\n");

	sound_set_log(my_log);

	if (sound_start() == 0){
		printf("\n  No hay tarjeta. Capitulo 11.\n\n");
		return 1;
	}

	fire = load_sound("..\\..\\res\\fire.wav");
	set_sound_volume(fire, 32);

	//-------------------------------------------------------
	// LA CUENTA QUE OBLIGA A HACERLO DISTINTO.
	//
	// res\prody8.wav ocupa 2.719.788 bytes. Casi 2,7 MB.
	//
	// Una maquina DOS tiene 640 KB de memoria convencional. La cancion es
	// CUATRO VECES el total de la memoria de la maquina, sistema operativo
	// incluido.
	//
	// load_sound() no puede con eso, y no por un fallo: es fisicamente
	// imposible.
	//
	// La solucion es no cargarla: leer un trocito, reproducirlo, leer el
	// siguiente, y asi. Eso es el STREAMING.
	//
	// El trozo son 16384 bytes: 0,37 segundos a 44100 Hz. Un buffer fijo.
	// Cuando se acaba, se lee el siguiente trozo del fichero. Cuando se
	// acaba el fichero, se vuelve al principio y la cancion se repite.
	//
	//   Un efecto:   memoria = el tamano del WAV
	//   Una cancion: memoria = 16 KB, dure lo que dure
	//
	// Una cancion de una hora costaria los mismos 16 KB.
	//-------------------------------------------------------
	printf("\n");
	printf("  Arrancando la cancion (2,7 MB, leida del disco a trozos)...\n");
	printf("\n");

	if (play_song("..\\..\\res\\prody8.wav") == 0){
		printf("  No pude arrancarla. Mira arriba el motivo.\n");
	}

	//-------------------------------------------------------
	// EL REQUISITO QUE PILLA A TODO EL MUNDO.
	//
	// load_sound() acepta un WAV a cualquier frecuencia: lo convierte a
	// 44100 al cargarlo. Lo hace UNA VEZ, al arrancar, y le da igual
	// tardar.
	//
	// play_song() NO puede hacer eso. Va leyendo el fichero mientras suena,
	// y no hay ningun sitio donde convertir sobre la marcha: no hay tiempo
	// y no hay memoria.
	//
	// Asi que la cancion tiene que venir ya a 44100 Hz, mono, 8 bits. Si no
	// lo esta, play_song() lo dice en el log y no suena.
	//-------------------------------------------------------
	printf("  1   = disparo (encima de la musica)\n");
	printf("  +/- = volumen de la musica\n");
	printf("  P   = parar la musica\n");
	printf("  ESC = salir\n");
	printf("\n");
	printf("  La musica y los efectos pasan por el MISMO mezclador: los\n");
	printf("  efectos se oyen por encima sin cortarla.\n");
	printf("\n");

	do {

		// El mismo sound_update() de siempre. Por dentro, ademas de
		// mezclar, es el que rellena el buffer de la cancion leyendo mas
		// fichero cuando hace falta.
		sound_update();

		if (kbhit()){

			key = getch();

			if (key == '1'){ play_sound(fire); }

			if (key == '+'){ set_song_volume(48); printf("  volumen alto\n"); }
			if (key == '-'){ set_song_volume(8);  printf("  volumen bajo\n"); }

			if (key == 'p' || key == 'P'){
				stop_song();
				printf("  musica parada\n");
			}

			if (key == 27){ break; }

		}

	} while (1);

	sound_end();

	printf("\n");
	printf("  2,7 MB sonando en una maquina de 640 KB, con 16 KB de buffer.\n");
	printf("  Con esto el sonido esta terminado. Capitulo 15: la red.\n");
	printf("\n");

	return 0;

}
