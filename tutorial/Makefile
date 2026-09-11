# ===========================================================
# Makefile maestro del curso.
#
#   make            compila TODOS los capitulos
#   make clean      borra todos los .exe y .obj
#
# Cada chapNN.exe se queda en la carpeta de su capitulo, no aqui, para que
# puedas entrar, ejecutarlo y estudiarlo por separado:
#
#   cd ch07
#   chap07
#
#
# POR QUE "command /c" Y NO "makeall.bat" A SECAS
#
# Un fichero .bat NO es un programa ejecutable: es un guion que interpreta
# COMMAND.COM. MAKE intenta EJECUTAR lo que le pones, y al no ser un .exe ni
# un .com contesta "comando o nombre de archivo incorrecto".
#
# Asi que hay que llamar al interprete y pasarle el guion:
#
#   command /c makeall.bat
#
# Tambien puedes saltarte MAKE y lanzarlo directamente desde DOS:
#
#   makeall           compila todos
#   makeall 07        compila solo el 07
#   cleanall          limpia todos
#
#
# POR QUE UN .BAT Y NO HACERLO AQUI DENTRO
#
# Porque hay que entrar en cada carpeta, y un "cd" lanzado desde MAKE se
# ejecuta en un COMMAND.COM hijo que muere al terminar la linea: MAKE se
# queda donde estaba y el "make" siguiente compilaria lo que no es. En un
# fichero de proceso por lotes el cd si persiste, porque es el mismo
# interprete el que va leyendo las lineas.
# ===========================================================

all:
	command /c makeall.bat

clean:
	command /c cleanall.bat
