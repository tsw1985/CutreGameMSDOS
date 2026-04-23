#ifndef PLAYERS
#define PLAYERS


#define TANK_WIDTH 17
#define TANK_HEIGHT 15

	struct player{
		
		unsigned int position_y;
		unsigned int position_x;
		unsigned int wins;
		char *sprite;
		// to do : animation
	};

	
	void player_init(struct player *_player);
	void player_free(struct player *_player);
	
	

#endif