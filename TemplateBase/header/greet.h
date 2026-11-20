#ifndef GREET
#define GREET

//===========================================================
// Example module.
//
// Nothing useful by itself: it is here to show the shape of a module
// (a .c in src\, its .h in header\, one rule in the Makefile) so the next
// one can be made by copying it.
//===========================================================

// Prints a line from this module, to prove it is linked in
void hello_greet();

// Prints "Hello, <name>!" the given number of times
void greet_person(char *name, int times);


#endif
