global _hola
global _set_vga_320_200_mode
global _reset_pic

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


_reset_pic:
    push bp
    push si
    push di
    push ds
    
    ;code here
    in  al, 0x61
    or  al, 0x82
    out 0x61, al
    and al, 0x7F
    out 0x61, al
    mov al, 0x20
    out 0x20, al
    
    ;end code here
    
    pop ds
    pop di
    pop si
    pop bp
retf
    
    
    
    
Saludo db 'Empezamos juego!!!',13,10,'$'

