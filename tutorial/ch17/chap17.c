//===========================================================
// CAPITULO 17 - Enviar y recibir: un chat
//
// Ya se encuentran. Ahora que se digan algo.
//
// Lo que se aprende aqui:
//
//   1. Que net_send() manda un MENSAJE, no un flujo de bytes.
//   2. Por que hay que llamar a net_update() en cada vuelta.
//   3. Que lo que envias puede perderse, y que eso es normal.
//   4. Que puedes mandar lo que te de la gana, no solo texto.
//
// Dos maquinas:  ./launch_game_both.sh  y en las dos  cd tutorial\ch17
//===========================================================

#include <stdio.h>
#include <conio.h>
#include <string.h>

#include "header\net.h"


static void my_log(char *message){

	printf("    [net] %s\n", message);

}


int main(){

	char line[80];
	char incoming[NET_MAX_DATA];
	int length;
	int position;
	int key;
	unsigned long sent;
	unsigned long received;

	printf("\n");
	printf("CAPITULO 17 - Un chat\n");
	printf("\n");

	net_set_log(my_log);

	if (net_start() == 0){
		printf("\n  No hay driver IPX. Capitulo 15.\n\n");
		return 1;
	}

	printf("\n  Buscando a la otra maquina...\n\n");

	if (net_find_peer(30) == 0){
		printf("\n  No aparecio nadie.\n\n");
		net_end();
		return 1;
	}

	printf("\n");
	printf("  Escribe y pulsa ENTER. ESC para salir.\n");
	printf("  ------------------------------------------------\n");

	position = 0;
	sent = 0;
	received = 0;

	do {

		//---------------------------------------------------
		// net_update() ES OBLIGATORIO EN CADA VUELTA.
		//
		// net.c deja cuatro buzones preparados con el driver. Cuando llega
		// un paquete, el driver lo mete en uno y lo marca como lleno.
		//
		// net_update() es quien va a mirar esos cuatro buzones, se lleva lo
		// que haya, y vuelve a dejarlos vacios y preparados.
		//
		// Si no lo llamas, los cuatro se llenan y a partir de ahi TODO LO
		// QUE LLEGUE SE PIERDE. El driver no guarda nada por su cuenta.
		//
		// Se llama "poll", sondear: la iniciativa es tuya. IPX ofrece lo
		// contrario (que te avise con una interrupcion) y net.c no lo usa a
		// proposito, porque esa rutina saltaria en mitad de cualquier cosa.
		//---------------------------------------------------
		net_update();

		//---------------------------------------------------
		// RECIBIR
		//
		// net_receive() devuelve UN mensaje completo, o 0 si no hay nada.
		// Nunca medio mensaje, y nunca dos pegados.
		//
		// Eso es lo que significa "datagrama", y es la diferencia gorda con
		// TCP: en TCP tu mandas 10 bytes y 5 bytes, y el otro puede leer 15
		// de golpe, o 3 y 12. Tienes que inventarte tu donde acaba cada
		// mensaje. Aqui no: un net_send() es un net_receive().
		//
		// El bucle while es porque en una sola vuelta pueden haber llegado
		// varios.
		//---------------------------------------------------
		length = net_receive(incoming, NET_MAX_DATA);

		while (length > 0){

			incoming[length] = 0;
			printf("\n  EL OTRO> %s\n  > ", incoming);
			received = received + 1;

			length = net_receive(incoming, NET_MAX_DATA);

		}

		//---------------------------------------------------
		// ENVIAR
		//---------------------------------------------------
		if (kbhit()){

			key = getch();

			if (key == 27){ break; }

			if (key == 13){

				if (position > 0){

					line[position] = 0;

					// net_send() devuelve 1 si lo entrego al driver. Eso NO
					// quiere decir que llegue: IPX no garantiza nada. Puede
					// perderse y nadie te avisa.
					//
					// Para un chat eso es un problema. Para un juego no: es
					// preferible perder un paquete y seguir que parar la
					// partida para reenviarlo. El capitulo 18 explica como
					// se aguanta esa perdida sin reenviar nada.
					if (net_send(line, position) == 1){
						sent = sent + 1;
					}else{
						printf("\n  [no pude enviar]\n");
					}

					position = 0;
					printf("\n  > ");

				}

			}else if (key == 8){

				if (position > 0){
					position = position - 1;
					printf("\b \b");
				}

			}else if (key >= 32 && position < 70){

				line[position] = (char)key;
				position = position + 1;
				putch(key);

			}

		}

		if (net_connection_lost() == 1){
			printf("\n\n  Se ha perdido la conexion.\n");
			break;
		}

	} while (1);

	printf("\n");
	printf("  ------------------------------------------------\n");
	printf("  enviados: %lu    recibidos: %lu\n", sent, received);
	printf("\n");
	printf("  Y esto no es solo para texto. net_send() manda BYTES: hasta\n");
	printf("  %d de una vez. Pueden ser 3 bytes que signifiquen algo para\n", NET_MAX_DATA);
	printf("  ti, una struct, o un trozo de un fichero. El capitulo 18 manda\n");
	printf("  justo eso: una struct de 20 bytes con teclas de tanque.\n");
	printf("\n");

	net_end();

	return 0;

}
