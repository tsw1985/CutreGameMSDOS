#include <stdio.h>
#include <conio.h>
#include "header\util.h"
#include "header\greet.h"
#include "header\hello.h"

// Hello world for the template: one call into each module, C and ASM, so
// a broken Makefile or a missing .obj shows up straight away. Replace the
// body with the real program.
int main(){

	int sum;

	app_log_clear();
	app_log("Start");

	printf("Hello world from main.c\n\n");

	hello_util();
	hello_greet();
	greet_person("DOS", 3);

	asm_hello();

	sum = asm_add(2, 3);
	printf("asm_add(2, 3) = %d\n", sum);

	app_log("End");

	printf("\nPress any key...\n");
	getch();

	return 0;
}
