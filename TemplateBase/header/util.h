#ifndef UTIL
#define UTIL

//===========================================================
// Debug log.
//
// Once a program switches to graphics mode there is nowhere to print to:
// any printf() would paint garbage over the screen. So everything is
// written to app.log, in the directory the program is run from, and read
// after quitting.
//===========================================================

// Prints a line from this module, to prove it is linked in
void hello_util();

// Appends one line to app.log
void app_log(char *message);

// Empties app.log. Call it once at startup, so each run starts with a
// clean file instead of mixing its lines with the previous run's.
void app_log_clear();


#endif
