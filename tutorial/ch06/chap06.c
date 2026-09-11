//===========================================================
// CAPITULO 6 - Animacion: que las orugas se muevan
//
// El tanque del capitulo 5 se desplazaba, pero parecia una pegatina
// arrastrandose. Le falta que las orugas giren.
//
// Lo que se aprende aqui:
//
//   1. Que una animacion son dos dibujos y un contador, nada mas.
//   2. Por que NO se puede cambiar de dibujo en cada frame.
//   3. Que la animacion solo avanza si el tanque se ha movido de verdad.
//
// Aqui aparece por primera vez "struct player", la estructura real del
// juego, que vive en header/players.h. Y se usan player_init() y
// player_reset() de src/players.c.
//===========================================================

#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include "header\bmp.h"
#include "header\players.h"
#include "..\tutlib.h"



// El tanque, con la estructura real del juego
struct player tank;


//===========================================================
// Cargar los OCHO sprites del tanque: dos por cada direccion.
//
// Las coordenadas son las del juego, copiadas de src/main.c. Cada pareja
// son las mismas orugas en dos posiciones distintas.
//===========================================================
static void load_sprites(){

	bmp_open_sprite_sheet("..\\..\\res\\sprites.bmp");

	bmp_extract_sprite(  2,  5, TANK_WIDTH, TANK_HEIGHT, tank.sprite_tank_up);
	bmp_extract_sprite( 23,  5, TANK_WIDTH, TANK_HEIGHT, tank.sprite_tank_up_2);

	bmp_extract_sprite( 43, 10, TANK_WIDTH, TANK_HEIGHT, tank.sprite_tank_down);
	bmp_extract_sprite( 63, 10, TANK_WIDTH, TANK_HEIGHT, tank.sprite_tank_down_2);

	bmp_extract_sprite( 83,  8, TANK_WIDTH, TANK_HEIGHT, tank.sprite_tank_left);
	bmp_extract_sprite(102,  8, TANK_WIDTH, TANK_HEIGHT, tank.sprite_tank_left_2);

	bmp_extract_sprite(124,  8, TANK_WIDTH, TANK_HEIGHT, tank.sprite_tank_right);
	bmp_extract_sprite(145,  8, TANK_WIDTH, TANK_HEIGHT, tank.sprite_tank_right_2);

	bmp_close_sprite_sheet();

}


//===========================================================
// Elegir que dibujo toca AHORA.
//
// Dos preguntas: hacia donde mira, y cual de los dos fotogramas va.
//===========================================================
static unsigned char *pick_sprite(){

	if (tank.current_direction == MOVE_UP){
		if (tank.current_frame == 0){ return tank.sprite_tank_up; }
		return tank.sprite_tank_up_2;
	}

	if (tank.current_direction == MOVE_DOWN){
		if (tank.current_frame == 0){ return tank.sprite_tank_down; }
		return tank.sprite_tank_down_2;
	}

	if (tank.current_direction == MOVE_LEFT){
		if (tank.current_frame == 0){ return tank.sprite_tank_left; }
		return tank.sprite_tank_left_2;
	}

	if (tank.current_frame == 0){ return tank.sprite_tank_right; }
	return tank.sprite_tank_right_2;

}


//===========================================================
// EL CONTADOR. Esta es la leccion del capitulo.
//
// Si cambiaras de fotograma en cada vuelta del bucle, las orugas girarian
// a 70 cambios por segundo: no verias una animacion, verias un borron.
//
// Asi que se cuenta. speed_counter sube en cada frame, y solo cuando llega
// a speed_total se cambia el dibujo y se vuelve a cero. Con speed_total = 5
// el dibujo cambia 14 veces por segundo, que es lo que el ojo lee como
// movimiento.
//
// Y lo segundo, igual de importante: esto SOLO se llama si el tanque se ha
// movido. Un tanque parado tiene las orugas quietas.
//===========================================================
static void update_animation(){

	tank.speed_counter = tank.speed_counter + 1;

	if (tank.speed_counter >= tank.speed_total){

		tank.speed_counter = 0;

		tank.current_frame = tank.current_frame + 1;

		if (tank.current_frame >= tank.total_frames){
			tank.current_frame = 0;
		}

	}

}


int main(){

	unsigned char *sprite_now;
	int is_moving;

	printf("\n");
	printf("CAPITULO 6 - Animacion\n");
	printf("\n");
	printf("Flechas para mover, ESC para salir.\n");
	printf("\n");
	printf("Fijate en las orugas: solo giran mientras el tanque avanza.\n");
	printf("Sueltalo y se paran en seco.\n");
	printf("\n");
	getch();

	bmp_init_buffers(WIDTH, HEIGHT);
	bmp_fill_background_in_main_buffer("..\\..\\res\\cutre.bmp");
	bmp_extract_pallete_from_file("..\\..\\res\\cutre.bmp");

	//-------------------------------------------------------
	// player_init() reserva los buffers de todos los sprites del tanque y
	// pone los valores de animacion por defecto. Es de src/players.c, el
	// fichero real.
	//-------------------------------------------------------
	player_init(&tank);
	load_sprites();

	player_reset(&tank, 150, 90, MOVE_UP);

	install_kbd();
	set_video_mode(0x0013);
	bmp_write_pallete_data_into_dac(buffer_palleta_data);

	do {

		is_moving = 0;

		if (keys[KEY_UP]){
			tank.position_y = tank.position_y - PIXEL_TO_MOVE;
			tank.current_direction = MOVE_UP;
			is_moving = 1;
		}else if (keys[KEY_DOWN]){
			tank.position_y = tank.position_y + PIXEL_TO_MOVE;
			tank.current_direction = MOVE_DOWN;
			is_moving = 1;
		}else if (keys[KEY_LEFT]){
			tank.position_x = tank.position_x - PIXEL_TO_MOVE;
			tank.current_direction = MOVE_LEFT;
			is_moving = 1;
		}else if (keys[KEY_RIGHT]){
			tank.position_x = tank.position_x + PIXEL_TO_MOVE;
			tank.current_direction = MOVE_RIGHT;
			is_moving = 1;
		}

		if (tank.position_x > WIDTH  - TANK_WIDTH ){ tank.position_x = WIDTH  - TANK_WIDTH;  }
		if (tank.position_y > HEIGHT - TANK_HEIGHT){ tank.position_y = HEIGHT - TANK_HEIGHT; }

		// AQUI esta la clave: la animacion solo avanza si se ha movido
		if (is_moving == 1){
			update_animation();
		}

		sprite_now = pick_sprite();

		bmp_draw_world_window(buffer_background_image_data);
		draw_sprite_to_buffer(sprite_now, TANK_WIDTH, TANK_HEIGHT,
		                      (int)tank.position_x, (int)tank.position_y,
		                      buffer_background_image_data);

		wait_retrace();
		bmp_paint_image_data_to_vga(buffer_background_image_data);

	} while (!keys[KEY_ESC]);

	uninstall_kbd();
	set_video_mode(0x0003);

	player_free(&tank);
	bmp_close_files();
	bmp_delete_buffers();

	printf("\n");
	printf("Dos dibujos y un contador. Eso es toda la animacion.\n");
	printf("\n");

	return 0;

}
