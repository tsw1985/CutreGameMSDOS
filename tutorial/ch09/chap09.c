//===========================================================
// CAPITULO 9 - Balas
//
// Lo que se aprende aqui:
//
//   1. Que una bala es un estado mas dentro del tanque, no un objeto nuevo.
//   2. Por que la direccion de la bala se CONGELA al disparar.
//   3. El detector de flanco: por que sin el tienes una ametralladora.
//   4. Colision de una bala: un punto, no tres.
//
// Lo que ya sabes esta en ..\tutlib.h.
//===========================================================

#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include "header\bmp.h"
#include "header\players.h"
#include "..\tutlib.h"


struct player player1;
struct player player2;


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
// Una bala en vuelo, un paso.
//
// player_move_bullet() la avanza y la mata si se sale del mapa.
// Despues se mira si ha dado en una pared.
//
// Fijate en que se comprueba UN SOLO punto, el centro de la bala:
//
//     bullet_position_x + BULLET_CENTER_X
//
// El tanque necesitaba tres puntos porque es grande y tiene un frente ancho.
// La bala mide 4x3: su centro basta.
//
// OJO con una consecuencia: la bala avanza BULLET_PIXEL_TO_MOVE = 3 pixeles
// por frame y se comprueba en un punto. Un muro de menos de 3 pixeles de
// grosor se lo salta. Por eso los mapas tienen muros gruesos.
//===========================================================
static void update_bullet(struct player *p){

	if (p->bullet_is_flying == 0){

		// Sin disparar: la bala se queda pegada a la punta del canon,
		// siguiendo al tanque. No se dibuja, pero esta lista.
		player_update_bullet_position(p);
		return;

	}

	player_move_bullet(p);

	// player_move_bullet() puede haberla matado por salirse del mapa. Solo
	// se lee el mapa mientras esta viva, para que la coordenada sea siempre
	// un punto real de dentro.
	if (p->bullet_is_flying == 0){
		return;
	}

	if (bmp_is_wall((int)(p->bullet_position_x + BULLET_CENTER_X),
	                (int)(p->bullet_position_y + BULLET_CENTER_Y)) == 1){
		p->bullet_is_flying = 0;
	}

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

	//-------------------------------------------------------
	// EL DETECTOR DE FLANCO. La leccion del capitulo.
	//
	// El bit de disparo sigue puesto TODO el rato que mantienes la tecla.
	// Si dispararas con el valor del bit a secas, en cuanto la bala muriera
	// saldria otra sola, y otra, y otra: una ametralladora.
	//
	// Asi que hay que detectar el FLANCO: el frame exacto en que el bit
	// pasa de 0 a 1. Eso necesita recordar como estaba el frame anterior, y
	// para eso esta fire_was_pressed dentro de la struct.
	//
	// El mismo patron vale para cualquier accion de "una vez por
	// pulsacion": abrir una puerta, cambiar de arma, pausar.
	//
	// Va FUERA de la cadena if / else if de arriba, a proposito: disparar
	// no es una direccion, y el tanque tiene que poder moverse y disparar
	// en el mismo frame.
	//-------------------------------------------------------
	if (input_bits & 0x10){

		if (p->fire_was_pressed == 0){
			player_fire_bullet(p);
		}

		p->fire_was_pressed = 1;

	}else{

		p->fire_was_pressed = 0;

	}

}


int main(){

	unsigned char input1;
	unsigned char input2;

	printf("\n");
	printf("CAPITULO 9 - Balas\n");
	printf("\n");
	printf("  Jugador 1:  flechas       + 5 del teclado numerico\n");
	printf("  Jugador 2:  W A S D       + G\n");
	printf("  ESC para salir\n");
	printf("\n");
	printf("Una bala en el aire por tanque. Prueba a mantener el disparo:\n");
	printf("solo sale una. Y prueba a disparar y girar: la bala sigue recta.\n");
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

	player_reset(&player1, PLAYER1_START_X, PLAYER1_START_Y, PLAYER1_START_DIRECTION);
	player_reset(&player2, PLAYER2_START_X, PLAYER2_START_Y, PLAYER2_START_DIRECTION);

	install_kbd();
	set_video_mode(0x0013);
	bmp_write_pallete_data_into_dac(buffer_palleta_data);

	do {

		input1 = read_input_from_keys(KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_NUMPAD_5);
		input2 = read_input_from_keys(KEY_W,  KEY_S,    KEY_A,    KEY_D,     KEY_G);

		process_player_input(&player1, input1);
		process_player_input(&player2, input2);

		update_bullet(&player1);
		update_bullet(&player2);

		bmp_draw_world_window(buffer_background_image_data);

		draw_sprite_to_buffer(tut_pick_sprite(&player1), TANK_WIDTH, TANK_HEIGHT,
		                      (int)player1.position_x, (int)player1.position_y,
		                      buffer_background_image_data);

		draw_sprite_to_buffer(tut_pick_sprite(&player2), TANK_WIDTH, TANK_HEIGHT,
		                      (int)player2.position_x, (int)player2.position_y,
		                      buffer_background_image_data);

		// Las balas solo se pintan mientras VUELAN. La bala cargada sigue
		// existiendo, pegada al canon, pero no se dibuja: si no, el tanque
		// pasearia una bala visible en el morro todo el rato.
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

	player_free(&player1);
	player_free(&player2);
	bmp_close_files();
	bmp_delete_buffers();

	printf("\n");
	printf("La bala no es un objeto aparte: es bullet_position_x/y,\n");
	printf("bullet_direction y bullet_is_flying dentro del tanque.\n");
	printf("\n");

	return 0;

}
