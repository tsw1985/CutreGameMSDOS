//===========================================================
// CAPITULO 8 - Dos jugadores
//
// A partir de aqui, todo lo que ya sabes (recortar sprites, animar, mirar
// antes de saltar) esta en ..\tutlib.h, para que este fichero contenga solo
// la idea nueva. Si algo de ahi no te suena, esta dicho de que capitulo es.
//
// Lo que se aprende aqui:
//
//   1. Que dos jugadores no cuestan casi nada si el estado esta en una
//      struct.
//   2. LA IDEA GRANDE DEL CURSO: separar "que teclas" de "de donde vienen
//      esas teclas". Esta es la que hara posible el juego en red sin tocar
//      nada de esto.
//   3. Por que cada jugador necesita su propia cadena if / else if.
//===========================================================

#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include "header\bmp.h"
#include "header\players.h"
#include "..\tutlib.h"


struct player player1;
struct player player2;


//===========================================================
// LA IDEA DEL CAPITULO.
//
// Esta funcion mira cinco teclas y devuelve UN BYTE con cinco bits:
//
//   bit 0  arriba      bit 3  derecha
//   bit 1  abajo       bit 4  disparar
//   bit 2  izquierda
//
// Parece un rodeo. Lo es, y es a proposito.
//
// A partir de este byte, TODO lo que viene despues deja de saber de donde
// salieron las teclas. Le da igual si las pulso este teclado, el teclado de
// otro ordenador, o si las genero una grabacion.
//
// Esa indiferencia es lo que hara que el capitulo 18 pueda meter el juego
// en red sin tocar una sola linea del movimiento ni de las colisiones. Los
// bits llegaran por el cable en vez de por el teclado, y ya esta.
//
// Los nombres NET_INPUT_* vienen de header/lockstep.h, el header real, para
// que sean exactamente los mismos bits que viajan por la red.
//===========================================================
static unsigned char read_input_from_keys(unsigned char key_up,
                                          unsigned char key_down,
                                          unsigned char key_left,
                                          unsigned char key_right,
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
// Y esta recibe el byte, no el teclado.
//
// Fijate en que NO consulta keys[] por ningun lado. No sabe que existe un
// teclado.
//
// El if / else if hace que solo entre una direccion por frame: es lo que
// impide las diagonales. Y por eso cada jugador necesita su PROPIA llamada
// a esta funcion: si compartieran la cadena, solo se movería uno de los dos
// por frame.
//===========================================================
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

}


int main(){

	unsigned char input1;
	unsigned char input2;

	printf("\n");
	printf("CAPITULO 8 - Dos jugadores\n");
	printf("\n");
	printf("  Jugador 1 (azul):  flechas\n");
	printf("  Jugador 2 (rojo):  W A S D\n");
	printf("  ESC para salir\n");
	printf("\n");
	printf("Los dos se mueven a la vez. Eso lo permite el manejador de\n");
	printf("teclado del capitulo 5.\n");
	printf("\n");
	getch();

	bmp_init_buffers(WIDTH, HEIGHT);
	bmp_fill_background_in_main_buffer("..\\..\\res\\cutre.bmp");
	bmp_fill_background_collision_in_buffer("..\\..\\res\\cutrecol.bmp");
	bmp_extract_pallete_from_file("..\\..\\res\\cutre.bmp");

	//-------------------------------------------------------
	// Dos tanques. Fijate en lo barato que sale: las mismas dos llamadas,
	// una por jugador. Eso es lo que se gano metiendo todo el estado de un
	// tanque en una struct en el capitulo 6.
	//
	// El 0 y el 21 son la fila de la hoja de sprites: el tanque azul esta
	// arriba y el rojo justo debajo.
	//-------------------------------------------------------
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

		// 1. El teclado se convierte en bits. AQUI acaba el teclado.
		input1 = read_input_from_keys(KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_NUMPAD_5);
		input2 = read_input_from_keys(KEY_W,  KEY_S,    KEY_A,    KEY_D,     KEY_G);

		// 2. De aqui en adelante solo hay bits. Una llamada por jugador.
		process_player_input(&player1, input1);
		process_player_input(&player2, input2);

		// 3. Dibujar
		bmp_draw_world_window(buffer_background_image_data);

		draw_sprite_to_buffer(tut_pick_sprite(&player1), TANK_WIDTH, TANK_HEIGHT,
		                      (int)player1.position_x, (int)player1.position_y,
		                      buffer_background_image_data);

		draw_sprite_to_buffer(tut_pick_sprite(&player2), TANK_WIDTH, TANK_HEIGHT,
		                      (int)player2.position_x, (int)player2.position_y,
		                      buffer_background_image_data);

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
	printf("El movimiento y las colisiones no saben que existe un teclado.\n");
	printf("Solo ven un byte con cinco bits. Acuerdate de esto en el\n");
	printf("capitulo 18.\n");
	printf("\n");

	return 0;

}
