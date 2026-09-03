#ifndef PLAYERS
#define PLAYERS

//===========================================================
// A tank and everything it owns: where it is, where it looks, its bullet,
// its explosion and its sprites.
//
// This file and players.c only know GEOMETRY: pixels, boxes and offsets.
// Nothing here knows about the map, the screen or the keyboard. Whatever
// has to read the collision map or paint pixels lives in main.c.
//===========================================================


// Size of one tank sprite cell in sprites.bmp
#define TANK_WIDTH 18
#define TANK_HEIGHT 18

// Size of one bullet sprite cell in sprites.bmp
#define TANK_BULLET_WIDTH  4
#define TANK_BULLET_HEIGHT 3

// Pixels a tank advances per frame while its direction key is held
#define PIXEL_TO_MOVE 2

// Pixels a bullet advances per frame. Bigger than PIXEL_TO_MOVE on purpose,
// so a shot is clearly faster than the tank that fired it.
#define BULLET_PIXEL_TO_MOVE 3

// Offset of the single pixel that represents the bullet: its center.
//
// bullet_position_x/y hold the TOP-LEFT corner, because that is what
// draw_sprite_to_buffer() needs, and the center is worked out by ADDING
// these. Storing the center and subtracting instead would wrap round to
// 65535 near the top or left edge, since the positions are unsigned.
#define BULLET_CENTER_X   (TANK_BULLET_WIDTH / 2)
#define BULLET_CENTER_Y   (TANK_BULLET_HEIGHT / 2)

// The 4 facing directions. Also used as the bullet's flight direction.
#define MOVE_UP 				1
#define MOVE_DOWN 			2
#define MOVE_LEFT 			3
#define MOVE_RIGHT 			4

// Size of one explosion sprite cell. NOT the size of a tank: the explosion
// is 13x13 and the tank 18x18, so it needs its own size everywhere it is
// extracted and drawn.
#define EXPLOSION_WIDTH 		13
#define EXPLOSION_HEIGHT 		13

// Pixels to push the explosion in, so its 13x13 sits centered in the 18x18
// box the tank was filling
#define EXPLOSION_OFFSET_X 		((TANK_WIDTH  - EXPLOSION_WIDTH)  / 2)
#define EXPLOSION_OFFSET_Y 		((TANK_HEIGHT - EXPLOSION_HEIGHT) / 2)

// Sprites the explosion animation has
#define EXPLOSION_TOTAL_SPRITES 2

// How long the whole explosion lasts, and how long each of its 2 sprites
// stays on screen, in main loop iterations. The loop waits for one vertical
// retrace per iteration and mode 13h runs at about 70 Hz, so 35 is roughly
// half a second and swapping every 7 gives 5 flickers in it.
#define EXPLOSION_TOTAL_FRAMES 	35
#define EXPLOSION_FRAME_DELAY 	7

// Collision box for tank against tank: smaller than the sprite, so the two
// tanks may overlap a little before one stops the other. A margin of 2
// gives a box from position+2 to position+15, which is up to 4 pixels of
// visible overlap. Raise it for more overlap, lower it for less.
//
// It is a SQUARE, and the same one for the 4 directions, on purpose. The
// real hull is 17x13 facing up/down and 13x17 facing left/right, so a box
// that followed it would change shape when the tank turns, and a tank could
// turn straight into an overlap without having moved. Two overlapping tanks
// stay frozen for good, because every direction they try still overlaps.
#define TANK_COLLISION_MARGIN 	2
#define TANK_COLLISION_WIDTH 	(TANK_WIDTH  - (TANK_COLLISION_MARGIN * 2))
#define TANK_COLLISION_HEIGHT 	(TANK_HEIGHT - (TANK_COLLISION_MARGIN * 2))

// Where each tank starts, and where it goes back to when a round restarts
// after a hit. X and Y are the TOP-LEFT corner of the sprite, not its
// center, so a tank spans from X to X+17.
//
// Check any new value against cutrecol.bmp before using it: the whole 18x18
// box has to land on floor. A tank born inside a wall is stuck from the
// first frame, because every direction comes back blocked.
#define PLAYER1_START_X 			110
#define PLAYER1_START_Y 			164
#define PLAYER1_START_DIRECTION 	MOVE_UP

#define PLAYER2_START_X 			110
#define PLAYER2_START_Y 			16
#define PLAYER2_START_DIRECTION 	MOVE_DOWN

// Offset of the cannon tip inside the 18x18 sprite box, one pair per
// direction. This is the point that decides whether the tank can advance,
// and it is also where its bullet is born.
//
// Worked out as (tip pixel in sprites.bmp) - (frame 1 origin used by
// init_graphics() to extract that direction):
//
//   UP:    origin (2,5)   tip (10,5)   -> (8,0)
//   DOWN:  origin (43,10) tip (51,26)  -> (8,16)
//   LEFT:  origin (83,8)  tip (83,16)  -> (0,8)
//   RIGHT: origin (124,8) tip (139,16) -> (15,8)
//
// The values below are those, then tuned by eye until the tip grazed the
// walls right (hence the -1). They are allowed to be negative:
// player_add_offset() is what keeps that from wrapping round to 65535.
#define CANNON_FINE_PX              6  // 8

#define CANNON_TIP_OFFSET_UP_X 		CANNON_FINE_PX
#define CANNON_TIP_OFFSET_UP_Y 		-1  // 0

#define CANNON_TIP_OFFSET_DOWN_X 	CANNON_FINE_PX
#define CANNON_TIP_OFFSET_DOWN_Y 	16 // 16

#define CANNON_TIP_OFFSET_LEFT_X 	0  // 0
#define CANNON_TIP_OFFSET_LEFT_Y 	CANNON_FINE_PX

#define CANNON_TIP_OFFSET_RIGHT_X 	15 // 15
#define CANNON_TIP_OFFSET_RIGHT_Y 	CANNON_FINE_PX

// Offset of the 2 extra collision points, the tracks: the two ends of the
// leading edge of the tank for each direction. Same method as the cannon
// tip (pixel in sprites.bmp minus the frame 1 origin):
//
//   UP:    origin (2,5)    tracks (4,10)  (16,10)  -> (2,5)  (14,5)
//   DOWN:  origin (43,10)  tracks (45,22) (58,22)  -> (2,12) (15,12)
//   LEFT:  origin (83,8)   tracks (87,9)  (87,23)  -> (4,1)  (4,15)
//   RIGHT: origin (124,8)  tracks (136,9) (136,23) -> (12,1) (12,15)
//
// The cannon tip alone is not enough: it only covers the middle of the
// tank, so a corner of the hull would go through the corner of a wall the
// cannon passed next to. The tracks alone are not enough either: the cannon
// would go into a wall the tracks are not touching yet.
#define TRACK1_OFFSET_UP_X 			2
#define TRACK1_OFFSET_UP_Y 			5
#define TRACK2_OFFSET_UP_X 			14
#define TRACK2_OFFSET_UP_Y 			5

#define TRACK1_OFFSET_DOWN_X 		2
#define TRACK1_OFFSET_DOWN_Y 		12
#define TRACK2_OFFSET_DOWN_X 		15
#define TRACK2_OFFSET_DOWN_Y 		12

#define TRACK1_OFFSET_LEFT_X 		4
#define TRACK1_OFFSET_LEFT_Y 		1
#define TRACK2_OFFSET_LEFT_X 		4
#define TRACK2_OFFSET_LEFT_Y 		15

#define TRACK1_OFFSET_RIGHT_X 		12
#define TRACK1_OFFSET_RIGHT_Y 		1
#define TRACK2_OFFSET_RIGHT_X 		12
#define TRACK2_OFFSET_RIGHT_Y 		15

struct player{

		// ---- Where the tank is and what it is doing ----

		unsigned int position_y;			// top-left corner of the sprite, in screen pixels
		unsigned int position_x;
		unsigned int wins;					// rounds won, the only thing that survives a restart
		unsigned int current_frame;			// which track sprite is showing, 0 or 1
		unsigned int current_direction;		// MOVE_UP / DOWN / LEFT / RIGHT
		unsigned int is_moving;				// 1 if it moved this frame, so the tracks advance


		// ---- Cannon tip, in absolute screen pixels ----
		//
		// All 4 pairs are recomputed every frame by
		// player_update_cannon_tip(), so whichever matches
		// current_direction is always ready. Used to place the bullet and
		// by the debug log, NOT by the collision check.

		unsigned int canonn_head_top_up_x;
		unsigned int canonn_head_top_up_y;

		unsigned int canonn_head_top_down_x;
		unsigned int canonn_head_top_down_y;

		unsigned int canonn_head_top_left_x;
		unsigned int canonn_head_top_left_y;

		unsigned int canonn_head_top_right_x;
		unsigned int canonn_head_top_right_y;


		// ---- Look-ahead: where the tank WOULD be one step further ----
		//
		// All filled by player_update_future_collision_points() right
		// before trying to move. Checking these instead of the current
		// position is what stops the tank from entering a wall and then
		// having to get out of it.

		unsigned int future_cannon_tip_x;	// the 3 points the wall check reads
		unsigned int future_cannon_tip_y;

		unsigned int future_track1_x;
		unsigned int future_track1_y;

		unsigned int future_track2_x;
		unsigned int future_track2_y;

		unsigned int future_position_x;		// tentative corner, for the tank vs tank box
		unsigned int future_position_y;


		// ---- Bullet ----

		unsigned int bullet_position_x;		// top-left corner of the bullet sprite
		unsigned int bullet_position_y;

		// Direction the bullet flies in. Frozen at the moment of the shot
		// by player_fire_bullet(), so the bullet keeps going the way the
		// cannon was pointing even if the tank turns away.
		unsigned int bullet_direction;

		// 0 = loaded: it sits on the cannon tip, follows the tank, is not
		//     drawn, and a new shot can be fired.
		// 1 = flying: it moves on its own and no new shot is possible until
		//     it dies, against a wall, a tank, or the edge of the screen.
		unsigned int bullet_is_flying;


		// ---- Explosion ----

		// 1 = this tank has been hit: the explosion is drawn in its place
		//     and the whole round is frozen. Cleared by player_reset().
		unsigned int is_exploding;

		unsigned int explosion_counter;			// frames until the next sprite swap
		unsigned int explosion_current_frame;	// which explosion sprite shows, 0 or 1

		// Whether this player's fire key was already down last frame, so a
		// shot goes off only on the frame the key GOES down. Without it,
		// holding the key would fire again the moment the bullet died.
		// It lives here so adding a player does not add a variable to main().
		unsigned int fire_was_pressed;


		// ---- Sound ----
		// Which mixer voice and which sample this tank uses. They are here
		// so process_player_input() stays generic: it drives whichever
		// player it is given without knowing that player 1 is the one with
		// engip1.wav. Set in init_players(), see sound.h for the values.

		unsigned int sound_engine_voice;
		unsigned int sound_engine_sample;
		unsigned int sound_fire_voice;


		// ---- Track animation timing ----

		unsigned int frame_counter;
		unsigned int total_frames;			// how many track sprites there are

		unsigned int speed_counter;			// counts up to speed_total
		unsigned int speed_total;			// the "delay": bigger = slower tracks


		// ---- Sprites, all filled by init_graphics() from sprites.bmp ----
		// Two frames each, swapped by current_frame to animate the tracks

		char *sprite_tank_up;
		char *sprite_tank_up_2;

		char *sprite_tank_down;
		char *sprite_tank_down_2;

		char *sprite_tank_left;
		char *sprite_tank_left_2;

		char *sprite_tank_right;
		char *sprite_tank_right_2;

		char *sprite_tank_bullet;			// only _bullet is used, _bullet2 is spare
		char *sprite_tank_bullet2;

		char *sprite_tank_explosion;		// 13x13, not tank sized
		char *sprite_tank_explosion2;

	};


	// Reserves / releases this player's sprite buffers. Called once each.
	void player_init(struct player *_player);
	void player_free(struct player *_player);

	// Puts a tank back to how it starts a round: position, direction, bullet
	// loaded, explosion off. Used both to set the game up and to restart a
	// round after a hit, so both go through exactly the same code.
	void player_reset(struct player *_player, unsigned int start_x, unsigned int start_y, unsigned int start_direction);

	// Adds an offset to a position without ever going below 0. The
	// CANNON_TIP_OFFSET_* values can be negative and the positions are
	// unsigned, so without this a tank on row 0 would land on 65535.
	unsigned int player_add_offset(unsigned int position, int offset);

	// Recomputes the 4 cannon tips from the tank's current position
	void player_update_cannon_tip(struct player *_player);

	// Works out where the tank and its 3 collision points WOULD be one
	// PIXEL_TO_MOVE step further in "direction", without moving anything
	void player_update_future_collision_points(struct player *_player, int direction);

	// Puts the loaded bullet on the cannon tip of the current direction
	void player_update_bullet_position(struct player *_player);

	// Fires, if no bullet of this player is already in the air. Returns 1 if
	// the shot really went off and 0 if it did not, so the caller knows
	// whether there is anything to make a noise about.
	int player_fire_bullet(struct player *_player);

	// Advances a flying bullet, and kills it if it leaves the screen. Walls
	// and tanks are not its business: main.c decides those.
	void player_move_bullet(struct player *_player);

	// Starts / advances the explosion. How LONG it lasts is decided by the
	// pause counter in main.c, not here.
	void player_start_explosion(struct player *_player);
	void player_update_explosion(struct player *_player);

#endif
