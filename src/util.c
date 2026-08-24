#include "header/util.h"
#include <stdio.h>

// Path of the log file. Both tanks_log() and tanks_log_clear() must use
// this same path, so it is defined once here instead of being repeated
// as a literal string in every function.
#define LOG_FILE_PATH "k:\\game.log"

// The "k:" drive only exists on the development machine. When the game
// runs anywhere else, that drive will not exist and fopen() will fail.
//
// This flag remembers that failure so we do not keep trying to open a
// drive that we already know is missing. Without this flag, tanks_log()
// would call fopen() again on every single throttled call (about once
// per second), which is both wasted time and, since the game is running
// in VGA graphics mode, a place we do not want to risk any unexpected
// DOS/BIOS behaviour from repeatedly touching a missing drive.
//
// It starts at 1 (logging enabled) and is set to 0 the first time we
// detect that the log file cannot be opened.
static int log_is_available = 1;

void hello_util(){
	printf("Hello this is from util\n");
}

void tanks_log_clear(){

	FILE *log_file;

	if (log_is_available == 0){
		return;
	}

	// Opening the file in "w" mode creates it if it does not exist yet,
	// and truncates it to zero bytes if it already exists. This gives a
	// fresh, empty log file at the start of every run, instead of mixing
	// lines from previous runs together.
	//
	// We truncate the file instead of deleting and recreating it, because
	// deleting it would break "tail -f" on the Linux side: tail -f keeps
	// following the old file handle, so it would stop showing new lines
	// after the file is deleted and a new one is created in its place.
	log_file = fopen(LOG_FILE_PATH, "w");

	if (log_file == NULL){
		// The "k:" drive (or the game.log path on it) is not available.
		// Do not print anything here: the game is running in VGA graphics
		// mode, and calling printf() while in graphics mode can corrupt
		// what is on screen. We just disable logging for the rest of the
		// run so the game keeps going normally without it.
		log_is_available = 0;
		return;
	}

	fclose(log_file);

}

void tanks_log(char *message){

	FILE *log_file;

	if (log_is_available == 0){
		return;
	}

	// Open the log file in append mode, so every call adds a new line
	// at the end of the file, instead of overwriting what was already there
	log_file = fopen(LOG_FILE_PATH, "a");

	if (log_file == NULL){
		// Same reasoning as in tanks_log_clear(): stop trying to use the
		// log for the rest of the run instead of risking a printf() call
		// while the game is in VGA graphics mode.
		log_is_available = 0;
		return;
	}

	fprintf(log_file, "%s\n", message);

	// Close the file right after writing, so the line is flushed to disk
	// immediately. This is needed so that "tail -f tanks.log" from Linux
	// shows the new line as soon as it is written, instead of waiting for
	// the DOS file buffer to be flushed later, or lost if the program crashes.
	fclose(log_file);

}
