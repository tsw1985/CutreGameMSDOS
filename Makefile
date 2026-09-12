# Makefile for Borland Turbo C 3.0 with NASM
# Based on the pattern of zlib's Makefile.bor

# Turbo C configuration
MODEL = -mh
CFLAGS = -O2 $(MODEL) -Iheader
CC = tcc
LD = tcc
ASM = nasm
ASMFLAGS = -f obj
LDFLAGS = $(MODEL)
TARGET = bin\game.exe

all: $(TARGET)

# Compile the object files
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


bin\lockstep.obj: src\lockstep.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\lockstep.obj src\lockstep.c

	
################################################
#                                 DEMOS                                                 #
#
# La intro. Cada efecto en su carpeta, y todos compilan igual.
#
# Fijate en que el -Idemos NO hace falta: los ficheros de demos\ se incluyen
# entre ellos como "demos\zoom\zoom.h", o sea desde la raiz del proyecto,
# que es justo desde donde MAKE ejecuta a tcc.
################################################
bin\demos.obj: demos\demos.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\demos.obj demos\demos.c

bin\rotozoom.obj: demos\rotozoom\rotozoom.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\rotozoom.obj demos\rotozoom\rotozoom.c

bin\wobble.obj: demos\wobble\wobble.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\wobble.obj demos\wobble\wobble.c

bin\zoom.obj: demos\zoom\zoom.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\zoom.obj demos\zoom\zoom.c

bin\scroll.obj: demos\scroll\scroll.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\scroll.obj demos\scroll\scroll.c

bin\mosaic.obj: demos\mosaic\mosaic.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\mosaic.obj demos\mosaic\mosaic.c

bin\blinds.obj: demos\blinds\blinds.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\blinds.obj demos\blinds\blinds.c

bin\ripple.obj: demos\ripple\ripple.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\ripple.obj demos\ripple\ripple.c

bin\bounce.obj: demos\bounce\bounce.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\bounce.obj demos\bounce\bounce.c

bin\cycle.obj: demos\cycle\cycle.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\cycle.obj demos\cycle\cycle.c

bin\stripes.obj: demos\stripes\stripes.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\stripes.obj demos\stripes\stripes.c


################################################	
#                                 ASM FILES                                             # 	
################################################
bin\video.obj: src\video.asm
	@if not exist bin mkdir bin
	$(ASM) $(ASMFLAGS) src\video.asm -o bin\video.obj


# Link the executable
#
# DOS only passes 127 characters of arguments when launching a program, and the
# list of .obj files no longer fits: adding sound.obj took the line from 110 to
# 127 characters and MAKE could no longer run tcc, failing with
# "Fatal: Unable to execute command: tcc".
#
# So the list goes in a response file, bin\link.rsp, handed to tcc with @. The
# line drops to 36 characters and it no longer matters how many .obj files get
# added later on.
$(TARGET): \
	bin\main.obj \
	bin\util.obj \
	bin\video.obj \
	bin\bmp.obj \
	bin\players.obj \
	bin\gameloop.obj \
	bin\sound.obj \
	bin\net.obj \
	bin\lockstep.obj \
	bin\demos.obj \
	bin\rotozoom.obj \
	bin\wobble.obj \
	bin\zoom.obj \
	bin\scroll.obj \
	bin\mosaic.obj \
	bin\blinds.obj \
	bin\ripple.obj \
	bin\bounce.obj \
	bin\cycle.obj \
	bin\stripes.obj
	@echo bin\main.obj bin\util.obj bin\video.obj > bin\link.rsp
	@echo bin\bmp.obj bin\players.obj bin\gameloop.obj >> bin\link.rsp
	@echo bin\sound.obj bin\net.obj bin\lockstep.obj >> bin\link.rsp
	@echo bin\demos.obj bin\rotozoom.obj bin\wobble.obj >> bin\link.rsp
	@echo bin\zoom.obj bin\scroll.obj bin\mosaic.obj >> bin\link.rsp
	@echo bin\blinds.obj bin\ripple.obj bin\bounce.obj >> bin\link.rsp
	@echo bin\cycle.obj bin\stripes.obj >> bin\link.rsp
	$(LD) $(LDFLAGS) -ebin\game.exe @bin\link.rsp

clean:
	@if exist bin\*.obj del bin\*.obj
	@if exist bin\link.rsp del bin\link.rsp
	@if exist bin\*.map del bin\*.map
	@if exist $(TARGET) del $(TARGET)