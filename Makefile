# Makefile para Borland Turbo C 3.0 con NASM
# Basado en el patrÃ³n de Makefile.bor de zlib

# Configuracion para Turbo C
MODEL = -mh
CFLAGS = -O2 $(MODEL) -Iheader
CC = tcc
LD = tcc
ASM = nasm
ASMFLAGS = -f obj
LDFLAGS = $(MODEL)
TARGET = bin\game.exe

all: $(TARGET)

# Compilar archivos objeto
bin\main.obj: src\main.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\main.obj src\main.c
	

bin\util.obj: src\util.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\util.obj src\util.c	
	
		
# BMP HANLDER	
bin\bmp.obj: src\bmp.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\bmp.obj src\bmp.c		

	
################################################	
#                                 ASM FILES                                             # 	
################################################
bin\video.obj: src\video.asm
	@if not exist bin mkdir bin
	$(ASM) $(ASMFLAGS) src\video.asm -o bin\video.obj


# Enlazar ejecutable
$(TARGET): \
	bin\main.obj \
	bin\util.obj \
	bin\video.obj \
	bin\bmp.obj \
	
	
	$(LD) $(LDFLAGS) -ebin\game.exe \
		bin\main.obj \
		bin\util.obj \
		bin\video.obj \
		bin\bmp.obj \
		

clean:
	@if exist bin\*.obj del bin\*.obj
	@if exist bin\*.map del bin\*.map
	@if exist $(TARGET) del $(TARGET)