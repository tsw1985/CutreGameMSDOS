#include "header\util.h"
#include <stdio.h>

// No drive letter and no directory: the file is created wherever the
// program is run from. A hardcoded drive that only exists on the
// development machine gives a silently empty log anywhere else.
#define LOG_FILE_PATH "app.log"

// Remembers that the log could not be opened (read only medium, full disk,
// no free DOS file handles), so we stop trying instead of calling fopen()
// again on every single log line.
static int log_is_available = 1;

void hello_util(){
	printf("Hello from util.c\n");
}

void app_log_clear(){

	FILE *log_file;

	if (log_is_available == 0){
		return;
	}

	log_file = fopen(LOG_FILE_PATH, "w");
	if (log_file == NULL){
		log_is_available = 0;
		return;
	}

	fclose(log_file);
}

void app_log(char *message){

	FILE *log_file;

	if (log_is_available == 0){
		return;
	}

	log_file = fopen(LOG_FILE_PATH, "a");
	if (log_file == NULL){
		log_is_available = 0;
		return;
	}

	fprintf(log_file, "%s\n", message);
	fclose(log_file);
}
