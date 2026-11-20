; Assembler half of the template. See header\hello.h for the rules that
; let Turbo C call these functions.

global _asm_hello
global _asm_add

segment _TEXT class=CODE

; void asm_hello();
_asm_hello:
    push bp
    push si
    push di
    push ds

    ; The string lives in this code segment, so point DS at it
    push cs
    pop ds

    mov dx, message
    mov ah, 9
    int 21h

    pop ds
    pop di
    pop si
    pop bp
retf


; int asm_add(int a, int b);
_asm_add:
    push bp
    mov bp, sp

    mov ax, [bp+6]      ; a
    add ax, [bp+8]      ; b, the result stays in AX

    pop bp
retf


message db 'Hello from hello.asm',13,10,'$'
