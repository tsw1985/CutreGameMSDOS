# TemplateBase

Plantilla vacía para proyectos de MS-DOS con **Borland Turbo C++ 3.0** (`tcc`,
modelo huge `-mh`) y **NASM**. Misma estructura y mismo Makefile que el juego.

```
src\      los .c
header\   los .h  (se incluyen como "header\xxx.h")
asm\      los .asm de NASM
res\      recursos (BMP, WAV...)
bin\      salida: los .obj, link.rsp y main.exe (lo crea el Makefile)
```

Compilar: `make` desde esta carpeta. Ejecutar: `bin\main.exe`. Limpiar: `make clean`.

Añadir un módulo: crear `src\nuevo.c` y `header\nuevo.h`, copiar una regla
`bin\xxx.obj` en el Makefile y añadir el `.obj` a la lista de `$(TARGET)` y a
una de las líneas `@echo` de `bin\link.rsp`. Cómo se llama a ASM desde C está
explicado en `header\hello.h`.

---

Empty template for MS-DOS projects with **Borland Turbo C++ 3.0** and **NASM**.
Build with `make`, run `bin\main.exe`. To add a module, create the `.c` / `.h`,
copy a `bin\xxx.obj` rule in the Makefile and add the `.obj` to the `$(TARGET)`
list and to one of the `bin\link.rsp` `@echo` lines. The C/ASM calling rules
are in `header\hello.h`.
