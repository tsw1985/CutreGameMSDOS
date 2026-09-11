//===========================================================
// CAPITULO 24 - El radar de cercania: numeros con sprites
//
// El capitulo 22 te dio una camara que sigue a TU tanque. Eso resolvio un
// problema y creo otro: en un mundo de cuatro pantallas, el otro tanque
// esta casi siempre fuera de lo que ves, y encontrarlo es dar vueltas al
// azar hasta tropezarte con el.
//
// Esto lo arregla con un numero en la parte de abajo de la pantalla, de
// 000% a 100%, dibujado con SPRITES y no con texto: en el modo 13h no hay
// printf que valga, la pantalla es un buffer de pixeles.
//
// Lo que se aprende aqui:
//
//   1. Una hoja de sprites que no son tanques: 11 celdas, 0..9 y el %.
//   2. Reservar y soltar 11 sprites en bloque, con el patron TODO O NADA.
//   3. Un HUD: por que este es el UNICO dibujo que NO resta la camara.
//   4. Componer un numero con sprites: dividir entre 10 y coger el resto.
//   5. Medir una distancia SIN coma flotante, y sin desbordar 16 bits.
//   6. El contorno: dibujar la SILUETA de un sprite en un color plano.
//
// Teclas:
//   flechas   mueven el tanque azul (el que sigue la camara)
//   W A S D   mueven el tanque rojo (para que veas cambiar el numero)
//   T         quita y pone el contorno negro
//   ESC       salir
//===========================================================

#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <alloc.h>

#include "header\bmp.h"
#include "header\players.h"
#include "..\tutlib.h"

#define KEY_T 0x14


//-------------------------------------------------------
// EL SITIO DEL RADAR EN LA PANTALLA
//
// Tres digitos y el %, siempre. 9 se dibuja como 009, y esto no es un
// capricho: si el numero cambiase de ancho al pasar de 9 a 10, un numero
// centrado saltaria de sitio. Un cartel que salta es un cartel que miras
// en vez de jugar.
//
// RADAR_WIDTH no es 4 * NUMBER_WIDTH. Las tres primeras celdas solo avanzan
// NUMBER_ADVANCE (13), pero la ultima ocupa su ancho entero (18):
//
//     |<-13->|<-13->|<-13->|<----18---->|
//     [  0   ][  0   ][  9  ][     %    ]
//     |<--------- 57 pixeles ---------->|
//
// Y todo son CONSTANTES: se calculan al compilar, no cada frame.
//-------------------------------------------------------
#define RADAR_DIGITS			3
#define RADAR_CELLS				(RADAR_DIGITS + 1)
#define RADAR_WIDTH				(((RADAR_CELLS - 1) * NUMBER_ADVANCE) + NUMBER_WIDTH)

#define RADAR_MARGIN_BOTTOM		2
#define RADAR_X					((WIDTH - RADAR_WIDTH) / 2)
#define RADAR_Y					(HEIGHT - NUMBER_HEIGHT - RADAR_MARGIN_BOTTOM)

// El color del contorno. El 0 es negro puro en la paleta de los tres temas
// (comprobado entrada por entrada), asi que no hace falta un color distinto
// para cada uno.
#define RADAR_OUTLINE_COLOR		0
#define RADAR_OUTLINE_STEPS		4


//-------------------------------------------------------
// Los 11 sprites.
//
// Fijate en una cosa: estos nombres los DECLARA header\players.h, con
// extern, y quien los define es el juego en src\main.c. Aqui los define
// este capitulo. Los dos sitios son validos porque un extern dice "esto
// existe en alguna parte", y el enlazador lo encuentra donde este.
//
// Y no estan dentro de struct player a proposito: el radar es UN cartel
// para toda la pantalla, no algo que tenga cada tanque. Metidos en la
// struct habria dos juegos identicos de 11 sprites, uno por jugador, para
// nada.
//-------------------------------------------------------
char *number_0 = NULL;
char *number_1 = NULL;
char *number_2 = NULL;
char *number_3 = NULL;
char *number_4 = NULL;
char *number_5 = NULL;
char *number_6 = NULL;
char *number_7 = NULL;
char *number_8 = NULL;
char *number_9 = NULL;
char *number_percent = NULL;

// Los cuatro sitios donde se estampa el contorno: un pixel a la izquierda,
// a la derecha, arriba y abajo. Las diagonales no: costarian la mitad mas
// para un grosor que a este tamano no se ve.
static int radar_outline_x[RADAR_OUTLINE_STEPS] = { -1,  1,  0,  0 };
static int radar_outline_y[RADAR_OUTLINE_STEPS] = {  0,  0, -1,  1 };

struct player tank1;
struct player tank2;

int outline_on;

static void free_sprite_numbers();


//===========================================================
// RESERVAR Y RECORTAR LOS 11 SPRITES
//
// numbers.bmp es una hoja de 320x200 exactamente igual que sprites.bmp, y
// se recorta con el MISMO cortador, bmp_extract_sprite(), sin ningun caso
// especial. Lo unico que hay que saber es la rejilla:
//
//     +--------+--------+--------+     +--------+
//     |   0    |   1    |   2    | ... |   %    |
//     |  18x18 |  18x18 |  18x18 |     |  18x18 |
//     +--------+--------+--------+     +--------+
//     x=0      x=18     x=36           x=180
//
// Once celdas de 18x18 en UNA fila, la celda N empieza en x = N * 18, y la
// tinta de cada cifra ocupa las filas 2 a 15 de su celda. Nada mas.
//
// PATRON TODO O NADA: si un malloc falla, se devuelve lo ya reservado y los
// 11 se quedan a NULL. Un juego a medias con un NULL en medio se dibujaria
// y se colgaria; asi, simplemente no hay radar.
//===========================================================
static int load_sprite_numbers(char *file){

	char **target[NUMBER_TOTAL_SPRITES];
	unsigned int cell;

	target[0]  = &number_0;
	target[1]  = &number_1;
	target[2]  = &number_2;
	target[3]  = &number_3;
	target[4]  = &number_4;
	target[5]  = &number_5;
	target[6]  = &number_6;
	target[7]  = &number_7;
	target[8]  = &number_8;
	target[9]  = &number_9;
	target[NUMBER_PERCENT_CELL] = &number_percent;

	// 11 celdas de 18x18 son 3564 bytes. Se piden DESPUES del mapa, que son
	// 256000: lo grande primero, sobre un monton limpio. Capitulo 23.
	for (cell = 0; cell < NUMBER_TOTAL_SPRITES; cell++){

		*target[cell] = (char *)malloc(NUMBER_WIDTH * NUMBER_HEIGHT);

		if (*target[cell] == NULL){
			free_sprite_numbers();
			return 0;
		}

	}

	bmp_open_sprite_sheet(file);

	for (cell = 0; cell < NUMBER_TOTAL_SPRITES; cell++){

		bmp_extract_sprite(cell * NUMBER_WIDTH,
		                   0,
		                   NUMBER_WIDTH,
		                   NUMBER_HEIGHT,
		                   *target[cell]);

	}

	bmp_close_sprite_sheet();

	return 1;

}


//===========================================================
// Devolverlos. Vale llamarla sobre un juego que nunca se reservo, que es
// como load_sprite_numbers() se limpia a si misma.
//===========================================================
static void free_sprite_numbers(){

	char **target[NUMBER_TOTAL_SPRITES];
	unsigned int cell;

	target[0]  = &number_0;
	target[1]  = &number_1;
	target[2]  = &number_2;
	target[3]  = &number_3;
	target[4]  = &number_4;
	target[5]  = &number_5;
	target[6]  = &number_6;
	target[7]  = &number_7;
	target[8]  = &number_8;
	target[9]  = &number_9;
	target[NUMBER_PERCENT_CELL] = &number_percent;

	for (cell = 0; cell < NUMBER_TOTAL_SPRITES; cell++){

		if (*target[cell] != NULL){
			free(*target[cell]);
			*target[cell] = NULL;
		}

	}

}


//===========================================================
// LA CERCANIA, DE 0 A 100
//
// Todo con enteros, a proposito. En este proyecto no hay ni un float, y un
// cartel no es motivo para enlazar la libreria de coma flotante de Turbo C
// en un programa que cuenta sus bytes.
//
// Entonces, la distancia real seria:
//
//     distancia = raiz(dx*dx + dy*dy)
//
// y esa raiz no la queremos. La aproximacion clasica es:
//
//     distancia = mayor + (menor / 2)
//
// Se queda a un 11% de la de verdad y cuesta una comparacion, una suma y un
// desplazamiento. Las otras dos candidatas eran peores AQUI:
//
//   |dx| + |dy|        (Manhattan) castiga las diagonales, y la diagonal es
//                      justo donde empieza el otro tanque
//   maximo(|dx|,|dy|)  (Chebyshev) hace lo contrario: premia las diagonales
//
// Comprobado sobre el codigo de verdad: 100 pixeles en recto dan 88%, y
// 100 pixeles en diagonal dan 87%. Eso es lo que se busca.
//===========================================================
static int compute_proximity_percent(struct player *a, struct player *b){

	int dx;
	int dy;
	int swap;
	long distance;
	long worst_distance;
	long percent;

	//---------------------------------------------------
	// TRAMPA 1: el cast a int ANTES de restar.
	//
	// position_x es unsigned. Si el tanque a esta a la IZQUIERDA del b, la
	// resta en unsigned no da negativo: da 65000 y pico, porque da la
	// vuelta por abajo. El radar marcaria 0% cada vez que los tanques
	// estuviesen del reves.
	//---------------------------------------------------
	dx = (int)a->position_x - (int)b->position_x;
	dy = (int)a->position_y - (int)b->position_y;

	if (dx < 0){
		dx = -dx;
	}
	if (dy < 0){
		dy = -dy;
	}

	// Dejar el mayor en dx, para poder escribir la formula tal cual
	if (dx < dy){
		swap = dx;
		dx = dy;
		dy = swap;
	}

	distance = (long)dx + ((long)dy / 2L);

	// La misma formula sobre el mundo entero, para tener la escala. Sale de
	// map_width y map_height, asi que no hay ningun 640 escrito a mano: el
	// dia que cargues un mapa de otro tamano, el radar se recalibra solo.
	// Un tanque es una caja y no un punto, de ahi el menos TANK_WIDTH.
	if (map_width > map_height){
		worst_distance = (long)(map_width  - TANK_WIDTH)
		               + ((long)(map_height - TANK_HEIGHT) / 2L);
	}else{
		worst_distance = (long)(map_height - TANK_HEIGHT)
		               + ((long)(map_width  - TANK_WIDTH) / 2L);
	}

	if (worst_distance <= 0){
		return 0;
	}

	//---------------------------------------------------
	// TRAMPA 2: esto TIENE que ir en long.
	//
	// distance * 100 llega a unos 78000 en el mapa de 640x400, y un
	// unsigned int se queda en 65535. En 16 bits la cuenta daria la vuelta
	// y el radar marcaria un alegre 100% en la otra punta del mundo.
	//---------------------------------------------------
	percent = 100L - ((distance * 100L) / worst_distance);

	if (percent < 0){
		percent = 0;
	}
	if (percent > 100){
		percent = 100;
	}

	return (int)percent;

}


//===========================================================
// PINTAR EL RADAR
//
// AQUI ESTA LA IDEA DEL CAPITULO, y es una resta que NO se hace.
//
// Desde el capitulo 20, todo lo que se dibuja lleva la misma cuenta:
//
//     pantalla = mundo - camara
//
// Los tanques la llevan, las balas la llevan, la explosion la lleva. El
// radar NO, porque el radar no esta EN el mundo: esta en la pantalla. Su
// sitio son las mismas 4 celdas pase lo que pase, y por eso aqui no
// aparecen camera_x ni camera_y por ningun lado.
//
// Es la misma regla del capitulo 22 vista del otro lado: si la camara
// decide donde va algo, ese algo es decoracion. El radar es decoracion
// pura, y por eso tampoco entra nunca en el checksum de la red: las dos
// maquinas lo calculan cada una por su cuenta y les sale lo mismo, porque
// las dos simulan las dos posiciones.
//===========================================================
static void draw_proximity_radar(struct player *a, struct player *b){

	char *figure[RADAR_CELLS];
	char *digit[10];
	int percent;
	int value;
	int cell;
	int step;

	// Un solo NULL vale por los 11: o estan todos o no esta ninguno
	if (number_0 == NULL){
		return;
	}

	digit[0] = number_0;
	digit[1] = number_1;
	digit[2] = number_2;
	digit[3] = number_3;
	digit[4] = number_4;
	digit[5] = number_5;
	digit[6] = number_6;
	digit[7] = number_7;
	digit[8] = number_8;
	digit[9] = number_9;

	percent = compute_proximity_percent(a, b);

	//---------------------------------------------------
	// DE UN NUMERO A CUATRO SPRITES
	//
	// De derecha a izquierda, que es como se saca un numero a cachos:
	//
	//     % 10   da la cifra de las unidades
	//     / 10   tira esa cifra y deja el resto
	//
	// Con 50:   50 % 10 = 0  ->  celda 2      50 / 10 = 5
	//            5 % 10 = 5  ->  celda 1       5 / 10 = 0
	//            0 % 10 = 0  ->  celda 0
	//
	//     [0][5][0][%]
	//
	// El cero de la izquierda sale SOLO: no hay que hacer nada especial
	// para rellenar, porque el bucle da tres vueltas siempre.
	//---------------------------------------------------
	value = percent;

	for (cell = RADAR_DIGITS - 1; cell >= 0; cell--){
		figure[cell] = digit[value % 10];
		value = value / 10;
	}

	figure[RADAR_DIGITS] = number_percent;

	//---------------------------------------------------
	// EL CONTORNO, EN DOS PASADAS
	//
	// El radar no tiene fondo propio: cae sobre el trozo de mapa que la
	// camara este ensenando. Sobre el suelo oscuro del tema SKYNET las
	// cifras se leen de maravilla y sobre el muro de piedra clara del tema
	// MILITAR casi desaparecen. Un contorno negro lo arregla sin tocar el
	// .bmp que dibujo el artista.
	//
	// Y las dos pasadas no se pueden juntar en una. Las cifras van a 13
	// pixeles de distancia y su tinta llega a medir 14, asi que cada celda
	// pisa un poco la anterior. Si cada cifra se contornease y se pintase
	// antes de pasar a la siguiente, el contorno negro de una se comeria el
	// borde derecho de la que ya estaba pintada:
	//
	//     bien:  contorno contorno contorno contorno
	//            cifra    cifra    cifra    cifra
	//
	//     mal:   contorno cifra  contorno cifra  ...
	//                            ^ este contorno pisa la cifra anterior
	//---------------------------------------------------
	if (outline_on == 1){

		for (cell = 0; cell < RADAR_CELLS; cell++){

			for (step = 0; step < RADAR_OUTLINE_STEPS; step++){

				//---------------------------------------
				// draw_sprite_silhouette_to_buffer() es de src\bmp.c, la
				// funcion REAL del juego. Dibuja el sprite pero escribiendo
				// UN COLOR PLANO: la forma dice DONDE escribir y el color
				// dice QUE escribir.
				//
				// El color 0 sigue siendo el transparente en el ORIGEN, que
				// es lo que se lee de la hoja. Lo que se escribe en la
				// pantalla es otra cosa, y ahi el 0 es negro y normal.
				//---------------------------------------
				draw_sprite_silhouette_to_buffer(figure[cell],
				                      NUMBER_WIDTH,
				                      NUMBER_HEIGHT,
				                      RADAR_X + (cell * NUMBER_ADVANCE) + radar_outline_x[step],
				                      RADAR_Y + radar_outline_y[step],
				                      RADAR_OUTLINE_COLOR,
				                      buffer_background_image_data);

			}

		}

	}

	// Y ahora las cifras de verdad, encima del contorno
	for (cell = 0; cell < RADAR_CELLS; cell++){

		draw_sprite_to_buffer(figure[cell],
		                      NUMBER_WIDTH,
		                      NUMBER_HEIGHT,
		                      RADAR_X + (cell * NUMBER_ADVANCE),
		                      RADAR_Y,
		                      buffer_background_image_data);

	}

}


int main(){

	int t_was_down;

	printf("\n");
	printf("CAPITULO 24 - El radar de cercania\n");
	printf("\n");
	printf("  Dos tanques en el mundo de 640x400. La camara sigue al AZUL,\n");
	printf("  asi que el rojo casi siempre esta fuera de la pantalla.\n");
	printf("\n");
	printf("  El numero de abajo dice como de cerca esta, de 000%% a 100%%.\n");
	printf("\n");
	printf("  flechas   mueven el tanque azul\n");
	printf("  W A S D   mueven el tanque rojo\n");
	printf("  T         quita y pone el contorno negro\n");
	printf("  ESC       salir\n");
	printf("\n");
	printf("  Pulsa una tecla.\n");

	getch();

	//-------------------------------------------------------
	// EL TEMA MANDA, Y NO SOLO EN EL DIBUJO
	//
	// Se carga el nivel 1 del tema SKYNET, que es lo mismo que hace el
	// juego con   -sky -level1.
	//
	// Y hay una razon dura para que el radar necesite un tema: la paleta.
	// El DAC se carga de la paleta del MAPA, y numbers.bmp esta pintado con
	// la paleta de SU tema. Son la misma, asi que las cifras salen del color
	// que el artista quiso. Si mezclas, no: entre big.bmp (el mapa original
	// sin tema) y el numbers.bmp de SKYNET hay 254 entradas de paleta
	// distintas de 256, y las cifras saldrian de colores al azar.
	//
	// Por eso en el juego el radar solo existe con -sky, -war o -neon.
	//-------------------------------------------------------
	bmp_init_buffers(640, 400);
	bmp_fill_background_in_main_buffer("..\\..\\res\\15Level\\SKYNET\\NIVEL01\\big.bmp");
	bmp_fill_background_collision_in_buffer("..\\..\\res\\15Level\\SKYNET\\NIVEL01\\bigcol.bmp");
	bmp_extract_pallete_from_file("..\\..\\res\\15Level\\SKYNET\\NIVEL01\\big.bmp");

	player_init(&tank1);
	player_init(&tank2);

	bmp_open_sprite_sheet("..\\..\\res\\spr_sky.bmp");
	tut_load_tank_sprites(&tank1, 0);
	tut_load_tank_sprites(&tank2, 21);
	bmp_close_sprite_sheet();

	//-------------------------------------------------------
	// Y AHORA LOS NUMEROS.
	//
	// Despues de cerrar la hoja de tanques, y no es un detalle de estilo:
	// bmp_open_sprite_sheet() trabaja sobre UN SOLO FILE * global (esta en
	// src\bmp.c). Abrir numbers.bmp sin haber cerrado la otra hoja se
	// llevaria por delante el manejador de la primera.
	//-------------------------------------------------------
	if (load_sprite_numbers("..\\..\\res\\Numbers\\SKYNET\\numbers.bmp") == 0){
		printf("\n  No hay memoria para los numeros.\n");
		bmp_delete_buffers();
		return 1;
	}

	player_reset(&tank1, BIG_PLAYER1_START_X, BIG_PLAYER1_START_Y, BIG_PLAYER1_START_DIRECTION);
	player_reset(&tank2, BIG_PLAYER2_START_X, BIG_PLAYER2_START_Y, BIG_PLAYER2_START_DIRECTION);

	bmp_camera_snap((int)tank1.position_x, (int)tank1.position_y, TANK_WIDTH, TANK_HEIGHT);

	outline_on = 1;
	t_was_down = 0;

	install_kbd();
	set_video_mode(0x0013);
	bmp_write_pallete_data_into_dac(buffer_palleta_data);

	do {

		// Detector de flanco: la T hace efecto en el frame en que BAJA, no
		// mientras esta pulsada. Capitulo 9.
		if (keys[KEY_T]){
			if (t_was_down == 0){
				if (outline_on == 1){
					outline_on = 0;
				}else{
					outline_on = 1;
				}
			}
			t_was_down = 1;
		}else{
			t_was_down = 0;
		}

		// Tanque azul
		if (keys[KEY_UP]){
			tut_try_move(&tank1, MOVE_UP);
			tut_update_animation(&tank1);
		}else if (keys[KEY_DOWN]){
			tut_try_move(&tank1, MOVE_DOWN);
			tut_update_animation(&tank1);
		}else if (keys[KEY_LEFT]){
			tut_try_move(&tank1, MOVE_LEFT);
			tut_update_animation(&tank1);
		}else if (keys[KEY_RIGHT]){
			tut_try_move(&tank1, MOVE_RIGHT);
			tut_update_animation(&tank1);
		}

		// Tanque rojo
		if (keys[KEY_W]){
			tut_try_move(&tank2, MOVE_UP);
			tut_update_animation(&tank2);
		}else if (keys[KEY_S]){
			tut_try_move(&tank2, MOVE_DOWN);
			tut_update_animation(&tank2);
		}else if (keys[KEY_A]){
			tut_try_move(&tank2, MOVE_LEFT);
			tut_update_animation(&tank2);
		}else if (keys[KEY_D]){
			tut_try_move(&tank2, MOVE_RIGHT);
			tut_update_animation(&tank2);
		}

		// La camara, antes de pintar nada, para que el fondo y lo que va
		// encima esten de acuerdo sobre el mismo frame. Capitulo 22.
		bmp_camera_follow((int)tank1.position_x, (int)tank1.position_y, TANK_WIDTH, TANK_HEIGHT);

		// El trozo de mundo que toca ver. Capitulo 21.
		bmp_draw_world_window(buffer_background_image_data);

		//===============================================
		// LOS TANQUES, EN COORDENADAS DE MUNDO
		//
		// Estos SI restan la camara: estan en el mundo. El rojo puede caer
		// fuera de la pantalla, y no pasa nada: draw_sprite_to_buffer()
		// recorta lo que se sale, y con coordenadas CON SIGNO, que es la
		// otra trampa del capitulo 4.
		//===============================================
		draw_sprite_to_buffer(tut_pick_sprite(&tank1),
		                      TANK_WIDTH, TANK_HEIGHT,
		                      (int)tank1.position_x - camera_x,
		                      (int)tank1.position_y - camera_y,
		                      buffer_background_image_data);

		draw_sprite_to_buffer(tut_pick_sprite(&tank2),
		                      TANK_WIDTH, TANK_HEIGHT,
		                      (int)tank2.position_x - camera_x,
		                      (int)tank2.position_y - camera_y,
		                      buffer_background_image_data);

		//===============================================
		// Y EL RADAR, EN COORDENADAS DE PANTALLA.
		//
		// El ultimo, para que ningun tanque pueda pintarse encima. Y sin
		// restar la camara, que es de lo que va este capitulo.
		//===============================================
		draw_proximity_radar(&tank1, &tank2);

		wait_retrace();
		bmp_paint_image_data_to_vga(buffer_background_image_data);

	} while (!keys[KEY_ESC]);

	set_video_mode(0x0003);
	uninstall_kbd();

	free_sprite_numbers();
	player_free(&tank1);
	player_free(&tank2);
	bmp_close_files();
	bmp_delete_buffers();

	printf("\n");
	printf("  Lo que acabas de ver:\n");
	printf("\n");
	printf("    - 11 sprites recortados de una hoja de 11 celdas de 18x18\n");
	printf("    - un numero compuesto con %% 10 y / 10, tres cifras fijas\n");
	printf("    - el UNICO dibujo del juego que no resta la camara\n");
	printf("    - una distancia sin raiz cuadrada y sin coma flotante\n");
	printf("\n");

	return 0;

}
