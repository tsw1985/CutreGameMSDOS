//===========================================================
// CAPITULO 21 - La ventana: sacar un trozo de un mapa grande
//
// El capitulo 20 te dejo perdiendo el tanque por el borde. Antes de
// arreglarlo con una camara, hay que entender la pieza de debajo:
//
//     como se saca un rectangulo de 320x200 de dentro de una imagen
//     de 640 de ancho
//
// Y no es obvio. Es justo lo contrario de obvio.
//
// AQUI NO HAY CAMARA. La ventana la mueves TU, a mano, con las flechas.
// El tanque esta QUIETO en el mundo, clavado en la posicion (300, 190), y
// no se mueve en ningun momento.
//
// Muevete y miralo: veras el tanque deslizarse por la pantalla sin haberse
// movido ni un pixel. Eso es una ventana.
//
// Lo que se aprende aqui:
//
//   1. Que es el STRIDE (paso de fila) y por que aqui deja de coincidir
//      con el ancho de la pantalla.
//   2. Por que un memcpy ya no vale, y por que hacen falta 200.
//   3. Que mover la ventana NO mueve el mundo.
//===========================================================

#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include "header\bmp.h"
#include "header\players.h"
#include "..\tutlib.h"

// El tanque no se mueve de aqui en todo el programa
#define TANK_WORLD_X 228
#define TANK_WORLD_Y 140

struct player tank;


int main(){

	int step;

	printf("\n");
	printf("CAPITULO 21 - La ventana\n");
	printf("\n");
	printf("  Mundo: 640x400.   Pantalla: 320x200.\n");
	printf("\n");
	printf("  AQUI NO HAY CAMARA. Las flechas mueven LA VENTANA, no un\n");
	printf("  tanque. El tanque esta clavado en el mundo en (%d, %d) y no\n", TANK_WORLD_X, TANK_WORLD_Y);
	printf("  se va a mover en ningun momento.\n");
	printf("\n");
	printf("  Flechas   mover la ventana de 4 en 4 pixeles\n");
	printf("  MAYUS     mover de 32 en 32 (mantener)\n");
	printf("  ESC       salir\n");
	printf("\n");
	printf("  Fijate en el tanque: se desliza por la pantalla sin moverse.\n");
	printf("\n");
	getch();

	bmp_init_buffers(640, 400);
	bmp_fill_background_in_main_buffer("..\\..\\res\\big.bmp");
	bmp_extract_pallete_from_file("..\\..\\res\\big.bmp");

	player_init(&tank);

	bmp_open_sprite_sheet("..\\..\\res\\sprites.bmp");
	tut_load_tank_sprites(&tank, 0);
	bmp_close_sprite_sheet();

	tank.position_x = TANK_WORLD_X;
	tank.position_y = TANK_WORLD_Y;
	tank.current_direction = MOVE_UP;

	//-------------------------------------------------------
	// La ventana empieza en la esquina de arriba a la izquierda del mundo.
	//
	// camera_x y camera_y son los DOS NUMEROS que la definen: donde esta su
	// esquina superior izquierda, en coordenadas de mundo.
	//
	// Aqui los movemos a mano. En el capitulo 22 los movera una funcion.
	//-------------------------------------------------------
	camera_x = 0;
	camera_y = 0;

	install_kbd();
	set_video_mode(0x0013);
	bmp_write_pallete_data_into_dac(buffer_palleta_data);

	do {

		step = 4;

		// Mayusculas: ir mas rapido, para cruzar el mapa sin aburrirse
		if (keys[0x2A] || keys[0x36]){
			step = 32;
		}

		if (keys[KEY_UP]){    camera_y = camera_y - step; }
		if (keys[KEY_DOWN]){  camera_y = camera_y + step; }
		if (keys[KEY_LEFT]){  camera_x = camera_x - step; }
		if (keys[KEY_RIGHT]){ camera_x = camera_x + step; }

		//---------------------------------------------------
		// EL CLAMP, a mano.
		//
		// La ventana no puede salirse del mundo. Su esquina izquierda va de
		// 0 a map_width - 320, y la de arriba de 0 a map_height - 200.
		//
		// Sin esto, bmp_draw_world_window() leeria de antes del principio
		// del mapa o de despues del final. En DOS eso no da error: te
		// dibuja basura, o cuelga.
		//
		// En el capitulo 22 esto lo hara bmp_camera_clamp() sola.
		//---------------------------------------------------
		if (camera_x < 0){ camera_x = 0; }
		if (camera_y < 0){ camera_y = 0; }
		if (camera_x > map_width  - WIDTH ){ camera_x = map_width  - WIDTH;  }
		if (camera_y > map_height - HEIGHT){ camera_y = map_height - HEIGHT; }

		//---------------------------------------------------
		// LA FUNCION DEL CAPITULO.
		//
		// bmp_draw_world_window() copia el rectangulo de 320x200 que empieza
		// en (camera_x, camera_y) del mundo, al buffer de pantalla.
		//
		// Por dentro NO es un memcpy. Son 200 memcpy de 320 bytes, uno por
		// fila, porque en un mapa de 640 de ancho las filas del trozo que
		// quieres NO estan pegadas en memoria. El doc lo explica con un
		// dibujo.
		//---------------------------------------------------
		bmp_draw_world_window(buffer_background_image_data);

		//---------------------------------------------------
		// Y el tanque, en su sitio de siempre MENOS la camara.
		//
		// tank.position_x no cambia NUNCA en este programa. Lo unico que
		// cambia es camera_x. Y aun asi el tanque se mueve por la pantalla.
		//
		// Ahi esta la idea entera del bloque: una cosa es donde ESTA algo y
		// otra donde se PINTA.
		//---------------------------------------------------
		draw_sprite_to_buffer(tut_pick_sprite(&tank), TANK_WIDTH, TANK_HEIGHT,
		                      (int)tank.position_x - camera_x,
		                      (int)tank.position_y - camera_y,
		                      buffer_background_image_data);

		wait_retrace();
		bmp_paint_image_data_to_vga(buffer_background_image_data);

	} while (!keys[KEY_ESC]);

	uninstall_kbd();
	set_video_mode(0x0003);

	printf("\n");
	printf("  El tanque, en el MUNDO    : (%u, %u)   <- no ha cambiado nunca\n",
	       tank.position_x, tank.position_y);
	printf("  La ventana                : (%d, %d)\n", camera_x, camera_y);
	printf("  El tanque, en la PANTALLA : (%d, %d)\n",
	       (int)tank.position_x - camera_x, (int)tank.position_y - camera_y);
	printf("\n");
	printf("  Si la de pantalla se sale de 0..319 / 0..199, el tanque estaba\n");
	printf("  fuera de la ventana y no lo veias. Y seguia exactamente en el\n");
	printf("  mismo sitio del mundo.\n");
	printf("\n");
	printf("  Ahora ya sabes que es una ventana. El capitulo 22 le pone a\n");
	printf("  esos dos numeros alguien que los mueva por ti.\n");
	printf("\n");

	player_free(&tank);
	bmp_close_files();
	bmp_delete_buffers();

	return 0;

}
