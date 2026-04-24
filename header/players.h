#ifndef PLAYERS
#define PLAYERS


#define TANK_WIDTH 18
#define TANK_HEIGHT 18

	struct player{
		
		unsigned int position_y;
		unsigned int position_x;
		unsigned int wins;
		char *sprite_tank_up;
		char *sprite_tank_down;
		char *sprite_tank_left;
		char *sprite_tank_right;
		// to do : animation
	};

	
	void player_init(struct player *_player);
	void player_free(struct player *_player);
	
	

#endif