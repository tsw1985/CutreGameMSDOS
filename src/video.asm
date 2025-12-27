global _hola
global _set_vga_320_200_mode

segment _TEXT class=CODE
_hola:
    push bp
    push si
    push di
    push ds
    
    ; Usar el segmento de cÃ³digo para los datos
    push cs
    pop ds
    
    mov dx, Saludo
    mov ah, 9
    int 21h
    
    pop ds
    pop di
    pop si
    pop bp
retf


; SET 320x200 256 colors
_set_vga_320_200_mode:
    push bp
    push si
    push di
    push ds
    
    ; Usar el segmento de cÃ³digo para los datos
    push cs
    pop ds
    
    mov ax,0013h
    int 10h
    
    pop ds
    pop di
    pop si
    pop bp
retf
    
    
    
    
Saludo db 'Empezamos juego!!!',13,10,'$'

