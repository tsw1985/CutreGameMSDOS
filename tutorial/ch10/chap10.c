//===========================================================
// CAPITULO 10 - Impacto, explosion y ronda
//
// Con esto el juego local esta terminado.
//
// Lo que se aprende aqui:
//
//   1. Detectar que una bala ha dado en un tanque.
//   2. Que es una MAQUINA DE ESTADOS, aunque sea de dos estados.
//   3. Por que la ronda se congela unos frames en vez de reiniciarse de
//      golpe.
//   4. Por que los DOS impactos del mismo frame tienen que contar.
//===========================================================

#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include "header\bmp.h"
#include "header\players.h"
#include "..\tutlib.h"

// Cuantos frames dura la explosion antes de reiniciar la ronda
#define EXPLOSION_TOTAL_FRAMES 40

struct player player1;
struct player player2;

// EL ESTADO DEL JUEGO, en una sola variable:
//
//   0  = la ronda corre normal
//   >0 = alguien ha explotado; quedan estos frames de pausa
//
// Dos estados y un contador. Eso es una maquina de estados, y no hace falta
// nada mas elaborado.
unsigned int explosion_pause_counter;


static unsigned char read_input_from_keys(unsigned char key_up, unsigned char key_down,
                                          unsigned char key_left, unsigned char key_right,
                                          unsigned char key_fire){

	unsigned char bits;

	bits = 0;

	if (keys[key_up]){    bits = bits | 0x01; }
	if (keys[key_down]){  bits = bits | 0x02; }
	if (keys[key_left]){  bits = bits | 0x04; }
	if (keys[key_right]){ bits = bits | 0x08; }
	if (keys[key_fire]){  bits = bits | 0x10; }

	return bits;

}


//===========================================================
// Ha dado la bala de _player en el tanque _other?
//
// Cuatro comparaciones: un punto contra una caja. Si el centro de la bala
// esta dentro del rectangulo del otro tanque, ha dado.
//
// Esta es la prueba de colision mas barata que existe y para este juego
// sobra. No hace falta geometria de verdad.
//===========================================================
static int bullet_has_hit_tank(struct player *p, struct player *other){

	unsigned int bullet_x;
	unsigned int bullet_y;

	bullet_x = p->bullet_position_x + BULLET_CENTER_X;
	bullet_y = p->bullet_position_y + BULLET_CENTER_Y;

	if (bullet_x < other->position_x){ return 0; }
	if (bullet_x > other->position_x + TANK_WIDTH - 1){ return 0; }
	if (bullet_y < other->position_y){ return 0; }
	if (bullet_y > other->position_y + TANK_HEIGHT - 1){ return 0; }

	return 1;

}


// Devuelve 1 si esta bala ha matado al otro tanque
static int update_bullet(struct player *p, struct player *other){

	if (p->bullet_is_flying == 0){
		player_update_bullet_position(p);
		return 0;
	}

	player_move_bullet(p);

	if (p->bullet_is_flying == 0){
		return 0;
	}

	// El tanque se comprueba ANTES que el muro. En la practica da igual
	// (un tanque nunca esta encima de un muro) pero acertar es de lo que va
	// el juego.
	if (bullet_has_hit_tank(p, other) == 1){

		p->bullet_is_flying = 0;
		p->wins = p->wins + 1;

		return 1;

	}

	if (bmp_is_wall((int)(p->bullet_position_x + BULLET_CENTER_X),
	                (int)(p->bullet_position_y + BULLET_CENTER_Y)) == 1){
		p->bullet_is_flying = 0;
	}

	return 0;

}


static void process_player_input(struct player *p, unsigned char input_bits){

	int moved;

	moved = 0;

	if (input_bits & 0x01){
		moved = tut_try_move(p, MOVE_UP);
	}else if (input_bits & 0x02){
		moved = tut_try_move(p, MOVE_DOWN);
	}else if (input_bits & 0x04){
		moved = tut_try_move(p, MOVE_LEFT);
	}else if (input_bits & 0x08){
		moved = tut_try_move(p, MOVE_RIGHT);
	}

	if (moved == 1){
		tut_update_animation(p);
	}

	if (input_bits & 0x10){
		if (p->fire_was_pressed == 0){
			player_fire_bullet(p);
		}
		p->fire_was_pressed = 1;
	}else{
		p->fire_was_pressed = 0;
	}

}


static void restart_round(){

	player_reset(&player1, PLAYER1_START_X, PLAYER1_START_Y, PLAYER1_START_DIRECTION);
	player_reset(&player2, PLAYER2_START_X, PLAYER2_START_Y, PLAYER2_START_DIRECTION);

	// player_reset() NO toca wins: el marcador sobrevive a la ronda.

}


static void draw_tank_or_explosion(struct player *p){

	if (p->is_exploding == 1){

		unsigned char *boom;

		if (p->explosion_current_frame == 0){
			boom = p->sprite_tank_explosion;
		}else{
			boom = p->sprite_tank_explosion2;
		}

		// La explosion mide 13x13 y el tanque 18x18, asi que se mete 2
		// pixeles por cada lado para quedar centrada en el hueco que
		// ocupaba el tanque. De ahi los EXPLOSION_OFFSET_*.
		draw_sprite_to_buffer(boom, EXPLOSION_WIDTH, EXPLOSION_HEIGHT,
		                      (int)(p->position_x + EXPLOSION_OFFSET_X),
		                      (int)(p->position_y + EXPLOSION_OFFSET_Y),
		                      buffer_background_image_data);

	}else{

		draw_sprite_to_buffer(tut_pick_sprite(p), TANK_WIDTH, TANK_HEIGHT,
		                      (int)p->position_x, (int)p->position_y,
		                      buffer_background_image_data);

	}

}


int main(){

	unsigned char input1;
	unsigned char input2;
	int tank_was_hit;

	printf("\n");
	printf("CAPITULO 10 - Impacto y ronda\n");
	printf("\n");
	printf("  Jugador 1:  flechas + 5 del numerico\n");
	printf("  Jugador 2:  W A S D + G\n");
	printf("  ESC para salir (el marcador se imprime al salir)\n");
	printf("\n");
	getch();

	bmp_init_buffers(WIDTH, HEIGHT);
	bmp_fill_background_in_main_buffer("..\\..\\res\\cutre.bmp");
	bmp_fill_background_collision_in_buffer("..\\..\\res\\cutrecol.bmp");
	bmp_extract_pallete_from_file("..\\..\\res\\cutre.bmp");

	player_init(&player1);
	player_init(&player2);

	bmp_open_sprite_sheet("..\\..\\res\\sprites.bmp");
	tut_load_tank_sprites(&player1, 0);
	tut_load_tank_sprites(&player2, 21);
	bmp_close_sprite_sheet();

	restart_round();
	explosion_pause_counter = 0;

	install_kbd();
	set_video_mode(0x0013);
	bmp_write_pallete_data_into_dac(buffer_palleta_data);

	do {

		//===============================================
		// LA MAQUINA DE ESTADOS. Dos ramas, y en la de abajo el juego
		// esta CONGELADO: no se lee el teclado, no se mueven las balas,
		// solo avanza la animacion de la explosion.
		//===============================================
		if (explosion_pause_counter == 0){

			// ---- ESTADO NORMAL ----

			input1 = read_input_from_keys(KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_NUMPAD_5);
			input2 = read_input_from_keys(KEY_W,  KEY_S,    KEY_A,    KEY_D,     KEY_G);

			process_player_input(&player1, input1);
			process_player_input(&player2, input2);

			tank_was_hit = 0;

			//-------------------------------------------
			// LAS DOS balas se comprueban, y DESPUES se mira si hubo
			// impacto.
			//
			// Si saliera del bucle en cuanto una acierta, un doble
			// impacto en el mismo frame contaria solo uno. Asi los dos
			// se matan a la vez y los dos suman punto, que es lo justo.
			//-------------------------------------------
			if (update_bullet(&player1, &player2) == 1){
				player_start_explosion(&player2);
				tank_was_hit = 1;
			}

			if (update_bullet(&player2, &player1) == 1){
				player_start_explosion(&player1);
				tank_was_hit = 1;
			}

			if (tank_was_hit == 1){

				// Apagar las balas que siguieran en el aire, o se
				// quedarian congeladas en mitad de la pantalla durante
				// toda la pausa.
				player1.bullet_is_flying = 0;
				player2.bullet_is_flying = 0;

				// Empieza la pausa. Los tanques NO se reinician aqui:
				// se quedan donde les dieron, para poder dibujar la
				// explosion encima.
				explosion_pause_counter = EXPLOSION_TOTAL_FRAMES;

			}

		}else{

			// ---- ESTADO ARDIENDO ----

			player_update_explosion(&player1);
			player_update_explosion(&player2);

			explosion_pause_counter = explosion_pause_counter - 1;

			if (explosion_pause_counter == 0){
				restart_round();
			}

		}

		bmp_draw_world_window(buffer_background_image_data);

		draw_tank_or_explosion(&player1);
		draw_tank_or_explosion(&player2);

		if (player1.bullet_is_flying == 1){
			draw_sprite_to_buffer(player1.sprite_tank_bullet, TANK_BULLET_WIDTH, TANK_BULLET_HEIGHT,
			                      (int)player1.bullet_position_x, (int)player1.bullet_position_y,
			                      buffer_background_image_data);
		}

		if (player2.bullet_is_flying == 1){
			draw_sprite_to_buffer(player2.sprite_tank_bullet, TANK_BULLET_WIDTH, TANK_BULLET_HEIGHT,
			                      (int)player2.bullet_position_x, (int)player2.bullet_position_y,
			                      buffer_background_image_data);
		}

		wait_retrace();
		bmp_paint_image_data_to_vga(buffer_background_image_data);

	} while (!keys[KEY_ESC]);

	uninstall_kbd();
	set_video_mode(0x0003);

	printf("\n");
	printf("  Jugador 1: %u    Jugador 2: %u\n", player1.wins, player2.wins);
	printf("\n");
	printf("El juego local esta terminado. A partir del capitulo 11, sonido.\n");
	printf("\n");

	player_free(&player1);
	player_free(&player2);
	bmp_close_files();
	bmp_delete_buffers();

	return 0;

}
