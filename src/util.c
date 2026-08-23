#include "header/util.h"
#include <stdio.h>

// Path of the log file. Both tanks_log() and tanks_log_clear() must use
// this same path, so it is defined once here instead of being repeated
// as a literal string in every function.
#define LOG_FILE_PATH "k:\\game.log"

void hello_util(){
	printf("Hello this is from util\n");
}

void tanks_log_clear(){

	FILE *log_file;

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
		printf("ERROR!!! I can not create the game.log file\n");
		return;
	}

	fclose(log_file);

}

void tanks_log(char *message){

	FILE *log_file;

	// Open the log file in append mode, so every call adds a new line
	// at the end of the file, instead of overwriting what was already there
	log_file = fopen(LOG_FILE_PATH, "a");

	if (log_file == NULL){
		printf("ERROR!!! I can not open the game.log file\n");
		return;
	}

	fprintf(log_file, "%s\n", message);

	// Close the file right after writing, so the line is flushed to disk
	// immediately. This is needed so that "tail -f tanks.log" from Linux
	// shows the new line as soon as it is written, instead of waiting for
	// the DOS file buffer to be flushed later, or lost if the program crashes.
	fclose(log_file);

}
