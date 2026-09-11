//===========================================================
// CAPITULO 2 - Cargar un BMP y mostrarlo
//
// En el capitulo 1 pintamos pixeles a mano. Ahora cargamos un dibujo de
// verdad desde el disco.
//
// Lo que se aprende aqui:
//
//   1. Como esta hecho por dentro un fichero BMP de 256 colores.
//   2. Que hay que cargar DOS cosas: la paleta y los pixeles. Y que si te
//      olvidas de la paleta, ves el dibujo con los colores cambiados.
//   3. Que los BMP guardan sus filas AL REVES, de abajo arriba.
//
// Del juego real se usa src/bmp.c entero. Nosotros no parseamos nada a
// mano: llamamos a las mismas funciones que llama el juego.
//===========================================================

#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include "header\bmp.h"


static void set_video_mode(unsigned int mode){

	union REGS regs;

	regs.x.ax = mode;
	int86(0x10, &regs, &regs);

}


int main(){

	printf("\n");
	printf("CAPITULO 2 - Cargar un BMP y mostrarlo\n");
	printf("\n");

	//-------------------------------------------------------
	// PASO 1: reservar los buffers.
	//
	// bmp_init_buffers() pide la memoria y, de paso, fija el tamano del
	// mundo. Aqui le pedimos un mundo de exactamente una pantalla, que es
	// como funcionaba el juego antes del mapa grande.
	//
	// Tiene que ser lo PRIMERO. Todo lo demas escribe en estos buffers.
	//-------------------------------------------------------
	printf("Reservando buffers ...\n");
	bmp_init_buffers(WIDTH, HEIGHT);

	//-------------------------------------------------------
	// PASO 2: los pixeles del dibujo.
	//
	// Van a parar a buffer_original_background_bmp, que es la copia limpia
	// del mapa: nunca se dibuja nada encima de ella.
	//-------------------------------------------------------
	printf("Cargando los pixeles de cutre.bmp ...\n");
	bmp_fill_background_in_main_buffer("..\\..\\res\\cutre.bmp");

	//-------------------------------------------------------
	// PASO 3: la paleta, del MISMO fichero.
	//
	// Esto es lo que la gente se salta. Los pixeles son numeros del 0 al
	// 255; sin la tabla que dice que color es cada numero, ves el dibujo
	// con los colores de otra imagen.
	//
	// Son dos pasos porque son dos cosas: extraer la paleta del fichero a
	// un buffer, y despues escribirla en el DAC de la tarjeta.
	//-------------------------------------------------------
	printf("Cargando la paleta del mismo fichero ...\n");
	bmp_extract_pallete_from_file("..\\..\\res\\cutre.bmp");

	printf("\n");
	printf("Pulsa una tecla. Vas a ver el mapa PRIMERO SIN PALETA, con los\n");
	printf("colores por defecto de la VGA. Pulsa otra vez y cargo la buena.\n");
	getch();

	set_video_mode(0x0013);

	//-------------------------------------------------------
	// Primero a proposito SIN escribir la paleta, para que se vea el
	// desastre.
	//
	// bmp_draw_world_window() copia el trozo visible del mapa al buffer de
	// pantalla, y bmp_paint_image_data_to_vga() vuelca ese buffer a la
	// tarjeta. Con un mundo de una pantalla, "el trozo visible" es todo.
	//-------------------------------------------------------
	bmp_draw_world_window(buffer_background_image_data);
	bmp_paint_image_data_to_vga(buffer_background_image_data);

	getch();

	//-------------------------------------------------------
	// Y ahora la paleta de verdad. Fijate en que NO se vuelve a dibujar
	// nada: los 64000 bytes de la pantalla son exactamente los mismos.
	// Lo unico que cambia es la tabla de colores.
	//-------------------------------------------------------
	bmp_write_pallete_data_into_dac(buffer_palleta_data);

	getch();

	set_video_mode(0x0003);

	bmp_close_files();
	bmp_delete_buffers();

	printf("\n");
	printf("Los pixeles de la pantalla no cambiaron entre una imagen y la\n");
	printf("otra. Solo cambio la paleta. Eso es lo que significa que el byte\n");
	printf("de un pixel sea un indice y no un color.\n");
	printf("\n");

	return 0;

}
