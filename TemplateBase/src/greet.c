#include "header\greet.h"
#include <stdio.h>

void hello_greet(){
	printf("Hello from greet.c\n");
}

void greet_person(char *name, int times){

	int i;

	for (i = 0; i < times; i++){
		printf("  %d: Hello, %s!\n", i + 1, name);
	}
}
