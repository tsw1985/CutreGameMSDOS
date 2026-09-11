//===========================================================
// CAPITULO 13 - Mezclar: varios sonidos a la vez
//
// Lo que se aprende aqui:
//
//   1. QUE ES MEZCLAR y por que hay que hacerlo por software.
//   2. Que es el recorte (clipping) del sonido y a que suena.
//   3. La diferencia entre un sonido de una vez y uno en bucle.
//   4. Por que un bucle necesita que alguien lo pare.
//===========================================================

#include <stdio.h>
#include <conio.h>

#include "header\sound.h"


static void my_log(char *message){

	printf("    [sound] %s\n", message);

}


int main(){

	int fire;
	int engine1;
	int engine2;
	int died;
	int key;
	int engine1_on;
	int engine2_on;

	printf("\n");
	printf("CAPITULO 13 - Mezclar\n");
	printf("\n");

	sound_set_log(my_log);

	if (sound_start() == 0){
		printf("\n  No hay tarjeta. Capitulo 11.\n\n");
		return 1;
	}

	fire    = load_sound("..\\..\\res\\fire.wav");
	engine1 = load_sound("..\\..\\res\\engip1.wav");
	engine2 = load_sound("..\\..\\res\\engip2.wav");
	died    = load_sound("..\\..\\res\\died.wav");

	//-------------------------------------------------------
	// LOS VOLUMENES, y aqui hay una leccion.
	//
	// Los motores van bajos (12) a proposito. Dos motores a tope mas un
	// disparo se suman, y la suma se sale del rango que cabe en un byte.
	// Cuando eso pasa, el mezclador tiene que RECORTAR: dejarlo en el
	// maximo. Y el recorte suena a distorsion sucia.
	//
	// Estos numeros son los del juego real, de src/main.c.
	//-------------------------------------------------------
	set_sound_volume(fire,    16);
	set_sound_volume(engine1, 12);
	set_sound_volume(engine2, 12);
	set_sound_volume(died,    34);

	printf("\n");
	printf("  1 = disparo        3 = motor 1 (bucle)\n");
	printf("  2 = explosion      4 = motor 2 (bucle)\n");
	printf("  5 = subir los motores a tope  <- escucha la distorsion\n");
	printf("  0 = parar todo     ESC = salir\n");
	printf("\n");
	printf("  Enciende los dos motores y dispara encima. Los tres suenan a\n");
	printf("  la vez: eso es la mezcla.\n");
	printf("\n");

	engine1_on = 0;
	engine2_on = 0;

	do {

		sound_update();

		if (kbhit()){

			key = getch();

			//-----------------------------------------------
			// play_sound(): suena una vez y se acaba sola.
			//
			// Si la pulsas otra vez antes de que termine, suenan las DOS
			// copias a la vez. Eso es lo normal en un disparo.
			//-----------------------------------------------
			if (key == '1'){ play_sound(fire); }
			if (key == '2'){ play_sound(died); }

			//-----------------------------------------------
			// loop_sound(): suena y VUELVE A EMPEZAR, para siempre.
			//
			// Un bucle no se acaba solo: alguien tiene que pararlo. En el
			// juego, el motor arranca cuando pulsas una flecha y para
			// cuando la sueltas.
			//
			// Y stop_looping_sound() no es opcional: si el tanque muere y
			// nadie lo para, el motor se queda sonando mientras arde.
			//-----------------------------------------------
			if (key == '3'){
				if (engine1_on == 0){
					loop_sound(engine1);
					engine1_on = 1;
					printf("  motor 1 ON\n");
				}else{
					stop_looping_sound(engine1);
					engine1_on = 0;
					printf("  motor 1 OFF\n");
				}
			}

			if (key == '4'){
				if (engine2_on == 0){
					loop_sound(engine2);
					engine2_on = 1;
					printf("  motor 2 ON\n");
				}else{
					stop_looping_sound(engine2);
					engine2_on = 0;
					printf("  motor 2 OFF\n");
				}
			}

			//-----------------------------------------------
			// La demostracion del recorte. Sube los motores al maximo y
			// dispara encima: la suma se sale de rango, el mezclador
			// recorta, y se oye sucio.
			//
			// Por eso los volumenes del juego estan bajos.
			//-----------------------------------------------
			if (key == '5'){
				set_sound_volume(engine1, SOUND_VOLUME_MAX);
				set_sound_volume(engine2, SOUND_VOLUME_MAX);
				set_sound_volume(fire,    SOUND_VOLUME_MAX);
				printf("  volumenes al maximo: dispara con los dos motores\n");
			}

			if (key == '0'){
				stop_all_sounds();
				engine1_on = 0;
				engine2_on = 0;
				printf("  todo parado\n");
			}

			if (key == 27){ break; }

		}

	} while (1);

	sound_end();

	printf("\n");
	printf("  La tarjeta solo tiene UN canal de 8 bits. Todo lo que oyes a\n");
	printf("  la vez lo ha sumado sound_update() a mano, muestra a muestra,\n");
	printf("  antes de dejarselo a la tarjeta.\n");
	printf("\n");

	return 0;

}
