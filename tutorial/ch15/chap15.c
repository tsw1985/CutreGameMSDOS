//===========================================================
// CAPITULO 15 - Encontrar el driver IPX
//
// Empieza el bloque de red. Este capitulo no manda nada todavia: solo
// encuentra el driver y abre un socket. En modo texto, para poder leer.
//
// Lo que se aprende aqui:
//
//   1. Que es IPX y por que no es TCP/IP.
//   2. Como se averigua si hay driver: el INT 2F.
//   3. Que es un socket y para que sirve.
//   4. Que es un "far call" y por que IPX no usa una interrupcion.
//
// Se usa src/net.c, la libreria real del juego.
//===========================================================

#include <stdio.h>
#include <conio.h>

#include "header\net.h"


static void my_log(char *message){

	printf("    [net] %s\n", message);

}


int main(){

	printf("\n");
	printf("CAPITULO 15 - Encontrar el driver IPX\n");
	printf("\n");

	//-------------------------------------------------------
	// QUE ES IPX
	//
	// En 1995 no habia TCP/IP en DOS de serie. Lo que habia en las oficinas
	// eran redes Novell NetWare, y su protocolo se llamaba IPX.
	//
	// IPX NO es TCP/IP, y las diferencias importan:
	//
	//   - No hay conexiones. No existe "conectar con". Solo mandas paquetes
	//     sueltos, como echar cartas al buzon.
	//   - No hay garantia de entrega. Un paquete puede perderse y nadie te
	//     avisa.
	//   - No hay garantia de orden. Pueden llegar cambiados.
	//   - No hay direcciones que escribir. La direccion de una maquina es la
	//     MAC de su tarjeta, y se descubre sola (capitulo 17).
	//
	// Suena peor que TCP/IP. Para un juego es MEJOR: no quieres que un
	// paquete perdido pare la partida para reenviarlo, quieres seguir.
	//-------------------------------------------------------

	net_set_log(my_log);

	//-------------------------------------------------------
	// COMO SE ENCUENTRA EL DRIVER
	//
	// IPX es un TSR: un programa cargado antes que el tuyo que se queda
	// residente en memoria. Hay que preguntarle a DOS si esta.
	//
	// El convenio es el INT 2F, la "interrupcion multiplexada", que es el
	// tablon de anuncios de los TSR: cada uno tiene su numero y contesta si
	// esta.
	//
	//   AX = 0x7A00  ->  "IPX, estas ahi?"
	//
	// Si vuelve con AL = 0xFF, esta. Y ademas te deja en ES:DI la direccion
	// de su punto de entrada.
	//
	// Y AQUI viene lo raro: a IPX NO se le llama con una interrupcion. Se
	// le llama con un FAR CALL a esa direccion, como a una funcion normal
	// que estuviera en otro segmento. Es la unica API de DOS que funciona
	// asi, y es lo que hace que net.c tenga ensamblador.
	//
	// Que funcion quieres se pone en BX:
	//
	//   0x0000  abrir socket      0x0003  enviar
	//   0x0001  cerrar socket     0x0004  escuchar
	//   0x0009  tu direccion      0x000A  ceder el control
	//-------------------------------------------------------
	printf("  Llamando a net_start() ...\n");
	printf("\n");

	if (net_start() == 1){

		printf("\n");
		printf("  DRIVER ENCONTRADO Y SOCKET ABIERTO.\n");
		printf("\n");

		//---------------------------------------------------
		// QUE ES UN SOCKET
		//
		// Un numero de 16 bits que identifica "de que va" un paquete.
		//
		// Por el mismo cable pasan paquetes de todo: del juego, de otro
		// juego, del servidor de ficheros. Todos llegan a tu tarjeta. El
		// socket es lo que te deja quedarte solo con los tuyos.
		//
		// net.c usa el 0x869C. Cualquier numero de 0x8000 arriba vale:
		// Novell reservo ese rango para programas no registrados.
		//
		// Si otro programa ya lo tiene cogido, abrir falla. Por eso
		// net_start() puede decir "puede que haya otra copia corriendo".
		//---------------------------------------------------
		printf("  Socket 0x869C abierto. A partir de ahora, todo paquete\n");
		printf("  que llegue a este socket es para nosotros, y el resto del\n");
		printf("  trafico de la red se ignora solo.\n");
		printf("\n");
		printf("  Nuestra identificacion en la red:\n");
		printf("    id local = %lu\n", net_get_local_id());
		printf("\n");
		printf("  Ese numero es aleatorio y se usa en el capitulo 17 para\n");
		printf("  decidir quien es el jugador 1 sin gastar ni un paquete.\n");

		net_end();

		printf("\n");
		printf("  net_end() ha cerrado el socket. Hay que hacerlo: un socket\n");
		printf("  que se queda abierto no lo puede usar el siguiente\n");
		printf("  programa.\n");

	}else{

		printf("\n");
		printf("  No hay driver IPX.\n");
		printf("\n");
		printf("  En DOSBox: pon  ipx=true  en la seccion [ipx] de\n");
		printf("  dosbox.conf, y luego dentro de DOSBox:\n");
		printf("      ipxnet startserver        en una maquina\n");
		printf("      ipxnet connect <su ip>    en la otra\n");
		printf("\n");
		printf("  Lo mas facil: usa launch_game_both.sh desde Linux, que\n");
		printf("  levanta las dos ventanas ya conectadas.\n");
		printf("\n");
		printf("  En DOS real: carga LSL, el driver ODI de tu tarjeta e\n");
		printf("  IPXODI antes de ejecutar esto.\n");

	}

	printf("\n");

	return 0;

}
