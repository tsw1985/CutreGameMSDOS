//===========================================================
// CAPITULO 20 - El mundo deja de ser la pantalla
//
// Este capitulo esta hecho para que ALGO SALGA MAL, a proposito.
//
// El mapa pasa a ser de 640x400: cuatro pantallas. El tanque se mueve por el
// mundo entero... y la pantalla se queda quieta en la esquina de arriba a la
// izquierda.
//
// Asi que en cuanto te alejas, el tanque desaparece y conduces a ciegas.
//
// Eso es incomodo, y es justo el objetivo: hasta que no sufres el problema,
// una camara parece un adorno.
//
// Lo que se aprende aqui:
//
//   1. LOS DOS SISTEMAS DE COORDENADAS. La idea central del bloque.
//   2. Que el juego funciona perfectamente sin camara: lo que no funciona
//      es MIRARLO.
//   3. Que el limite del movimiento deja de ser la pantalla.
//===========================================================

#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include "header\bmp.h"
#include "header\players.h"
#include "..\tutlib.h"


struct player tank;


int main(){

	int moved;

	printf("\n");
	printf("CAPITULO 20 - El mundo deja de ser la pantalla\n");
	printf("\n");
	printf("  El mapa mide 640x400. La pantalla, 320x200.\n");
	printf("\n");
	printf("  La pantalla NO se va a mover. En cuanto te alejes, el tanque\n");
	printf("  se sale por el borde y lo pierdes de vista.\n");
	printf("\n");
	printf("  Es a proposito: eso es la vida sin camara.\n");
	printf("\n");
	printf("  Flechas para mover, ESC para salir.\n");
	printf("\n");
	getch();

	//-------------------------------------------------------
	// LO UNICO QUE CAMBIA RESPECTO AL CAPITULO 7.
	//
	// bmp_init_buffers(640, 400) en vez de (WIDTH, HEIGHT). A partir de
	// aqui, map_width vale 640 y map_height 400, mientras que WIDTH y
	// HEIGHT siguen valiendo 320 y 200 porque esa es LA PANTALLA.
	//
	// Y ahi esta la separacion que hay que entender:
	//
	//   WIDTH / HEIGHT          la pantalla. Nunca cambian. 320x200.
	//   map_width / map_height  el mundo. Aqui, 640x400.
	//
	// Confundirlos es EL error de este bloque.
	//-------------------------------------------------------
	bmp_init_buffers(640, 400);
	bmp_fill_background_in_main_buffer("..\\..\\res\\big.bmp");
	bmp_fill_background_collision_in_buffer("..\\..\\res\\bigcol.bmp");
	bmp_extract_pallete_from_file("..\\..\\res\\big.bmp");

	player_init(&tank);

	bmp_open_sprite_sheet("..\\..\\res\\sprites.bmp");
	tut_load_tank_sprites(&tank, 0);
	bmp_close_sprite_sheet();

	//-------------------------------------------------------
	// Ese spawn es de header/players.h y es para el mapa GRANDE.
	//
	// El de siempre, PLAYER2_START_Y = 16, caeria dentro del muro del borde
	// del mapa grande, que tiene 17 pixeles de grosor. El tanque naceria
	// atrapado. Al cambiar de mundo hay que revisar los puntos de partida.
	//-------------------------------------------------------
	player_reset(&tank, BIG_PLAYER1_START_X, BIG_PLAYER1_START_Y, BIG_PLAYER1_START_DIRECTION);

	install_kbd();
	set_video_mode(0x0013);
	bmp_write_pallete_data_into_dac(buffer_palleta_data);

	do {

		moved = 0;

		//-----------------------------------------------
		// El movimiento y las colisiones NO CAMBIAN NADA.
		//
		// tut_try_move() pregunta a bmp_is_wall(), que ahora conoce un
		// mundo de 640x400 porque la mascara se cargo de bigcol.bmp.
		//
		// El juego funciona perfectamente en el mundo grande. Lo que no
		// funciona es verlo.
		//
		// Y fijate en que ya no hay ningun recorte contra WIDTH ni HEIGHT:
		// lo que para al tanque es el MURO pintado en el mapa. El limite
		// dejo de ser codigo y paso a ser dato.
		//-----------------------------------------------
		if (keys[KEY_UP]){
			moved = tut_try_move(&tank, MOVE_UP);
		}else if (keys[KEY_DOWN]){
			moved = tut_try_move(&tank, MOVE_DOWN);
		}else if (keys[KEY_LEFT]){
			moved = tut_try_move(&tank, MOVE_LEFT);
		}else if (keys[KEY_RIGHT]){
			moved = tut_try_move(&tank, MOVE_RIGHT);
		}

		if (moved == 1){
			tut_update_animation(&tank);
		}

		//-----------------------------------------------
		// EL DIBUJADO, y aqui esta el problema del capitulo.
		//
		// bmp_draw_world_window() copia el trozo visible del mundo. Como
		// camera_x y camera_y valen 0 y nadie los toca, ese trozo es
		// siempre la esquina de arriba a la izquierda.
		//
		// Y el tanque se pinta en su coordenada de MUNDO tal cual, sin
		// restar nada. Mientras esta en la primera pantalla, coincide. En
		// cuanto pasa de x=320, se pinta fuera y no lo ves.
		//
		// (No revienta nada porque draw_sprite_to_buffer() recorta. Sin ese
		// recorte del capitulo 4, esto estaria machacando memoria ajena
		// ahora mismo.)
		//-----------------------------------------------
		bmp_draw_world_window(buffer_background_image_data);

		draw_sprite_to_buffer(tut_pick_sprite(&tank), TANK_WIDTH, TANK_HEIGHT,
		                      (int)tank.position_x, (int)tank.position_y,
		                      buffer_background_image_data);

		wait_retrace();
		bmp_paint_image_data_to_vga(buffer_background_image_data);

	} while (!keys[KEY_ESC]);

	uninstall_kbd();
	set_video_mode(0x0003);

	printf("\n");
	printf("  El tanque acabo en la posicion de mundo (%u, %u).\n",
	       tank.position_x, tank.position_y);
	printf("  El mundo mide %dx%d y la pantalla %dx%d.\n",
	       map_width, map_height, WIDTH, HEIGHT);
	printf("\n");
	printf("  Si esas coordenadas pasan de 320 o de 200, el tanque estaba\n");
	printf("  vivo y moviendose en un sitio que la pantalla no ensenaba.\n");
	printf("\n");
	printf("  La partida iba bien. Lo que faltaba era decidir QUE TROZO\n");
	printf("  mirar. Eso es una camara, y es el capitulo 21.\n");
	printf("\n");

	player_free(&tank);
	bmp_close_files();
	bmp_delete_buffers();

	return 0;

}
