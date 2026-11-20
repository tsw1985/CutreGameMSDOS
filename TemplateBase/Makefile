# Makefile for Borland Turbo C 3.0 with NASM
# Same layout as the game's Makefile: one rule per object, everything lands
# in bin\, and the link goes through a response file.
#
# To add a new module:
#   1. Copy one of the bin\xxx.obj rules below and rename it.
#   2. Add the .obj to the $(TARGET) dependency list.
#   3. Add the .obj to one of the @echo lines that build bin\link.rsp.

# Turbo C configuration
MODEL = -mh
CFLAGS = -O2 $(MODEL) -Iheader
CC = tcc
LD = tcc
ASM = nasm
ASMFLAGS = -f obj
LDFLAGS = $(MODEL)
TARGET = bin\main.exe

all: $(TARGET)

################################################
#                   C FILES                    #
################################################
bin\main.obj: src\main.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\main.obj src\main.c

bin\util.obj: src\util.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\util.obj src\util.c

bin\greet.obj: src\greet.c
	@if not exist bin mkdir bin
	$(CC) -c $(CFLAGS) -obin\greet.obj src\greet.c


################################################
#                  ASM FILES                   #
################################################
bin\hello.obj: asm\hello.asm
	@if not exist bin mkdir bin
	$(ASM) $(ASMFLAGS) asm\hello.asm -o bin\hello.obj


# Link the executable
#
# DOS only passes 127 characters of arguments when launching a program, and a
# long list of .obj files stops fitting: MAKE then fails with
# "Fatal: Unable to execute command: tcc".
#
# So the list goes in a response file, bin\link.rsp, handed to tcc with @.
# Keep each @echo line short and add new lines with >> as the project grows.
$(TARGET): \
	bin\main.obj \
	bin\util.obj \
	bin\greet.obj \
	bin\hello.obj
	@echo bin\main.obj bin\util.obj > bin\link.rsp
	@echo bin\greet.obj bin\hello.obj >> bin\link.rsp
	$(LD) $(LDFLAGS) -ebin\main.exe @bin\link.rsp

clean:
	@if exist bin\*.obj del bin\*.obj
	@if exist bin\link.rsp del bin\link.rsp
	@if exist bin\*.map del bin\*.map
	@if exist $(TARGET) del $(TARGET)
