#ifndef PLAYERS
#define PLAYERS


#define TANK_WIDTH 18
#define TANK_HEIGHT 18

#define TANK_BULLET_WIDTH  4
#define TANK_BULLET_HEIGHT 3


// How many pixels the tank moves per keypress, and how far "future" look-
// ahead collision checks look past the current cannon tip
#define PIXEL_TO_MOVE 2

// How many pixels the bullet travels per frame once it has been fired.
// Bigger than PIXEL_TO_MOVE on purpose, so a shot is clearly faster than
// the tank that fired it.
#define BULLET_PIXEL_TO_MOVE 3

// Offset (inside the bullet sprite box) of the pixel used to test the
// bullet against the collision map: the center of the sprite.
//
// bullet_position_x/y always hold the TOP-LEFT corner of the bullet,
// because that is what draw_sprite_to_buffer() needs. The collision point
// is worked out by adding these two, instead of storing the center and
// subtracting when drawing: the positions are "unsigned int", so a bullet
// close to the top or left edge of the screen would wrap around to 65535
// on a subtraction and read way outside the collision buffer.
#define BULLET_CENTER_X   (TANK_BULLET_WIDTH / 2)
#define BULLET_CENTER_Y   (TANK_BULLET_HEIGHT / 2)

#define MOVE_UP 				1
#define MOVE_DOWN 			2
#define MOVE_LEFT 			3
#define MOVE_RIGHT 			4

// Relative offset (from the sprite's top-left corner) of the cannon tip,
// inside the TANK_WIDTH x TANK_HEIGHT sprite box.
//
// All 4 are now confirmed by reading the tip pixel directly from the
// sprite sheet (sprites.bmp) and subtracting the frame 1 origin used in
// init_graphics() (in main.c) to extract that direction's sprite:
//
//   UP:    origin (2,5)   tip (10,5)  -> offset (8,0)
//   DOWN:  origin (43,10) tip (51,26) -> offset (8,16)
//   LEFT:  origin (83,8)  tip (83,16) -> offset (0,8)
//   RIGHT: origin (124,8) tip (139,16)-> offset (15,8)
#define CANNON_FINE_PX                           8

#define CANNON_TIP_OFFSET_UP_X 		CANNON_FINE_PX
#define CANNON_TIP_OFFSET_UP_Y 		0

#define CANNON_TIP_OFFSET_DOWN_X 	CANNON_FINE_PX
#define CANNON_TIP_OFFSET_DOWN_Y 	16

#define CANNON_TIP_OFFSET_LEFT_X 	0
#define CANNON_TIP_OFFSET_LEFT_Y 	CANNON_FINE_PX

#define CANNON_TIP_OFFSET_RIGHT_X 	15
#define CANNON_TIP_OFFSET_RIGHT_Y 	CANNON_FINE_PX

// Relative offset (from the sprite's top-left corner) of the 2 extra
// collision points: the tank's tracks. The cannon tip alone is not enough,
// because it only covers the middle of the tank: without these, a corner of
// the tank would go through the corner of a wall that the cannon missed.
//
// They are the two ends of the LEADING edge of the tank for that direction
// (the cannon is not counted, it has its own point):
//
//   going UP/DOWN    -> left track and right track, on the front row
//   going LEFT/RIGHT -> top track and bottom track, on the front column
//
// Same method as the cannon tip: the pixel read on the sprite sheet
// (sprites.bmp) minus the frame 1 origin used in init_graphics() to extract
// that direction's sprite.
//
//   UP:    origin (2,5)    tracks (4,10)  (16,10)  -> (2,5)  (14,5)
//   DOWN:  origin (43,10)  tracks (45,22) (58,22)  -> (2,12) (15,12)
//   LEFT:  origin (83,8)   tracks (87,9)  (87,23)  -> (4,1)  (4,15)
//   RIGHT: origin (124,8)  tracks (136,9) (136,23) -> (12,1) (12,15)
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
		
		unsigned int position_y;
		unsigned int position_x;
		unsigned int wins;
		unsigned int current_frame;
		unsigned int current_direction;
		unsigned int is_moving;
		
		
		// Absolute position (in the 320x200 screen) of the cannon tip,
		// one pair per direction. All four are kept up to date every
		// time player_update_cannon_tip() is called, regardless of which
		// direction the tank is currently facing, so that whichever one
		// matches "current_direction" is always ready to read/log/check.
		unsigned int canonn_head_top_up_x;
		unsigned int canonn_head_top_up_y;

		unsigned int canonn_head_top_down_x;
		unsigned int canonn_head_top_down_y;

		unsigned int canonn_head_top_left_x;
		unsigned int canonn_head_top_left_y;

		unsigned int canonn_head_top_right_x;
		unsigned int canonn_head_top_right_y;

		// Where the cannon tip WOULD be, one PIXEL_TO_MOVE step further in
		// a given direction, computed by
		// player_update_future_collision_points()
		// right before actually moving. Reading this position (instead of
		// the current tip) means it is not the tank's own sprite pixel
		// yet, so it is safe to check directly against VGA memory (A000).
		unsigned int future_cannon_tip_x;
		unsigned int future_cannon_tip_y;

		// The same look-ahead, for the 2 track points of that direction.
		// Movement is blocked if ANY of the three (cannon tip, track 1,
		// track 2) would land on a wall.
		unsigned int future_track1_x;
		unsigned int future_track1_y;

		unsigned int future_track2_x;
		unsigned int future_track2_y;

		// Absolute position (in the 320x200 screen) where the bullet
		// sprite is drawn. It always sits on the cannon tip of the
		// direction the tank is facing right now, which is exactly the
		// same pixel used for the collision check, so the bullet is
		// always visible at the mouth of the cannon. Kept up to date by
		// player_update_bullet_position().
		unsigned int bullet_position_x;
		unsigned int bullet_position_y;

		// Direction the bullet travels in. It is frozen at the moment of
		// the shot (copied from current_direction by player_fire_bullet())
		// and never changes afterwards, so the bullet keeps flying the way
		// the cannon was pointing even if the tank turns or moves away.
		unsigned int bullet_direction;

		// 0 = the bullet is "loaded": it sits on the cannon tip and follows
		//     the tank, and a new shot can be fired.
		// 1 = the bullet is flying: it moves on its own, ignores the tank,
		//     and no new shot can be fired until it dies (against a wall or
		//     by leaving the screen).
		unsigned int bullet_is_flying;


		
		//Animations
		unsigned int frame_counter;
		unsigned int total_frames;
		
		//Animation Speed
		unsigned int speed_counter;
		unsigned int speed_total;  // This is the "delay"
		
		
		char *sprite_tank_up;
		char *sprite_tank_up_2;

		char *sprite_tank_down;
		char *sprite_tank_down_2;
		
		
		char *sprite_tank_left;
		char *sprite_tank_left_2;
		
		
		char *sprite_tank_right;
		char *sprite_tank_right_2;
		
		char *sprite_tank_bullet;
		char *sprite_tank_bullet2;
		
		// to do : animation
	};

	
	void player_init(struct player *_player);
	void player_free(struct player *_player);
	void player_update_cannon_tip(struct player *_player);
	void player_update_future_collision_points(struct player *_player, int direction);
	void player_update_bullet_position(struct player *_player);
	void player_fire_bullet(struct player *_player);
	void player_move_bullet(struct player *_player);
	
	

#endif