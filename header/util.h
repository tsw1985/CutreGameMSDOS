#ifndef UTIL
#define UTIL

//===========================================================
// Debug log.
//
// The game runs in graphics mode, so there is nowhere to print to: any
// printf() would paint garbage over the screen. Instead everything is
// written to game.log, in the directory the game is run from, and read
// after quitting. From the host side it can be followed live with
// "tail -f bin/game.log".
//===========================================================

void hello_util();

// Appends one line to game.log
void tanks_log(char *message);

// Empties game.log. Called once at startup, so each run starts with a
// clean file instead of mixing its lines with the previous run's.
void tanks_log_clear();


#endif
