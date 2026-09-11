@echo off
REM ===========================================================
REM Compila TODOS los capitulos del curso, uno detras de otro.
REM
REM   makeall          compila todos
REM   makeall 07       compila solo el 07
REM
REM Cada chapNN.exe se queda en la carpeta de SU capitulo, para que
REM puedas entrar, ejecutarlo y estudiarlo por separado.
REM
REM Se hace con un .bat y no dentro del Makefile porque en DOS un
REM 'cd' lanzado desde MAKE no le cambia el directorio a MAKE: se
REM ejecuta en un COMMAND.COM hijo y se pierde al volver. En un
REM fichero de proceso por lotes si persiste.
REM ===========================================================

if not "%1"=="" goto one

echo.
echo ===== CAPITULO 01 =====
cd ch01
make
cd ..

echo.
echo ===== CAPITULO 02 =====
cd ch02
make
cd ..

echo.
echo ===== CAPITULO 03 =====
cd ch03
make
cd ..

echo.
echo ===== CAPITULO 04 =====
cd ch04
make
cd ..

echo.
echo ===== CAPITULO 05 =====
cd ch05
make
cd ..

echo.
echo ===== CAPITULO 06 =====
cd ch06
make
cd ..

echo.
echo ===== CAPITULO 07 =====
cd ch07
make
cd ..

echo.
echo ===== CAPITULO 08 =====
cd ch08
make
cd ..

echo.
echo ===== CAPITULO 09 =====
cd ch09
make
cd ..

echo.
echo ===== CAPITULO 10 =====
cd ch10
make
cd ..

echo.
echo ===== CAPITULO 11 =====
cd ch11
make
cd ..

echo.
echo ===== CAPITULO 12 =====
cd ch12
make
cd ..

echo.
echo ===== CAPITULO 13 =====
cd ch13
make
cd ..

echo.
echo ===== CAPITULO 14 =====
cd ch14
make
cd ..

echo.
echo ===== CAPITULO 15 =====
cd ch15
make
cd ..

echo.
echo ===== CAPITULO 16 =====
cd ch16
make
cd ..

echo.
echo ===== CAPITULO 17 =====
cd ch17
make
cd ..

echo.
echo ===== CAPITULO 18 =====
cd ch18
make
cd ..

echo.
echo ===== CAPITULO 19 =====
cd ch19
make
cd ..

echo.
echo ===== CAPITULO 20 =====
cd ch20
make
cd ..

echo.
echo ===== CAPITULO 21 =====
cd ch21
make
cd ..

echo.
echo ===== CAPITULO 22 =====
cd ch22
make
cd ..

echo.
echo ===== CAPITULO 23 =====
cd ch23
make
cd ..

echo.
echo ===== CAPITULO 24 =====
cd ch24
make
cd ..

echo.
echo ===== TERMINADO =====
echo.
echo Cada chapNN.exe esta en su carpeta chNN.
echo Para probar uno:   cd ch07   y luego   chap07
echo.
goto end

:one
echo.
echo ===== CAPITULO %1 =====
cd ch%1
make
cd ..

:end
