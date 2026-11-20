#ifndef HELLO
#define HELLO

//===========================================================
// Functions written in assembler (asm\hello.asm, assembled by NASM).
//
// How C and NASM meet, in the huge model (-mh):
//
//   - Turbo C puts an underscore in front of every C name, so the C
//     function asm_hello() is the label _asm_hello in the .asm file,
//     declared there with "global".
//   - Every call is a FAR call: the function returns with retf, and the
//     first argument is at [bp+6] (bp+0 old bp, bp+2 return offset,
//     bp+4 return segment).
//   - Arguments are pushed right to left and the CALLER removes them,
//     so the .asm just does retf, never "retf n".
//   - An int comes back in AX.
//   - The function must preserve BP, SI, DI and DS.
//===========================================================

// Prints a line using DOS (INT 21h, AH=09h)
void asm_hello();

// Returns a + b, to show how parameters arrive and how a value comes back
int asm_add(int a, int b);


#endif
