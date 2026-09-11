//===========================================================
// CAPITULO 16 - Que dos maquinas se encuentren
//
// IPX no tiene conexiones y no hay ninguna IP que escribir. Entonces, como
// sabe una maquina donde esta la otra?
//
// Lo que se aprende aqui:
//
//   1. Que es un BROADCAST y por que resuelve el problema.
//   2. El protocolo HELLO / HELLO_ACK.
//   3. Como se decide quien es el jugador 1 SIN gastar ni un paquete.
//   4. Las tres formas de emparejar: servidor, cliente e iguales.
//
// Para probarlo hacen falta DOS maquinas. Lo mas facil, desde Linux:
//     ./launch_game_both.sh
// y en las dos ventanas, cd tutorial\ch16  y  chap16
//===========================================================

#include <stdio.h>
#include <conio.h>

#include "header\net.h"


static void my_log(char *message){

	printf("    [net] %s\n", message);

}


int main(){

	printf("\n");
	printf("CAPITULO 16 - Encontrarse\n");
	printf("\n");

	net_set_log(my_log);

	if (net_start() == 0){
		printf("\n  No hay driver IPX. Mira el capitulo 15.\n\n");
		return 1;
	}

	printf("\n");
	printf("  Mi id: %lu\n", net_get_local_id());
	printf("\n");

	//-------------------------------------------------------
	// EL PROBLEMA
	//
	// En TCP/IP escribes una IP. En IPX no hay IP: la direccion de una
	// maquina es la MAC de su tarjeta, doce digitos hexadecimales que nadie
	// se sabe de memoria y que cambian si cambias de tarjeta.
	//
	// Obligar al jugador a escribir eso seria horrible.
	//
	// LA SOLUCION: el broadcast.
	//
	// Existe una direccion especial, FF:FF:FF:FF:FF:FF, que significa
	// "todas las maquinas de este segmento de red". Mandas ahi y le llega a
	// todo el mundo.
	//
	// Entonces el protocolo es:
	//
	//   1. Grito HELLO a todos, cada cuarto de segundo.
	//   2. Si alguien me oye, me contesta HELLO_ACK, y en esa respuesta
	//      viene SU direccion, porque todo paquete IPX lleva quien lo
	//      manda.
	//   3. Ya se donde esta. A partir de ahi le hablo solo a el.
	//
	// Nadie ha escrito nada. Ese es el objetivo.
	//-------------------------------------------------------
	printf("  Buscando a alguien (30 segundos)...\n");
	printf("  Arranca este mismo programa en la otra maquina.\n");
	printf("\n");

	//-------------------------------------------------------
	// LAS TRES FORMAS DE EMPAREJAR
	//
	// net.c ofrece tres puertas al mismo mecanismo:
	//
	//   net_wait_for_client(s)     escucha y no grita.   "servidor"
	//   net_connect_to_server(s)   grita hasta que le contestan. "cliente"
	//   net_find_peer(s)           grita Y escucha.  Dos iguales.
	//
	// Las tres usan el mismo bucle por dentro. "Servidor" y "cliente" son
	// un convenio construido encima de IPX, no algo que IPX tenga: en IPX
	// las dos maquinas son exactamente iguales.
	//
	// Para un juego de dos, net_find_peer() es lo comodo: arrancas las dos
	// en cualquier orden y se encuentran.
	//-------------------------------------------------------
	if (net_find_peer(30) == 0){
		printf("\n  No aparecio nadie.\n\n");
		net_end();
		return 1;
	}

	printf("\n");
	printf("  EMPAREJADOS.\n");
	printf("\n");
	printf("    mi id  : %lu\n", net_get_local_id());
	printf("    su id  : %lu\n", net_get_remote_id());
	printf("\n");

	//-------------------------------------------------------
	// QUIEN ES EL JUGADOR 1, SIN GASTAR UN PAQUETE
	//
	// Alguien tiene que llevar el tanque azul y alguien el rojo, y las dos
	// maquinas tienen que estar de acuerdo.
	//
	// Se podria negociar: "yo quiero ser el 1", "vale, pues yo el 2"...
	// paquetes, esperas, y un caso raro si los dos piden lo mismo a la vez.
	//
	// Se hace mucho mas simple: cada maquina saco un numero aleatorio al
	// arrancar, y en el emparejamiento cada una ya conoce los DOS numeros.
	// Asi que las dos aplican la misma regla:
	//
	//     el del numero mas pequeno es el jugador 1
	//
	// Las dos hacen la misma cuenta con los mismos datos, asi que llegan a
	// la misma conclusion. Cero paquetes, cero esperas, cero casos raros.
	//
	// Este truco -- que las dos partes calculen lo mismo en vez de
	// preguntarselo -- es exactamente la idea del lockstep del capitulo 18,
	// en pequeno.
	//-------------------------------------------------------
	if (net_get_local_id() < net_get_remote_id()){
		printf("    Soy el JUGADOR 1 (mi id es menor)\n");
	}else{
		printf("    Soy el JUGADOR 2 (mi id es mayor)\n");
	}

	printf("\n");
	printf("  Mira la otra pantalla: dice justo lo contrario, y las dos lo\n");
	printf("  han decidido solas.\n");
	printf("\n");
	printf("  Pulsa una tecla para salir.\n");

	getch();

	net_end();

	printf("\n");

	return 0;

}
