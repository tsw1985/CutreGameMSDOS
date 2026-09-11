# ===========================================================
# CAPITULO 18
#
# Se compila con:   make
# Se ejecuta con:   chap18
# Se limpia con:    make clean
#
# El -I..\..  le dice a Turbo C que busque los #include empezando en la raiz
# del proyecto, que es donde esta header\. Asi este capitulo escribe
# #include "header\bmp.h" exactamente igual que lo escribe el juego.
#
# Y fijate en los fuentes: son los FICHEROS REALES del juego, en ..\..\src\.
# No hay copias de nada. Si manana arreglas un fallo ahi, este capitulo se
# arregla solo.
#
# OJO con la linea de compilacion: DOS solo pasa 127 caracteres de argumentos
# a un programa. Con tantos ficheros ya no caben, asi que la lista se mete en
# un fichero de respuesta (chap18.rsp) y se le pasa a tcc con una @. Es
# exactamente lo mismo que hace el Makefile del juego, y por la misma razon.
# ===========================================================

CC = tcc
MODEL = -mh
CFLAGS = -O2 $(MODEL) -I..\..

TARGET = chap18.exe

all: $(TARGET)

$(TARGET): chap18.c ..\..\src\bmp.c ..\..\src\players.c ..\..\src\util.c ..\..\src\net.c ..\..\src\lockstep.c
	@echo chap18.c ..\..\src\bmp.c ..\..\src\players.c ..\..\src\util.c ..\..\src\net.c ..\..\src\lockstep.c > chap18.rsp
	$(CC) $(CFLAGS) -echap18.exe @chap18.rsp

clean:
	@if exist *.obj del *.obj
	@if exist *.map del *.map
	@if exist *.rsp del *.rsp
	@if exist $(TARGET) del $(TARGET)
