//===========================================================
// CAPITULO 11 - Encontrar la Sound Blaster
//
// Este capitulo no pinta nada: se queda en modo texto para que puedas leer
// lo que va pasando. Es el unico asi.
//
// Lo que se aprende aqui:
//
//   1. Que en DOS no hay drivers: el programa habla con la tarjeta.
//   2. Como se averigua DONDE esta la tarjeta: la variable BLASTER.
//   3. Que es un puerto de E/S y que es el DSP.
//   4. Por que sound.c avisa por una funcion que le pasas tu.
//
// Se usa src/sound.c, la libreria real del juego, entera.
//===========================================================

#include <stdio.h>
#include <conio.h>
#include <stdlib.h>

#include "header\sound.h"


//===========================================================
// El log.
//
// sound.c no sabe escribir en ningun sitio. No hace printf, no abre
// ficheros, no conoce game.log. Lo unico que hace es llamar a una funcion
// que le des tu.
//
// El juego le pasa tanks_log(), que escribe en game.log. Nosotros le
// pasamos esta, que imprime en pantalla, porque en este capitulo seguimos
// en modo texto.
//
// Eso es lo que hace que sound.c sea una LIBRERIA y no "el sonido de este
// juego": no impone nada sobre el programa que la usa.
//===========================================================
static void my_log(char *message){

	printf("    [sound] %s\n", message);

}


int main(){

	char *blaster;

	printf("\n");
	printf("CAPITULO 11 - Encontrar la Sound Blaster\n");
	printf("\n");

	//-------------------------------------------------------
	// PASO 1: mirar la variable de entorno BLASTER.
	//
	// En DOS no hay nadie que sepa que hardware tienes. Ni sistema de
	// plug and play, ni registro, ni drivers. Cada programa se busca la
	// vida.
	//
	// El convenio que se impuso es una variable de entorno que pone el
	// propio instalador de la tarjeta:
	//
	//     SET BLASTER=A220 I5 D1 H5 P330 T6
	//
	//   A220  puerto base, en hexadecimal
	//   I5    numero de IRQ (la interrupcion que usa)
	//   D1    canal de DMA de 8 bits
	//   H5    canal de DMA de 16 bits
	//   P330  puerto MIDI
	//   T6    tipo de tarjeta
	//
	// De todo eso, a sound.c le importan A, I y D.
	//-------------------------------------------------------
	blaster = getenv("BLASTER");

	if (blaster != NULL){
		printf("  BLASTER = %s\n", blaster);
	}else{
		printf("  BLASTER no esta puesta. sound.c probara con A220 I5 D1,\n");
		printf("  que es la configuracion mas comun.\n");
	}

	printf("\n");

	//-------------------------------------------------------
	// PASO 2: decirle a sound.c por donde avisar. ANTES de arrancarlo, o
	// te pierdes los mensajes del propio arranque.
	//-------------------------------------------------------
	sound_set_log(my_log);

	//-------------------------------------------------------
	// PASO 3: arrancar.
	//
	// sound_start() hace, por dentro:
	//
	//   1. Lee BLASTER (o usa A220 I5 D1).
	//   2. RESETEA el DSP: escribe un 1 en el puerto base+6, espera, y
	//      escribe un 0. Si hay una tarjeta, contesta 0xAA por el puerto de
	//      lectura. Ese 0xAA es toda la deteccion: si llega, hay tarjeta.
	//   3. Reserva el buffer del DMA (capitulo 12).
	//   4. Instala su rutina de interrupcion en la IRQ.
	//
	// Un "puerto" es una direccion, pero de un espacio aparte del de la
	// memoria: se lee y se escribe con instrucciones distintas (IN y OUT,
	// que en Turbo C son inportb() y outportb()). Es por donde se habla con
	// el hardware.
	//
	// El DSP es el trozo de la tarjeta que se encarga del sonido digital.
	// Se le mandan ordenes de un byte por esos puertos.
	//-------------------------------------------------------
	printf("  Llamando a sound_start() ...\n");
	printf("\n");

	if (sound_start() == 1){

		printf("\n");
		printf("  TARJETA ENCONTRADA.\n");
		printf("\n");
		printf("  Y ahora lo importante: si NO la hubiera encontrado, el\n");
		printf("  juego funcionaria exactamente igual, en silencio. Todas\n");
		printf("  las llamadas de sonido comprueban si hay tarjeta y no\n");
		printf("  hacen nada si no la hay.\n");
		printf("\n");
		printf("  El sonido es un extra, no un requisito. Nunca dejes que la\n");
		printf("  falta de una tarjeta impida jugar.\n");

		sound_end();

		printf("\n");
		printf("  sound_end() ha devuelto la IRQ y ha parado el DMA. Eso hay\n");
		printf("  que hacerlo SIEMPRE antes de salir: si dejas el DMA\n");
		printf("  corriendo, la tarjeta sigue leyendo una memoria que ya no\n");
		printf("  es tuya y oyes ruido hasta que reinicias.\n");

	}else{

		printf("\n");
		printf("  No hay tarjeta (o no responde).\n");
		printf("\n");
		printf("  En DOSBox: mira que en dosbox.conf tengas\n");
		printf("      [sblaster]\n");
		printf("      sbtype=sb16\n");
		printf("      sbbase=220\n");
		printf("      irq=7\n");
		printf("      dma=1\n");

	}

	printf("\n");

	return 0;

}
