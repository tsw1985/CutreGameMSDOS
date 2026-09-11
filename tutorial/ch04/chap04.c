//===========================================================
// CAPITULO 4 - Sprites: recortar y pintar con transparencia
//
// Ya sabemos poner un fondo. Ahora hay que poner cosas ENCIMA.
//
// Lo que se aprende aqui:
//
//   1. Que es una hoja de sprites y por que se usa una sola imagen.
//   2. Como se recorta un rectangulo de esa hoja.
//   3. Que es el color transparente y por que hace falta.
//   4. Que es el recorte (clipping) y por que sin el se cuelga la maquina.
//
// Todo con las funciones reales: bmp_open_sprite_sheet(),
// bmp_extract_sprite() y draw_sprite_to_buffer(), de src/bmp.c.
//===========================================================

#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <alloc.h>

#include "header\bmp.h"
#include "header\players.h"

static void set_video_mode(unsigned int mode){

	union REGS regs;

	regs.x.ax = mode;
	int86(0x10, &regs, &regs);

}

static void wait_retrace(void){

	while (inp(0x3DA) & 0x08);
	while (!(inp(0x3DA) & 0x08));

}


int main(){

	unsigned char *tank_up;
	unsigned char *tank_right;
	unsigned char *bullet;
	int x;

	printf("\n");
	printf("CAPITULO 4 - Sprites\n");
	printf("\n");

	bmp_init_buffers(WIDTH, HEIGHT);
	bmp_fill_background_in_main_buffer("..\\..\\res\\cutre.bmp");
	bmp_extract_pallete_from_file("..\\..\\res\\cutre.bmp");

	//-------------------------------------------------------
	// PASO 1: reservar sitio para cada sprite por separado.
	//
	// Un tanque son TANK_WIDTH x TANK_HEIGHT = 18 x 18 = 324 bytes. Esas
	// constantes vienen de header/players.h, el header real del juego.
	//-------------------------------------------------------
	tank_up    = (unsigned char*)malloc(TANK_WIDTH * TANK_HEIGHT);
	tank_right = (unsigned char*)malloc(TANK_WIDTH * TANK_HEIGHT);
	bullet     = (unsigned char*)malloc(TANK_BULLET_WIDTH * TANK_BULLET_HEIGHT);

	//-------------------------------------------------------
	// PASO 2: abrir la hoja de sprites.
	//
	// TODOS los dibujos del juego estan en un solo BMP, sprites.bmp, en una
	// rejilla. Se hace asi por dos razones: un fichero se abre una vez en
	// lugar de treinta, y todos comparten paleta por construccion.
	//
	// Fijate en que se ABRE, no se carga. El juego no guarda la hoja en
	// memoria: lee cada sprite directamente del fichero. La razon completa
	// esta en el manual de la camara, parte 9.4, y tiene que ver con no
	// dejar agujeros en la memoria.
	//-------------------------------------------------------
	bmp_open_sprite_sheet("..\\..\\res\\sprites.bmp");

	//-------------------------------------------------------
	// PASO 3: recortar. Las coordenadas son las del juego de verdad,
	// copiadas de src/main.c.
	//
	//   bmp_extract_sprite(x, y, ancho, alto, destino)
	//
	// x e y son la esquina SUPERIOR IZQUIERDA del recorte dentro de la hoja,
	// contando desde arriba.
	//-------------------------------------------------------
	bmp_extract_sprite(  2,  5, TANK_WIDTH, TANK_HEIGHT, tank_up);
	bmp_extract_sprite(124,  8, TANK_WIDTH, TANK_HEIGHT, tank_right);
	bmp_extract_sprite(252, 14, TANK_BULLET_WIDTH, TANK_BULLET_HEIGHT, bullet);

	bmp_close_sprite_sheet();

	printf("Tres sprites recortados. Pulsa una tecla.\n");
	getch();

	set_video_mode(0x0013);
	bmp_write_pallete_data_into_dac(buffer_palleta_data);

	//-------------------------------------------------------
	// Los sprites quietos, para verlos.
	//-------------------------------------------------------
	bmp_draw_world_window(buffer_background_image_data);

	draw_sprite_to_buffer(tank_up,    TANK_WIDTH, TANK_HEIGHT,  60,  90, buffer_background_image_data);
	draw_sprite_to_buffer(tank_right, TANK_WIDTH, TANK_HEIGHT, 140,  90, buffer_background_image_data);
	draw_sprite_to_buffer(bullet, TANK_BULLET_WIDTH, TANK_BULLET_HEIGHT, 220, 98, buffer_background_image_data);

	wait_retrace();
	bmp_paint_image_data_to_vga(buffer_background_image_data);

	getch();

	//-------------------------------------------------------
	// Y ahora la demostracion del RECORTE.
	//
	// El tanque se pasea de un borde al otro, saliendose por los dos lados.
	// Fijate en que se le pasan coordenadas NEGATIVAS y mayores que 320, y
	// no pasa nada: draw_sprite_to_buffer() pinta solo la parte que cae
	// dentro.
	//
	// Si esos parametros fueran unsigned, un -9 valdria 65527 y la escritura
	// caeria muy lejos del buffer. En DOS eso no da error: corrompe memoria
	// y el juego se cuelga mas tarde, en otro sitio.
	//-------------------------------------------------------
	for (x = -TANK_WIDTH - 4; x < WIDTH + 4; x = x + 2){

		bmp_draw_world_window(buffer_background_image_data);
		draw_sprite_to_buffer(tank_right, TANK_WIDTH, TANK_HEIGHT, x, 90, buffer_background_image_data);

		wait_retrace();
		bmp_paint_image_data_to_vga(buffer_background_image_data);

	}

	getch();

	set_video_mode(0x0003);
	bmp_close_files();
	bmp_delete_buffers();
	free(tank_up);
	free(tank_right);
	free(bullet);

	printf("\n");
	printf("El tanque entro y salio por los bordes a medias, sin romper\n");
	printf("nada. Eso es el recorte. Y el fondo se ve entre las esquinas\n");
	printf("del tanque: eso es el color 0, que no se pinta.\n");
	printf("\n");

	return 0;

}
