# ===========================================================
# CAPITULO 15
#
# Se compila con:   make
# Se ejecuta con:   chap15
# Se limpia con:    make clean
#
# El -I..\..  le dice a Turbo C que busque los #include empezando en la raiz
# del proyecto, que es donde esta header\. Asi este capitulo escribe
# #include "header\bmp.h" exactamente igual que lo escribe el juego.
#
# Y fijate en los fuentes: son los FICHEROS REALES del juego, en ..\..\src\.
# No hay copias de nada. Si manana arreglas un fallo ahi, este capitulo se
# arregla solo.
# ===========================================================

CC = tcc
MODEL = -mh
CFLAGS = -O2 $(MODEL) -I..\..

TARGET = chap15.exe

all: $(TARGET)

$(TARGET): chap15.c ..\..\src\net.c
	$(CC) $(CFLAGS) -echap15.exe chap15.c ..\..\src\net.c

clean:
	@if exist *.obj del *.obj
	@if exist *.map del *.map
	@if exist *.rsp del *.rsp
	@if exist $(TARGET) del $(TARGET)
