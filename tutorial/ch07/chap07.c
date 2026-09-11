//===========================================================
// CAPITULO 7 - Colisiones contra el mapa
//
// Hasta ahora el tanque atravesaba las paredes. Se paraba en el borde de la
// pantalla y nada mas.
//
// Lo que se aprende aqui:
//
//   1. Por que hace falta un SEGUNDO mapa solo para las colisiones.
//   2. Por que NO se puede mirar el dibujo bonito para decidir.
//   3. La tecnica de "mirar antes de saltar": calcular donde ESTARIAS,
//      comprobar, y solo entonces moverte.
//   4. Por que se comprueban 3 puntos y no el rectangulo entero.
//
// Pulsa TAB durante el juego para ver el mapa de colisiones. Eso es lo que
// ve el juego; lo otro es lo que ves tu.
//
// Se usan bmp_is_wall() de src/bmp.c y
// player_update_future_collision_points() de src/players.c.
//===========================================================

#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include "header\bmp.h"
#include "header\players.h"
#include "..\tutlib.h"


struct player tank;

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


//===========================================================
// LA FUNCION DEL CAPITULO.
//
// Devuelve 1 si el tanque chocaria contra un muro al avanzar en esa
// direccion.
//
// player_update_future_collision_points() calcula donde estarian, un paso
// mas adelante, TRES puntos del tanque: la punta del canon y las dos
// orugas. Se guardan en future_cannon_tip_x/y, future_track1_x/y y
// future_track2_x/y.
//
// Y luego se le pregunta al mapa de colisiones por esos tres puntos.
//
// TRES puntos y no los 324 del rectangulo: comprobar el rectangulo entero
// costaria 324 lecturas por tanque y por frame, y no aportaria nada. Los
// tres puntos del frente son los unicos que pueden tocar algo primero.
//===========================================================
static int is_blocked_by_wall(int direction){

	player_update_future_collision_points(&tank, direction);

	if (bmp_is_wall((int)tank.future_cannon_tip_x, (int)tank.future_cannon_tip_y) == 1){
		return 1;
	}

	if (bmp_is_wall((int)tank.future_track1_x, (int)tank.future_track1_y) == 1){
		return 1;
	}

	if (bmp_is_wall((int)tank.future_track2_x, (int)tank.future_track2_y) == 1){
		return 1;
	}

	return 0;

}


//===========================================================
// Intentar moverse. "Mirar antes de saltar".
//
// Fijate en el orden, porque es lo importante:
//
//   1. Apuntar hacia alli (girar es gratis, nunca choca)
//   2. Preguntar si el paso esta libre
//   3. Solo si lo esta, dar el paso
//
// Nunca se mueve primero para deshacerlo despues. Si lo hicieras asi,
// tendrias un frame en el que el tanque esta dentro de la pared, y todo lo
// que mirase el estado en ese instante veria algo imposible.
//===========================================================
static int try_move(int direction){

	tank.current_direction = direction;

	if (is_blocked_by_wall(direction) == 1){
		return 0;
	}

	if (direction == MOVE_UP){
		tank.position_y = tank.position_y - PIXEL_TO_MOVE;
	}else if (direction == MOVE_DOWN){
		tank.position_y = tank.position_y + PIXEL_TO_MOVE;
	}else if (direction == MOVE_LEFT){
		tank.position_x = tank.position_x - PIXEL_TO_MOVE;
	}else if (direction == MOVE_RIGHT){
		tank.position_x = tank.position_x + PIXEL_TO_MOVE;
	}

	return 1;

}


int main(){

	unsigned char *sprite_now;
	int moved;
	int show_collision;
	int tab_was_down;

	printf("\n");
	printf("CAPITULO 7 - Colisiones\n");
	printf("\n");
	printf("Flechas para mover. ESC para salir.\n");
	printf("\n");
	printf("  TAB  cambia entre el mapa bonito y el mapa de COLISIONES.\n");
	printf("\n");
	printf("El de colisiones es lo que ve el juego: azul se pisa, amarillo\n");
	printf("no. Fijate en que NO coinciden exactamente con el dibujo.\n");
	printf("\n");
	getch();

	bmp_init_buffers(WIDTH, HEIGHT);

	//-------------------------------------------------------
	// DOS mapas, de dos ficheros distintos:
	//
	//   cutre.bmp     el dibujo bonito. Solo para mirar.
	//   cutrecol.bmp  los muros. Solo para decidir.
	//
	// El segundo tiene dos colores: 3 (suelo) y 252 (muro).
	//-------------------------------------------------------
	bmp_fill_background_in_main_buffer("..\\..\\res\\cutre.bmp");
	bmp_fill_background_collision_in_buffer("..\\..\\res\\cutrecol.bmp");
	bmp_extract_pallete_from_file("..\\..\\res\\cutre.bmp");

	player_init(&tank);
	load_sprites();
	player_reset(&tank, PLAYER1_START_X, PLAYER1_START_Y, PLAYER1_START_DIRECTION);

	// Los dos numeros de la animacion, igual que en el capitulo 6:
	// player_init() no los pone y sin ellos las orugas se quedan congeladas.
	tank.total_frames = 2;
	tank.speed_total  = 2;

	show_collision = 0;
	tab_was_down = 0;

	install_kbd();
	set_video_mode(0x0013);
	bmp_write_pallete_data_into_dac(buffer_palleta_data);

	do {

		// TAB: solo el frame en que se PULSA, no mientras se mantiene.
		// Sin esto cambiaria 70 veces por segundo.
		if (keys[KEY_TAB]){
			if (tab_was_down == 0){
				show_collision = !show_collision;
			}
			tab_was_down = 1;
		}else{
			tab_was_down = 0;
		}

		moved = 0;

		if (keys[KEY_UP]){
			moved = try_move(MOVE_UP);
		}else if (keys[KEY_DOWN]){
			moved = try_move(MOVE_DOWN);
		}else if (keys[KEY_LEFT]){
			moved = try_move(MOVE_LEFT);
		}else if (keys[KEY_RIGHT]){
			moved = try_move(MOVE_RIGHT);
		}

		// Las orugas solo giran si el tanque AVANZO de verdad. Empujando
		// contra una pared, se quedan quietas. Ese detalle se nota mucho.
		if (moved == 1){
			update_animation();
		}

		sprite_now = pick_sprite();

		//-------------------------------------------------------
		// Dibujar el fondo: el bonito, o el de colisiones.
		//
		// Cuando se ensena el de colisiones se pinta a partir de la
		// mascara, preguntandole a bmp_is_wall() pixel a pixel. Es lento y
		// da igual: es una ayuda para aprender, no parte del juego.
		//-------------------------------------------------------
		if (show_collision == 0){

			bmp_draw_world_window(buffer_background_image_data);

		}else{

			int px;
			int py;
			unsigned int offset;

			for (py = 0; py < HEIGHT; py++){
				for (px = 0; px < WIDTH; px++){
					offset = ((unsigned int)py * WIDTH) + (unsigned int)px;
					if (bmp_is_wall(px, py) == 1){
						buffer_background_image_data[offset] = 252;
					}else{
						buffer_background_image_data[offset] = 3;
					}
				}
			}

		}

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
	printf("El juego nunca mira el dibujo para decidir. Mira un segundo\n");
	printf("mapa que solo dice muro o no muro.\n");
	printf("\n");

	return 0;

}
