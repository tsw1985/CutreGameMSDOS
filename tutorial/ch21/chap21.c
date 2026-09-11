//===========================================================
// CAPITULO 21 - La camara
//
// El capitulo 20 te dejo conduciendo a ciegas. Esto lo arregla, y son SEIS
// lineas de cambio.
//
// Lo que se aprende aqui:
//
//   1. Que una camara son DOS NUMEROS y una resta.
//   2. Que es la ZONA MUERTA y por que no se sigue al tanque siempre.
//   3. Por que la correccion es "exactamente lo que se ha salido".
//   4. Que es el clamp.
//   5. Pulsa C y compara los tres modelos de camara.
//
// Todo con bmp_camera_follow() y bmp_camera_snap() de src/bmp.c, las
// funciones reales del juego.
//===========================================================

#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include "header\bmp.h"
#include "header\players.h"
#include "..\tutlib.h"

#define KEY_C 0x2E

struct player tank;


int main(){

	int moved;
	int mode;
	int c_was_down;
	int screen_x;
	int screen_y;

	printf("\n");
	printf("CAPITULO 21 - La camara\n");
	printf("\n");
	printf("  Flechas para moverte por el mundo de 640x400.\n");
	printf("\n");
	printf("  C  cambia entre los tres modelos de camara:\n");
	printf("       0 = SIN camara      (el capitulo 20, para comparar)\n");
	printf("       1 = ZONA MUERTA     (la del juego)\n");
	printf("       2 = SIEMPRE CENTRADA (para que veas por que no se usa)\n");
	printf("\n");
	printf("  ESC para salir.\n");
	printf("\n");
	getch();

	bmp_init_buffers(640, 400);
	bmp_fill_background_in_main_buffer("..\\..\\res\\big.bmp");
	bmp_fill_background_collision_in_buffer("..\\..\\res\\bigcol.bmp");
	bmp_extract_pallete_from_file("..\\..\\res\\big.bmp");

	player_init(&tank);

	bmp_open_sprite_sheet("..\\..\\res\\sprites.bmp");
	tut_load_tank_sprites(&tank, 0);
	bmp_close_sprite_sheet();

	player_reset(&tank, BIG_PLAYER1_START_X, BIG_PLAYER1_START_Y, BIG_PLAYER1_START_DIRECTION);

	//-------------------------------------------------------
	// Apuntar la camara al empezar.
	//
	// bmp_camera_snap() la centra de golpe, sin suavizado. Al empezar una
	// ronda no hay nada desde lo que seguir suavemente: el tanque acaba de
	// aparecer.
	//-------------------------------------------------------
	bmp_camera_snap((int)tank.position_x, (int)tank.position_y, TANK_WIDTH, TANK_HEIGHT);

	mode = 1;
	c_was_down = 0;

	install_kbd();
	set_video_mode(0x0013);
	bmp_write_pallete_data_into_dac(buffer_palleta_data);

	do {

		if (keys[KEY_C]){
			if (c_was_down == 0){
				mode = mode + 1;
				if (mode > 2){ mode = 0; }
				if (mode == 0){
					camera_x = 0;
					camera_y = 0;
				}
			}
			c_was_down = 1;
		}else{
			c_was_down = 0;
		}

		moved = 0;

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

		//===============================================
		// DECIDIR DONDE ESTA LA VENTANA.
		//
		// ANTES de pintar nada, para que el fondo y lo que va encima esten
		// de acuerdo sobre el mismo frame.
		//===============================================
		if (mode == 1){

			//-------------------------------------------
			// ZONA MUERTA. La del juego.
			//
			// La camara NO SE MUEVE mientras el tanque este dentro de un
			// rectangulo en el centro de la pantalla:
			//
			//   en X, de 100 a 320-100-18 = 202
			//   en Y, de  70 a 200- 70-18 = 112
			//
			// Cuando se sale, empuja EXACTAMENTE lo que se ha salido. El
			// tanque anda 2 pixeles, la camara anda 2 pixeles. Ni salto ni
			// retraso, y cuando paras, para con el.
			//
			// Midiendo un paseo de 200 frames, la camara esta QUIETA el 88%
			// del tiempo. Eso es lo que se busca: que el mundo solo se
			// mueva cuando de verdad vas a algun sitio.
			//-------------------------------------------
			bmp_camera_follow((int)tank.position_x, (int)tank.position_y, TANK_WIDTH, TANK_HEIGHT);

		}else if (mode == 2){

			//-------------------------------------------
			// SIEMPRE CENTRADA, para comparar.
			//
			// Es lo primero que se le ocurre a uno y es peor: el tanque se
			// queda clavado en el centro y lo que se mueve es TODO LO
			// DEMAS, en todos y cada uno de los frames.
			//
			// Pruebalo un rato. Marea, y te quita la sensacion de estar
			// moviendo tu tanque.
			//-------------------------------------------
			bmp_camera_snap((int)tank.position_x, (int)tank.position_y, TANK_WIDTH, TANK_HEIGHT);

		}

		// El modo 0 no toca la camara: se queda en 0,0 como el capitulo 20.

		bmp_draw_world_window(buffer_background_image_data);

		//-----------------------------------------------
		// LA RESTA. Esto es la camara entera.
		//
		//     pantalla = mundo - camara
		//
		// El tanque sigue estando donde esta en el mundo. Lo unico que
		// cambia es donde se PINTA.
		//
		// Y aqui es donde se cobra el recorte del capitulo 4: esta resta
		// puede dar NEGATIVA, y draw_sprite_to_buffer() tiene que
		// aguantarlo.
		//-----------------------------------------------
		screen_x = (int)tank.position_x - camera_x;
		screen_y = (int)tank.position_y - camera_y;

		draw_sprite_to_buffer(tut_pick_sprite(&tank), TANK_WIDTH, TANK_HEIGHT,
		                      screen_x, screen_y,
		                      buffer_background_image_data);

		wait_retrace();
		bmp_paint_image_data_to_vga(buffer_background_image_data);

	} while (!keys[KEY_ESC]);

	uninstall_kbd();
	set_video_mode(0x0003);

	printf("\n");
	printf("  Tanque en el MUNDO   : (%u, %u)\n", tank.position_x, tank.position_y);
	printf("  Camara               : (%d, %d)\n", camera_x, camera_y);
	printf("  Tanque en la PANTALLA: (%d, %d)\n",
	       (int)tank.position_x - camera_x, (int)tank.position_y - camera_y);
	printf("\n");
	printf("  Tres numeros y una resta. Eso es todo.\n");
	printf("\n");
	printf("  Y fijate en la camara: no puede pasar de (%d, %d), que es\n",
	       map_width - WIDTH, map_height - HEIGHT);
	printf("  map_width-320 y map_height-200. Eso es el CLAMP, y es lo que\n");
	printf("  impide que la ventana lea fuera del mapa.\n");
	printf("\n");
	printf("  Nota: con un mapa de 320x200 esos limites salen 0 y 0, asi que\n");
	printf("  la camara queda clavada en el origen y TODO ESTE CODIGO se\n");
	printf("  comporta como el juego de siempre. El modo normal no es un\n");
	printf("  caso especial: es el general con la camara topada.\n");
	printf("\n");

	player_free(&tank);
	bmp_close_files();
	bmp_delete_buffers();

	return 0;

}
