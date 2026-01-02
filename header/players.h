#ifndef PLAYERS
#define PLAYERS


	struct player{
		
		unsigned int position_y;
		unsigned int position_x;
		unsigned int wins;
		char *image_data;
		// to do : animation
	};

	
	void init_player(struct player *_player);
	
	

#endif