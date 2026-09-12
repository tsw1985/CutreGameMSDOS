//===========================================================
// EL REPRODUCTOR DE DEMOS
//
// Reserva, reparte los efectos entre las imagenes, y suelta. Todo lo que
// no es un efecto en si mismo esta aqui.
//
// Lee demos.h antes que esto: ahi esta explicado por que la memoria se
// pide y se devuelve entera, y por que la musica se corta al terminar.
//===========================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>		// inp(), para el retrazo
#include <dos.h>
#include <bios.h>

#include "demos\demos.h"
#include "demos\demolib.h"

#include "demos\rotozoom\rotozoom.h"
#include "demos\wobble\wobble.h"
#include "demos\zoom\zoom.h"
#include "demos\scroll\scroll.h"
#include "demos\mosaic\mosaic.h"
#include "demos\blinds\blinds.h"
#include "demos\ripple\ripple.h"
#include "demos\bounce\bounce.h"
#include "demos\cycle\cycle.h"
#include "demos\stripes\stripes.h"


//-----------------------------------------------------------
// De src\sound.c. Se declaran a mano por lo mismo que las de bmp.c: para
// que esta carpeta dependa de funciones y no de los headers del juego.
//-----------------------------------------------------------
int  sound_start(void);
void sound_end(void);
void sound_update(void);
int  play_song(char *file_name);


// Ticks de la BIOS en un segundo. Son 18,2 de verdad, no 18, y por eso las
// cuentas van en long y se multiplica antes de dividir.
#define DEMO_TICKS_X10 	182

// Bytes de cabecera de un BMP antes de la paleta, y tamano de esta
#define DEMO_PALETTE_BYTES 	768

// El scancode / codigo ASCII de ESC
#define DEMO_KEY_ESCAPE 	27


//===========================================================
// LAS TABLAS
//===========================================================

int demo_sine[DEMO_ANGLE_STEPS] = {
	   0,    6,   13,   19,   25,   31,   38,   44,
	  50,   56,   62,   68,   74,   80,   86,   92,
	  98,  104,  109,  115,  121,  126,  132,  137,
	 142,  147,  152,  157,  162,  167,  172,  177,
	 181,  185,  190,  194,  198,  202,  206,  209,
	 213,  216,  220,  223,  226,  229,  231,  234,
	 237,  239,  241,  243,  245,  247,  248,  250,
	 251,  252,  253,  254,  255,  255,  256,  256,
	 256,  256,  256,  255,  255,  254,  253,  252,
	 251,  250,  248,  247,  245,  243,  241,  239,
	 237,  234,  231,  229,  226,  223,  220,  216,
	 213,  209,  206,  202,  198,  194,  190,  185,
	 181,  177,  172,  167,  162,  157,  152,  147,
	 142,  137,  132,  126,  121,  115,  109,  104,
	  98,   92,   86,   80,   74,   68,   62,   56,
	  50,   44,   38,   31,   25,   19,   13,    6,
	   0,   -6,  -13,  -19,  -25,  -31,  -38,  -44,
	 -50,  -56,  -62,  -68,  -74,  -80,  -86,  -92,
	 -98, -104, -109, -115, -121, -126, -132, -137,
	-142, -147, -152, -157, -162, -167, -172, -177,
	-181, -185, -190, -194, -198, -202, -206, -209,
	-213, -216, -220, -223, -226, -229, -231, -234,
	-237, -239, -241, -243, -245, -247, -248, -250,
	-251, -252, -253, -254, -255, -255, -256, -256,
	-256, -256, -256, -255, -255, -254, -253, -252,
	-251, -250, -248, -247, -245, -243, -241, -239,
	-237, -234, -231, -229, -226, -223, -220, -216,
	-213, -209, -206, -202, -198, -194, -190, -185,
	-181, -177, -172, -167, -162, -157, -152, -147,
	-142, -137, -132, -126, -121, -115, -109, -104,
	 -98,  -92,  -86,  -80,  -74,  -68,  -62,  -56,
	 -50,  -44,  -38,  -31,  -25,  -19,  -13,   -6
};

unsigned int demo_row[DEMO_HEIGHT];


void demo_tables_init(void){

	int y;

	// La tabla de senos ya viene escrita arriba, con los valores exactos,
	// para no necesitar math.h ni un solo float en todo el modulo.
	for (y = 0; y < DEMO_HEIGHT; y++){
		demo_row[y] = (unsigned int)y * DEMO_WIDTH;
	}

}


//===========================================================
// ESPERAR, MIRAR EL RELOJ Y MIRAR EL TECLADO
//===========================================================

void demo_wait_retrace(void){

	while (inp(0x3DA) & 0x08);
	while (!(inp(0x3DA) & 0x08));

}


//===========================================================
// El fin de frame. Ver demolib.h: aqui esta la razon de que la musica no se
// quede en bucle.
//===========================================================
void demo_show(unsigned char *screen){

	demo_wait_retrace();
	bmp_paint_image_data_to_vga(screen);
	sound_update();

}


void demo_sound(void){

	sound_update();

}


unsigned long demo_now(void){

	return (unsigned long)biostime(0, 0L);

}


//===========================================================
// 1 si se ha pulsado ESC.
//
// Cualquier otra tecla se lee y se tira: si no, se irian amontonando en el
// buffer de la BIOS y al volver el juego se encontraria con quince teclas
// pendientes.
//===========================================================
int demo_escape_pressed(void){

	int key;

	if (bioskey(1) == 0){
		return 0;
	}

	key = bioskey(0);

	if ((key & 0x00FF) == DEMO_KEY_ESCAPE){
		return 1;
	}

	return 0;

}


void demo_flush_keys(void){

	while (bioskey(1) != 0){
		bioskey(0);
	}

}


//===========================================================
// EL MODO DE VIDEO
//
// Puesto aqui con int86 y no llamando a set_vga_320_200_mode() de
// src\video.asm, que es lo que usa el juego. Un .asm mas es una cosa mas
// que hay que llevarse para reutilizar la carpeta, y esto son cuatro
// lineas.
//===========================================================
static void demo_set_video_mode(unsigned int mode){

	union REGS regs;

	regs.x.ax = mode;
	int86(0x10, &regs, &regs);

}


//===========================================================
// LOS FUNDIDOS
//
// Los mismos 32 pasos del juego, pero con su propio bucle, porque el del
// juego vive en src\main.c y main.c no se puede enlazar.
//
// sound_update() dentro, siempre. Un fundido son 33 retrazos, casi medio
// segundo, y sin refrescar el buffer DMA la tarjeta repetiria media
// muestra y la musica daria un salto en cada transicion. Es la misma razon
// por la que se llama dentro de la espera de red del juego.
//===========================================================
static void demo_fade_in(unsigned char *palette){

	int level;

	for (level = 0; level <= DEMO_FADE_STEPS; level++){
		demo_wait_retrace();
		bmp_write_pallete_data_into_dac_scaled(palette, level);
		sound_update();
	}

}


static void demo_fade_out(unsigned char *palette){

	int level;

	for (level = DEMO_FADE_STEPS; level >= 0; level--){
		demo_wait_retrace();
		bmp_write_pallete_data_into_dac_scaled(palette, level);
		sound_update();
	}

}


//===========================================================
// CARGAR UNA IMAGEN Y SU PALETA
//
// El BMP se abre una sola vez y se sacan las dos cosas del mismo
// descriptor: primero la paleta, que esta en el byte 54, y luego los
// pixeles, que estan en el 1078.
//
// Y hay que darle la vuelta. Los BMP guardan la ultima fila primero, asi
// que leidos tal cual salen del reves. bmp_revert_bmp() es exactamente
// para esto y es del juego, no una copia.
//
// Devuelve 1 si cargo, 0 si el fichero no estaba. Una imagen que falta se
// salta y la presentacion sigue: en una intro es preferible que falte una
// foto a que no arranque el juego.
//===========================================================
static int demo_load_image(char *path, unsigned char *image, unsigned char *palette){

	FILE *file;

	file = fopen(path, "rb");

	if (file == NULL){
		return 0;
	}

	bmp_load_pallete_data(palette, file);
	bmp_fill_buffer_with_image_data_from_file(image, file);

	fclose(file);

	bmp_revert_bmp(image);

	return 1;

}


//===========================================================
// LA COLECCION
//
// Anadir un efecto es escribir su .c en su carpeta, incluir su .h arriba y
// poner una linea aqui. No hay nada mas que tocar en ningun sitio.
//
// El orden importa poco, pero conviene no dejar juntos dos que se parezcan
// (wobble y ripple, scroll y stripes), para que la presentacion no parezca
// que se repite.
//===========================================================
static demo_effect_fn demo_effects[] = {
	demo_rotozoom,
	demo_wobble,
	demo_blinds,
	demo_zoom,
	demo_stripes,
	demo_cycle,
	demo_scroll,
	demo_ripple,
	demo_mosaic,
	demo_bounce
};

#define DEMO_EFFECT_COUNT 	(sizeof(demo_effects) / sizeof(demo_effects[0]))


//===========================================================
// Y LA FUNCION QUE LO JUNTA TODO
//===========================================================
int demo_run(char **image_paths, int image_count, unsigned int seconds_each){

	unsigned char *screen;
	unsigned char *image;
	unsigned char *palette;
	unsigned long ticks_each;
	unsigned long end_tick;
	int index;
	int effect;
	int finished;
	int had_sound;

	if (image_paths == NULL || image_count <= 0){
		return 1;
	}

	if (seconds_each == 0){
		seconds_each = DEMO_DEFAULT_SECONDS;
	}

	// 18,2 ticks por segundo. Multiplicar ANTES de dividir, y en long: con
	// enteros de 16 bits, 8 * 182 ya son 1456 y a los 360 segundos se
	// saldria.
	ticks_each = ((unsigned long)seconds_each * (unsigned long)DEMO_TICKS_X10) / 10UL;

	demo_tables_init();

	//-------------------------------------------------------
	// La memoria. Tres bloques y ni uno mas.
	//
	// Los dos grandes son de 64000, que caben de sobra en un malloc de 16
	// bits (el techo son 65535), asi que no hace falta farmalloc para
	// nada. Y se piden con el monton limpio, antes de que el juego haya
	// reservado nada.
	//-------------------------------------------------------
	screen  = (unsigned char *)malloc(DEMO_SCREEN);
	image   = (unsigned char *)malloc(DEMO_SCREEN);
	palette = (unsigned char *)malloc(DEMO_PALETTE_BYTES);

	if (screen == NULL || image == NULL || palette == NULL){

		// Devolver lo que si haya salido. Un fallo a medias que se queda a
		// medias es un agujero en el monton, y un agujero es lo que hace
		// que luego no quepa el mapa.
		if (screen != NULL){
			free(screen);
		}
		if (image != NULL){
			free(image);
		}
		if (palette != NULL){
			free(palette);
		}

		return 1;

	}

	//-------------------------------------------------------
	// El sonido, y se abre AQUI y no antes.
	//
	// Lo abre la demo y lo cierra la demo. Dejarlo vivo para que el juego
	// se lo encuentre puesto seria comodo y es justo lo que una vez dejo
	// al mapa de 256000 sin sitio contiguo. Que la musica vuelva a empezar
	// cuando arranca la partida es un precio pequeno al lado de eso.
	//-------------------------------------------------------
	had_sound = sound_start();

	if (had_sound == 1){
		play_song(DEMO_SONG);
	}

	demo_set_video_mode(0x0013);
	bmp_write_black_pallete_into_dac();

	demo_flush_keys();

	finished = 1;
	effect   = 0;

	for (index = 0; index < image_count; index++){

		//---------------------------------------------------
		// Alimentar la tarjeta a los dos lados de la carga.
		//
		// Leer 65078 bytes de disco es lo unico de todo el bucle que no
		// esta paceado por el retrazo, y la tarjeta se come medio buffer
		// cada tercio de segundo sin preguntar. En un disco duro esto son
		// milisegundos y no se nota; desde un disquete, se notaria.
		//---------------------------------------------------
		demo_sound();

		if (demo_load_image(image_paths[index], image, palette) == 0){
			demo_sound();
			continue;		// no estaba: a la siguiente
		}

		demo_sound();

		//---------------------------------------------------
		// La pantalla entra en negro SIEMPRE.
		//
		// No es por gusto: cada BMP trae su propia paleta y el VGA solo
		// tiene una, asi que pasar de una imagen a la siguiente sin
		// apagar la luz ensenaria un fotograma de la foto nueva con los
		// colores de la vieja. Por eso las intros de la epoca fundian
		// entre diapositiva y diapositiva.
		//---------------------------------------------------
		memcpy(screen, image, DEMO_SCREEN);
		bmp_paint_image_data_to_vga(screen);

		demo_fade_in(palette);

		end_tick = demo_now() + ticks_each;

		if (demo_effects[effect](image, screen, palette, end_tick) == 0){
			finished = 0;
		}

		//---------------------------------------------------
		// Y se apaga con la paleta que haya AHORA, que no tiene por que
		// ser la que se cargo: demo_cycle() la deja girada.
		//---------------------------------------------------
		demo_fade_out(palette);

		if (finished == 0){
			break;
		}

		effect++;
		if ((unsigned int)effect >= DEMO_EFFECT_COUNT){
			effect = 0;		// mas imagenes que efectos: se vuelve a empezar
		}

	}

	//-------------------------------------------------------
	// Y a devolverlo todo, en el orden contrario al que se pidio.
	//-------------------------------------------------------
	if (had_sound == 1){
		sound_end();
	}

	free(palette);
	free(image);
	free(screen);

	demo_flush_keys();

	//-------------------------------------------------------
	// Y de vuelta a modo texto.
	//
	// No es por dejarlo bonito: quien llame a esto puede querer imprimir
	// algo despues, y un printf sobre una pantalla en modo 13h no se ve.
	// En este juego, sin ir mas lejos, lo siguiente que pasa es el
	// emparejamiento por red, que habla por pantalla.
	//-------------------------------------------------------
	demo_set_video_mode(0x0003);

	return finished;

}
