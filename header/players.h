#ifndef PLAYERS
#define PLAYERS


#define TANK_WIDTH 18
#define TANK_HEIGHT 18

#define TANK_BULLET_WIDTH  4
#define TANK_BULLET_HEIGHT 3


// How many pixels the tank moves per keypress, and how far "future" look-
// ahead collision checks look past the current cannon tip
#define PIXEL_TO_MOVE 2

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
		// a given direction, computed by player_update_future_cannon_tip()
		// right before actually moving. Reading this position (instead of
		// the current tip) means it is not the tank's own sprite pixel
		// yet, so it is safe to check directly against VGA memory (A000).
		unsigned int future_cannon_tip_x;
		unsigned int future_cannon_tip_y;

		// Absolute position (in the 320x200 screen) where the bullet
		// sprite is drawn. It always sits on the cannon tip of the
		// direction the tank is facing right now, which is exactly the
		// same pixel used for the collision check, so the bullet is
		// always visible at the mouth of the cannon. Kept up to date by
		// player_update_bullet_position().
		unsigned int bullet_position_x;
		unsigned int bullet_position_y;


		
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
	void player_update_future_cannon_tip(struct player *_player, int direction);
	void player_update_bullet_position(struct player *_player);
	
	

#endif