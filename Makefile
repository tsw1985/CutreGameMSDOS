# Makefile para Borland Turbo C 3.0 con NASM
# Basado en el patrón de Makefile.bor de zlib

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
	
		
bin\bmp.obj: src\bmp.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\bmp.obj src\bmp.c		
	

bin\players.obj: src\players.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\players.obj src\players.c

	
bin\gameloop.obj: src\gameloop.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\gameloop.obj src\gameloop.c


bin\sound.obj: src\sound.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\sound.obj src\sound.c


bin\net.obj: src\net.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\net.obj src\net.c

	
################################################	
#                                 ASM FILES                                             # 	
################################################
bin\video.obj: src\video.asm
	@if not exist bin mkdir bin
	$(ASM) $(ASMFLAGS) src\video.asm -o bin\video.obj


# Enlazar ejecutable
#
# DOS solo admite 127 caracteres de argumentos al lanzar un programa, y la
# lista de .obj ya no cabe: al anadir sound.obj la linea paso de 110 a 127
# caracteres y MAKE dejo de poder ejecutar tcc, con el error
# "Fatal: Unable to execute command: tcc".
#
# Por eso la lista va en un fichero de respuesta, bin\link.rsp, y a tcc se le
# pasa con @. La linea baja a 36 caracteres y da igual cuantos .obj se
# anadan mas adelante.
$(TARGET): \
	bin\main.obj \
	bin\util.obj \
	bin\video.obj \
	bin\bmp.obj \
	bin\players.obj \
	bin\gameloop.obj \
	bin\sound.obj \
	bin\net.obj
	@echo bin\main.obj bin\util.obj bin\video.obj > bin\link.rsp
	@echo bin\bmp.obj bin\players.obj bin\gameloop.obj >> bin\link.rsp
	@echo bin\sound.obj bin\net.obj >> bin\link.rsp
	$(LD) $(LDFLAGS) -ebin\game.exe @bin\link.rsp

clean:
	@if exist bin\*.obj del bin\*.obj
	@if exist bin\link.rsp del bin\link.rsp
	@if exist bin\*.map del bin\*.map
	@if exist $(TARGET) del $(TARGET)