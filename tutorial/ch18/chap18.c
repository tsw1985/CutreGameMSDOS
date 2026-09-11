//===========================================================
// CAPITULO 18 - Lockstep: el juego en red
//
// Este es el capitulo importante del bloque. Aqui esta la idea que hace que
// el juego funcione en red, y no es la que uno espera.
//
// Lo que se aprende aqui:
//
//   1. Por que NO se envian posiciones.
//   2. Que es el DETERMINISMO y por que es la condicion de todo.
//   3. Que es LOCKSTEP.
//   4. Que es el RETARDO DE ENTRADA y por que hace falta.
//   5. Por que aqui se cobra lo que se sembro en el capitulo 8.
//
// Dos maquinas:  ./launch_game_both.sh  y en las dos  cd tutorial\ch18
//===========================================================

#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include "header\bmp.h"
#include "header\players.h"
#include "header\util.h"
#include "header\net.h"
#include "header\lockstep.h"
#include "..\tutlib.h"


struct player player1;
struct player player2;

int local_player_is_1;


static unsigned char read_input_from_keys(unsigned char key_up, unsigned char key_down,
                                          unsigned char key_left, unsigned char key_right,
                                          unsigned char key_fire){

	unsigned char bits;

	bits = 0;

	if (keys[key_up]){    bits = bits | NET_INPUT_UP; }
	if (keys[key_down]){  bits = bits | NET_INPUT_DOWN; }
	if (keys[key_left]){  bits = bits | NET_INPUT_LEFT; }
	if (keys[key_right]){ bits = bits | NET_INPUT_RIGHT; }
	if (keys[key_fire]){  bits = bits | NET_INPUT_FIRE; }

	return bits;

}


//===========================================================
// IDENTICA a la del capitulo 8. Ni una linea cambiada.
//
// Y esa es la demostracion de que la abstraccion del capitulo 8 funcionaba:
// esta funcion no sabe si los bits vienen de este teclado o de un cable.
//===========================================================
static void process_player_input(struct player *p, unsigned char input_bits){

	int moved;

	moved = 0;

	if (input_bits & NET_INPUT_UP){
		moved = tut_try_move(p, MOVE_UP);
	}else if (input_bits & NET_INPUT_DOWN){
		moved = tut_try_move(p, MOVE_DOWN);
	}else if (input_bits & NET_INPUT_LEFT){
		moved = tut_try_move(p, MOVE_LEFT);
	}else if (input_bits & NET_INPUT_RIGHT){
		moved = tut_try_move(p, MOVE_RIGHT);
	}

	if (moved == 1){
		tut_update_animation(p);
	}

}


int main(){

	unsigned char local_input;
	unsigned char player1_input;
	unsigned char player2_input;
	int connection_lost;

	printf("\n");
	printf("CAPITULO 18 - Lockstep\n");
	printf("\n");

	//-------------------------------------------------------
	// POR QUE NO SE ENVIAN POSICIONES
	//
	// Lo primero que se le ocurre a uno es: "mando donde esta mi tanque, y
	// el otro lo dibuja ahi". Parece obvio y no funciona.
	//
	//   - El tanque del otro se ve a saltos, porque los paquetes no llegan
	//     a un ritmo perfecto.
	//   - Si un paquete se pierde, su tanque se teletransporta.
	//   - Y para dos tanques y sus balas hay que mandar bastantes bytes por
	//     frame.
	//
	// LO QUE SE HACE EN SU LUGAR:
	//
	// Se manda QUE TECLAS has pulsado. Un byte. Y las DOS maquinas simulan
	// la partida entera, los dos tanques.
	//
	// Si las dos hacen exactamente las mismas cuentas con las mismas
	// entradas, llegan al mismo resultado. No hace falta mandar posiciones
	// porque las dos las calculan.
	//
	// A eso se le llama LOCKSTEP, y la condicion para que funcione es el
	// DETERMINISMO: mismas entradas, mismo resultado, siempre. Nada de
	// numeros aleatorios sin semilla compartida, nada de depender del
	// reloj, nada de depender de la velocidad de la maquina.
	//
	// El juego ya era determinista sin pretenderlo: el tanque se mueve 2
	// pixeles por FRAME, no por milisegundo.
	//-------------------------------------------------------

	net_set_log(tanks_log);

	if (net_init() == 0){
		printf("\n  No hay driver IPX. Capitulo 15.\n\n");
		return 1;
	}

	printf("  Buscando al otro jugador...\n\n");

	if (net_find_opponent() == 0){
		printf("\n  No aparecio nadie.\n\n");
		net_shutdown();
		return 1;
	}

	local_player_is_1 = net_is_player1();

	printf("\n");
	if (local_player_is_1 == 1){
		printf("  Llevo el tanque 1 (azul).\n");
	}else{
		printf("  Llevo el tanque 2 (rojo).\n");
	}
	printf("\n");
	printf("  Los DOS se conducen con las flechas: da igual cual te toco.\n");
	printf("  ESC para salir.\n");
	printf("\n");
	printf("  Pulsa una tecla para empezar.\n");
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

	connection_lost = 0;

	install_kbd();
	set_video_mode(0x0013);
	bmp_write_pallete_data_into_dac(buffer_palleta_data);

	do {

		//===============================================
		// EL FRAME EN RED. Cinco pasos, y el orden es sagrado.
		//===============================================

		// 1. Mis teclas. Este es el unico sitio donde se toca el teclado.
		local_input = read_input_from_keys(KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_NUMPAD_5);

		//-----------------------------------------------
		// 2. Entregarlas, y AQUI esta el RETARDO DE ENTRADA.
		//
		// net_set_local_input() no guarda esas teclas para el frame de
		// ahora: las guarda para el frame ACTUAL + NET_INPUT_DELAY (5).
		//
		// O sea que lo que pulsas tarda 5 frames en surtir efecto. Unos 70
		// milisegundos. Se nota poquisimo.
		//
		// Y sirve para esto: cuando llegue el frame 100, las teclas del
		// otro para el frame 100 se mandaron en su frame 95, o sea hace 5
		// frames. Han tenido cinco frames de margen para llegar.
		//
		// Sin retardo, cada frame tendria que esperar a un paquete que
		// acaba de salir, y cualquier hipo de la red se veria como un
		// tiron.
		//
		// Aplicar tus teclas al instante y las del otro con retardo NO
		// vale: las dos maquinas harian cuentas distintas y la partida se
		// partiria en dos. Tienen que ir las dos igual de tarde.
		//-----------------------------------------------
		net_set_local_input(local_input);
		net_send_input();

		//-----------------------------------------------
		// 3. Esperar a que lleguen las suyas para ESTE frame.
		//
		// Este es el precio del lockstep: si el otro se retrasa, tu te
		// paras. Las dos maquinas van siempre por el mismo frame, nunca una
		// por delante.
		//
		// Con el retardo de 5 frames, esta espera casi siempre dura cero.
		//-----------------------------------------------
		while (net_has_remote_input() == 0){

			net_poll();

			if (net_connection_lost() == 1){
				connection_lost = 1;
				break;
			}

		}

		if (connection_lost == 1){ break; }

		net_poll();

		//-----------------------------------------------
		// 4. Repartir. Mis teclas van a MI tanque, las suyas al suyo.
		//
		// Y fijate: a partir de esta linea, el resto del programa es
		// EXACTAMENTE el del capitulo 8. Ni una linea distinta.
		//
		// Eso es lo que se sembro alli al separar "que teclas" de "de donde
		// vienen". Meter la red no ha obligado a tocar el movimiento, ni
		// las colisiones, ni el dibujado.
		//-----------------------------------------------
		if (local_player_is_1 == 1){
			player1_input = net_get_local_input();
			player2_input = net_get_remote_input();
		}else{
			player1_input = net_get_remote_input();
			player2_input = net_get_local_input();
		}

		process_player_input(&player1, player1_input);
		process_player_input(&player2, player2_input);

		bmp_draw_world_window(buffer_background_image_data);

		draw_sprite_to_buffer(tut_pick_sprite(&player1), TANK_WIDTH, TANK_HEIGHT,
		                      (int)player1.position_x, (int)player1.position_y,
		                      buffer_background_image_data);

		draw_sprite_to_buffer(tut_pick_sprite(&player2), TANK_WIDTH, TANK_HEIGHT,
		                      (int)player2.position_x, (int)player2.position_y,
		                      buffer_background_image_data);

		wait_retrace();
		bmp_paint_image_data_to_vga(buffer_background_image_data);

		//-----------------------------------------------
		// 5. Este frame ha terminado. Avanzar.
		//
		// Las dos maquinas llegan a esta linea con el mismo estado, y solo
		// entonces pasan al frame siguiente. Van siempre a la par.
		//-----------------------------------------------
		net_advance_frame();

	} while (!keys[KEY_ESC]);

	uninstall_kbd();
	set_video_mode(0x0003);

	if (connection_lost == 1){
		printf("\n  Se ha perdido la conexion.\n");
	}

	player_free(&player1);
	player_free(&player2);
	bmp_close_files();
	bmp_delete_buffers();
	net_shutdown();

	printf("\n");
	printf("  Por el cable ha viajado UN BYTE de teclas por frame y por\n");
	printf("  jugador. Todo lo demas lo han calculado las dos maquinas por\n");
	printf("  separado, y han llegado al mismo sitio.\n");
	printf("\n");

	return 0;

}
