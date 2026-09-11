#ifndef TUTLIB_H
#define TUTLIB_H

//===========================================================
// Lo unico que el curso NO puede sacar del juego.
//
// Estas tres cosas viven dentro de src/main.c, que tiene su propio main(),
// asi que no se pueden enlazar. Estan aqui, en UN solo sitio, para que
// ningun capitulo las repita:
//
//   set_video_mode()   main.c usa src/video.asm para lo mismo
//   wait_retrace()     main.c, al final del fichero
//   el teclado         main.c: keys[], new_kbd_handler(), install_kbd()
//
// Las dos primeras son de graficos y su sitio natural seria src/bmp.c. El
// teclado no: depende de que teclas use tu juego, asi que cada programa
// tiene el suyo. El capitulo 5 lo explica entero.
//
// Todo lo demas del curso sale de src/ y header/ sin copiar nada.
//===========================================================

#include <dos.h>
#include <conio.h>

#define IRQ_KEYBOARD 9

// Scancodes. NO son letras: son el numero de la TECLA FISICA.
#define KEY_ESC        0x01
#define KEY_TAB        0x0F
#define KEY_W          0x11
#define KEY_A          0x1E
#define KEY_S          0x1F
#define KEY_D          0x20
#define KEY_G          0x22
#define KEY_UP         0x48
#define KEY_DOWN       0x50
#define KEY_LEFT       0x4B
#define KEY_RIGHT      0x4D
#define KEY_NUMPAD_5   0x4C


// 1 si la tecla esta pulsada AHORA MISMO. Capitulo 5.
volatile unsigned char keys[128];

static void interrupt far (*tut_old_kbd_handler)();


//-------------------------------------------------------
// El manejador de INT 9. Se ejecuta cada vez que tocas una tecla.
//
// El bit 7 del scancode distingue pulsar de soltar, y de ahi sale poder
// leer varias teclas a la vez. Capitulo 5 para el detalle.
//-------------------------------------------------------
static void interrupt far tut_kbd_handler(){

	unsigned char scancode;

	scancode = inp(0x60);

	if (scancode & 0x80){
		keys[scancode - 128] = 0;
	}else{
		keys[scancode] = 1;
	}

	// Avisar al controlador de interrupciones. Sin esto no llega ninguna mas.
	outp(0x20, 0x20);

}


static void install_kbd(){

	tut_old_kbd_handler = getvect(IRQ_KEYBOARD);
	setvect(IRQ_KEYBOARD, tut_kbd_handler);

}


//-------------------------------------------------------
// OBLIGATORIO antes de salir. Si no devuelves el vector, DOS sigue
// apuntando a una funcion que ya no existe y la siguiente tecla cuelga la
// maquina.
//-------------------------------------------------------
static void uninstall_kbd(){

	setvect(IRQ_KEYBOARD, tut_old_kbd_handler);

}


//-------------------------------------------------------
// Pedir un modo de video a la BIOS.  0x13 = 320x200x256,  0x03 = texto.
//-------------------------------------------------------
static void set_video_mode(unsigned int mode){

	union REGS regs;

	regs.x.ax = mode;
	int86(0x10, &regs, &regs);

}


//-------------------------------------------------------
// Esperar al retrazo vertical. Capitulo 3.
//-------------------------------------------------------
static void wait_retrace(void){

	while (inp(0x3DA) & 0x08);
	while (!(inp(0x3DA) & 0x08));

}



//===========================================================
// ---- ANDAMIAJE DE TANQUE ----
//
// Esto NO es codigo del juego: es lo que los capitulos 4, 5, 6 y 7 ya te
// han ensenado, recogido aqui para que del capitulo 8 en adelante cada
// fichero contenga SOLO la idea nueva.
//
// Si algo de aqui no te suena, vuelve al capitulo donde se explica:
//
//   tut_load_tank_sprites()   capitulo 4 (recortar de la hoja)
//   tut_pick_sprite()         capitulo 6 (direccion + fotograma)
//   tut_update_animation()    capitulo 6 (el contador)
//   tut_try_move()            capitulo 7 (mirar antes de saltar)
//===========================================================

#include "header\bmp.h"
#include "header\players.h"


//-------------------------------------------------------
// Los 8 sprites de un tanque. sheet_row es la fila de la hoja:
// 0 para el tanque azul (jugador 1), 21 para el rojo (jugador 2).
//
// Las coordenadas son las del juego real, de src/main.c.
//-------------------------------------------------------
static void tut_load_tank_sprites(struct player *p, int sheet_row){

	bmp_extract_sprite((unsigned int)(  2), (unsigned int)(  5 + sheet_row), TANK_WIDTH, TANK_HEIGHT, p->sprite_tank_up);
	bmp_extract_sprite((unsigned int)( 23), (unsigned int)(  5 + sheet_row), TANK_WIDTH, TANK_HEIGHT, p->sprite_tank_up_2);

	bmp_extract_sprite((unsigned int)( 43), (unsigned int)( 10 + sheet_row), TANK_WIDTH, TANK_HEIGHT, p->sprite_tank_down);
	bmp_extract_sprite((unsigned int)( 63), (unsigned int)( 10 + sheet_row), TANK_WIDTH, TANK_HEIGHT, p->sprite_tank_down_2);

	bmp_extract_sprite((unsigned int)( 83), (unsigned int)(  8 + sheet_row), TANK_WIDTH, TANK_HEIGHT, p->sprite_tank_left);
	bmp_extract_sprite((unsigned int)(102), (unsigned int)(  8 + sheet_row), TANK_WIDTH, TANK_HEIGHT, p->sprite_tank_left_2);

	bmp_extract_sprite((unsigned int)(124), (unsigned int)(  8 + sheet_row), TANK_WIDTH, TANK_HEIGHT, p->sprite_tank_right);
	bmp_extract_sprite((unsigned int)(145), (unsigned int)(  8 + sheet_row), TANK_WIDTH, TANK_HEIGHT, p->sprite_tank_right_2);

	bmp_extract_sprite((unsigned int)(252), (unsigned int)( 14), TANK_BULLET_WIDTH, TANK_BULLET_HEIGHT, p->sprite_tank_bullet);

	// La explosion es 13x13, no 18x18. Recortarla con TANK_WIDTH se llevaria
	// el hueco de al lado. Coordenadas exactas de src/main.c.
	if (sheet_row == 0){
		bmp_extract_sprite((unsigned int)(171), (unsigned int)(11), EXPLOSION_WIDTH, EXPLOSION_HEIGHT, p->sprite_tank_explosion);
		bmp_extract_sprite((unsigned int)(190), (unsigned int)(11), EXPLOSION_WIDTH, EXPLOSION_HEIGHT, p->sprite_tank_explosion2);
	}else{
		bmp_extract_sprite((unsigned int)(171), (unsigned int)(32), EXPLOSION_WIDTH, EXPLOSION_HEIGHT, p->sprite_tank_explosion);
		bmp_extract_sprite((unsigned int)(192), (unsigned int)(32), EXPLOSION_WIDTH, EXPLOSION_HEIGHT, p->sprite_tank_explosion2);
	}

}


// Que dibujo toca: hacia donde mira, y cual de los dos fotogramas. Cap. 6.
static unsigned char *tut_pick_sprite(struct player *p){

	if (p->current_direction == MOVE_UP){
		if (p->current_frame == 0){ return p->sprite_tank_up; }
		return p->sprite_tank_up_2;
	}
	if (p->current_direction == MOVE_DOWN){
		if (p->current_frame == 0){ return p->sprite_tank_down; }
		return p->sprite_tank_down_2;
	}
	if (p->current_direction == MOVE_LEFT){
		if (p->current_frame == 0){ return p->sprite_tank_left; }
		return p->sprite_tank_left_2;
	}
	if (p->current_frame == 0){ return p->sprite_tank_right; }
	return p->sprite_tank_right_2;

}


// El contador de la animacion. Cap. 6.
static void tut_update_animation(struct player *p){

	p->speed_counter = p->speed_counter + 1;

	if (p->speed_counter >= p->speed_total){
		p->speed_counter = 0;
		p->current_frame = p->current_frame + 1;
		if (p->current_frame >= p->total_frames){
			p->current_frame = 0;
		}
	}

}


// Los 3 puntos del frente contra el mapa de muros. Cap. 7.
static int tut_blocked_by_wall(struct player *p, int direction){

	player_update_future_collision_points(p, direction);

	if (bmp_is_wall((int)p->future_cannon_tip_x, (int)p->future_cannon_tip_y) == 1){ return 1; }
	if (bmp_is_wall((int)p->future_track1_x,     (int)p->future_track1_y)     == 1){ return 1; }
	if (bmp_is_wall((int)p->future_track2_x,     (int)p->future_track2_y)     == 1){ return 1; }

	return 0;

}


// Mirar antes de saltar. Devuelve 1 si avanzo de verdad. Cap. 7.
static int tut_try_move(struct player *p, int direction){

	p->current_direction = direction;

	if (tut_blocked_by_wall(p, direction) == 1){
		return 0;
	}

	if (direction == MOVE_UP){
		p->position_y = p->position_y - PIXEL_TO_MOVE;
	}else if (direction == MOVE_DOWN){
		p->position_y = p->position_y + PIXEL_TO_MOVE;
	}else if (direction == MOVE_LEFT){
		p->position_x = p->position_x - PIXEL_TO_MOVE;
	}else if (direction == MOVE_RIGHT){
		p->position_x = p->position_x + PIXEL_TO_MOVE;
	}

	return 1;

}


#endif
