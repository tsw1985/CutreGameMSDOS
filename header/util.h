#ifndef UTIL
#define UTIL

//===========================================================
// Debug log.
//
// The game runs in graphics mode, so there is nowhere to print to: any
// printf() would paint garbage over the screen. Instead everything is
// written to tanks.log and read after quitting.
//===========================================================

void hello_util();

// Appends one line to tanks.log
void tanks_log(char *message);

// Empties tanks.log. Called once at startup, so each run starts with a
// clean file instead of mixing its lines with the previous run's.
void tanks_log_clear();


#endif
