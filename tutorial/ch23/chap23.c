//===========================================================
// CAPITULO 23 - La mascara de bits y la memoria de DOS
//
// Ultimo capitulo. Va de la pieza que hace que el mapa grande QUEPA, y de
// la leccion mas cara del proyecto entero.
//
// Lo que se aprende aqui:
//
//   1. Por que el mapa de colisiones es de UN BIT por pixel.
//   2. El techo de 64 KB de malloc, y farmalloc.
//   3. Que es un puntero HUGE y por que hace falta.
//   4. FRAGMENTACION: por que puede haber 130 KB libres y no caber 42 KB.
//
// En modo texto, para poder leer los numeros.
//===========================================================

#include <stdio.h>
#include <conio.h>
#include <alloc.h>

#include "header\bmp.h"
#include "header\players.h"


int main(){

	unsigned long before;
	unsigned long after_world;
	unsigned long world_pixels;
	int x;
	int y;
	long walls;

	printf("\n");
	printf("CAPITULO 23 - La mascara de bits y la memoria\n");
	printf("\n");

	//-------------------------------------------------------
	// EL PRESUPUESTO
	//
	// DOS en modo real tiene 640 KB, y de ahi sale TODO: el sistema, los
	// drivers, los TSR de red, tu codigo, tu pila y todo lo que reserves.
	//
	// coreleft() y farcoreleft() dicen cuanto queda. En el modelo huge de
	// Turbo C devuelven lo mismo: son el mismo deposito.
	//-------------------------------------------------------
	before = (unsigned long)farcoreleft();

	printf("  Memoria libre al arrancar: %lu bytes\n", before);
	printf("\n");

	//-------------------------------------------------------
	// EL TECHO DE 64 KB
	//
	// malloc() recibe un size_t, que en Turbo C es de 16 BITS. El numero
	// mas grande que cabe ahi es 65535.
	//
	//     malloc(64000)    bien
	//     malloc(256000)   imposible: no se puede ni pedir
	//
	// Por eso el mapa se pide con farmalloc(), que recibe un unsigned long.
	//-------------------------------------------------------
	printf("  El mapa de 640x400 son %ld bytes de dibujo.\n", 640L * 400L);
	printf("  El maximo que acepta malloc() es 65535.\n");
	printf("  Por eso el mapa se pide con farmalloc().\n");
	printf("\n");

	//-------------------------------------------------------
	// Y EL PUNTERO TIENE QUE SER HUGE
	//
	// En el 8086 una direccion son dos numeros de 16 bits:
	//
	//     fisica = segmento * 16 + desplazamiento
	//
	// La aritmetica de un puntero FAR solo toca el desplazamiento. Y el
	// desplazamiento son 16 bits, asi que al pasar de 65535 DA LA VUELTA a
	// cero en vez de llevarse una al segmento.
	//
	// En un buffer de 64000 da igual, nunca llegas. En uno de 256000 das la
	// vuelta cuatro veces y lees basura.
	//
	// Un puntero HUGE se normaliza en cada operacion: el compilador ajusta
	// segmento y desplazamiento para que el desplazamiento quede siempre
	// entre 0 y 15. Por eso buffer_original_background_bmp esta declarado
	//
	//     unsigned char huge *
	//
	// en header/bmp.h.
	//-------------------------------------------------------
	printf("  Cargando el mundo de 640x400 ...\n");

	bmp_init_buffers(640, 400);
	bmp_fill_background_in_main_buffer("..\\..\\res\\big.bmp");
	bmp_fill_background_collision_in_buffer("..\\..\\res\\bigcol.bmp");

	after_world = (unsigned long)farcoreleft();

	printf("  Memoria libre ahora      : %lu bytes\n", after_world);
	printf("  Consumido                : %lu bytes\n", before - after_world);
	printf("\n");

	//-------------------------------------------------------
	// UN BIT POR PIXEL
	//
	// El mapa de colisiones tiene que cubrir el mundo ENTERO, no solo lo
	// que se ve: en red las dos maquinas simulan los dos tanques, asi que
	// tu maquina tiene que saber si el tanque del otro, en una sala que no
	// ves, ha chocado.
	//
	// A un byte por pixel serian 256000 bytes. No caben.
	//
	// Pero mira lo que se le pregunta a ese mapa:
	//
	//     if (bmp_is_wall(x, y) == 1)
	//
	// Solo hay DOS respuestas posibles. De los 256 valores que caben en un
	// byte te importa uno. Estas gastando 8 bits para un si/no.
	//
	// Un si/no cabe en 1 bit, y en un byte caben 8 pixeles:
	//
	//     640 * 400 / 8 = 32000 bytes
	//
	// Ocho veces menos, para exactamente la misma informacion. Y ojo: son
	// LA MITAD de lo que costaba el mapa de colisiones de UNA SOLA pantalla
	// antes (64000). El mundo entero ocupa menos que una pantalla.
	//-------------------------------------------------------
	world_pixels = 640UL * 400UL;

	printf("  El mapa de colisiones:\n");
	printf("    a 1 byte por pixel : %lu bytes  (no caben)\n", world_pixels);
	printf("    a 1 BIT  por pixel : %lu bytes  (esto es lo que se usa)\n", world_pixels / 8);
	printf("\n");

	// Contar los muros preguntando pixel a pixel. Lento y da igual: es una
	// demostracion, no parte del juego.
	printf("  Contando muros en el mundo entero ...\n");

	walls = 0;

	for (y = 0; y < map_height; y++){
		for (x = 0; x < map_width; x++){
			if (bmp_is_wall(x, y) == 1){
				walls = walls + 1;
			}
		}
	}

	printf("    %ld pixeles de muro de %ld\n", walls, (long)map_width * (long)map_height);
	printf("\n");

	//-------------------------------------------------------
	// LA LECCION CARA: FRAGMENTACION
	//
	// Durante el desarrollo, el juego cargaba todo menos el ultimo efecto
	// de sonido. El log decia:
	//
	//     Sound: could not load died.wav
	//
	// Y la cuenta decia que DEBERIA caber: quedaban 129982 bytes libres y
	// el fichero pide 42090.
	//
	// No faltaba memoria. La memoria libre estaba en el sitio equivocado.
	//
	// El orden de arranque era:
	//
	//   1. farmalloc(256000)   el mapa
	//   2. malloc(64000)       la hoja de sprites
	//   3. recortar los sprites
	//   4. free(64000)         soltar la hoja   <- DEJA UN AGUJERO
	//   5. farmalloc(42090)    el WAV           <- no lo encuentra
	//
	//   +----------------------------------------------------+
	//   |  MAPA 256000 | agujero 64000 | pantalla | libre    |
	//   +----------------------------------------------------+
	//                   ^^^^^^^^^^^^^^
	//                   libre, pero enterrado en medio
	//
	// De los 129982 libres, 64000 estaban en ese agujero y el resto arriba
	// del todo. Y NO ESTAN PEGADOS.
	//
	//   MEMORIA LIBRE TOTAL NO ES MEMORIA LIBRE CONTIGUA.
	//
	// El arreglo no fue mover cosas de sitio: fue QUITAR EL AGUJERO. La
	// hoja de sprites ya no se carga en memoria (capitulo 4): cada sprite
	// se lee directamente del fichero. Al no reservarse, no hay nada que
	// liberar, y sin liberar no hay agujero.
	//
	// Regla para DOS:
	//   1. Mide, no supongas.
	//   2. La reserva mas grande, la primera, sobre un monton limpio.
	//   3. Si puedes, no liberes nada durante la ejecucion.
	//-------------------------------------------------------
	printf("  ------------------------------------------------\n");
	printf("  MEMORIA LIBRE TOTAL no es MEMORIA LIBRE CONTIGUA.\n");
	printf("\n");
	printf("  En este proyecto se perdio una tarde por eso: quedaban\n");
	printf("  129982 bytes libres y un fichero de 42090 no cabia, porque\n");
	printf("  la mayor parte de ese hueco estaba enterrado DEBAJO del\n");
	printf("  bloque del mapa.\n");
	printf("\n");
	printf("  El arreglo fue no reservar la hoja de sprites: si no reservas,\n");
	printf("  no liberas, y si no liberas no hay agujero. Por eso el\n");
	printf("  capitulo 4 lee los sprites directamente del fichero.\n");
	printf("  ------------------------------------------------\n");
	printf("\n");
	printf("  Aqui acaba el curso. Ya has visto entero el camino desde una\n");
	printf("  pantalla en negro hasta dos tanques peleando en red por un\n");
	printf("  mundo de cuatro pantallas.\n");
	printf("\n");
	printf("  Pulsa una tecla.\n");

	getch();

	bmp_close_files();
	bmp_delete_buffers();

	printf("\n");

	return 0;

}
