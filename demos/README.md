# Los diez efectos

*[English version](README-EN.md)*

La intro que sale con `game.exe /demo`. Una carpeta por efecto, y cada uno
explicado paso a paso: la idea, el código, las trampas y qué romper para ver
cómo funciona.

**Léelos en este orden.** No es el orden en que se ven, es de más fácil a más
difícil: cada uno añade una técnica sobre el anterior, y para los dos últimos
hace falta todo lo que enseñan los primeros.

| | Efecto | La idea | us/frame |
|---|---|---|---|
| 1 | [**cycle** — La paleta girando](cycle/doc/README.md) | La pantalla no se toca: se gira la PALETA | 1,6 |
| 2 | [**scroll** — El desplazamiento infinito](scroll/doc/README.md) | Dos `memcpy` por fila y da la vuelta sola | 7,6 |
| 3 | [**blinds** — La persiana veneciana](blinds/doc/README.md) | Tiras que se abren desde el centro, desfasadas | 7,6 |
| 4 | [**stripes** — Bandas a distinta velocidad](stripes/doc/README.md) | Parallax al reves, con las velocidades de un seno | 7,6 |
| 5 | [**wobble** — La imagen que ondula](wobble/doc/README.md) | Un seno por FILA: una bandera al viento | 8,0 |
| 6 | [**bounce** — La foto que se pasea](bounce/doc/README.md) | Lissajous, y el recorte CON SIGNO | 8,1 |
| 7 | [**ripple** — La gota en el estanque](ripple/doc/README.md) | Como el wobble, pero la onda se muere al alejarse | 9,0 |
| 8 | [**mosaic** — El pixelado que respira](mosaic/doc/README.md) | Bloques, y no repintar lo que no ha cambiado | 19,6 |
| 9 | [**zoom** — Acercarse y alejarse](zoom/doc/README.md) | La tabla de columnas: 320 cuentas y no 64.000 | 110 |
| 10 | [**rotozoom** — Girar y hacer zoom a la vez](rotozoom/doc/README.md) | 2 sumas por pixel, y el tramo visible por fila | 179 |

Los costes están medidos ejecutando el código de verdad sobre `res/demo/`, en
microsegundos de CPU del anfitrión por frame. Son relativos: lo que importa es
que un `rotozoom` cuesta veinte veces un `wobble`, no el número absoluto.

## Cómo está montada la carpeta

```
demos/
   demos.h          toda la API publica: demo_run()
   demos.c          el reproductor: memoria, tiempos, ESC, fundidos, la tabla
   demolib.h        lo que comparten todos: tabla de senos, retrazo, demo_show()
   <efecto>/
      <efecto>.c    el efecto
      <efecto>.h    su unica declaracion
      doc/          esta explicacion, en los dos idiomas
```

Todos los efectos tienen **la misma forma**, y eso es lo que permite que
`demos.c` los tenga en un array de punteros a función:

```c
int demo_<nombre>(unsigned char *image,
                  unsigned char *screen,
                  unsigned char *palette,
                  unsigned long end_tick);
```

Añadir uno es: escribir su `.c` en su carpeta, incluir su `.h` en `demos.c` y
poner una línea en `demo_effects[]`. No hay nada más que tocar en ningún sitio.

## Las reglas que cumplen todos

- **Nada de coma flotante.** Ni un `float` en la carpeta. Punto fijo 8.8 y una
  tabla de senos de 256 entradas, porque una vuelta de 256 pasos da la vuelta
  con `& 255` en vez de con una división.
- **`demo_show()` una vez por frame.** Retrazo, volcado y alimentar la tarjeta
  de sonido. El sonido está ahí dentro para que ningún efecto pueda olvidarlo
  — uno ya lo olvidó, y la música se quedó en bucle ocho segundos.
- **Ni un header del juego.** La carpeta toma prestadas siete funciones de
  `bmp.c` y cuatro de `sound.c`, declaradas a mano en `demolib.h`. No sabe que
  existen los tanques.
- **Sin `/demo` no existe nada.** Ni un byte de memoria, ni un fichero abierto.
- **Un callback por frame, y nada más.** `demo_set_idle()` recibe un puntero a
  función que `demo_sound()` llama una vez por frame; si devuelve 1, la intro
  se corta como si hubieras pulsado ESC. El juego enchufa ahí el latido de la
  red, y la carpeta sigue sin saber que existe una red.

## Cómo se ejecutan

```bash
./play.sh local -demo
game.exe /demo
```

Los requisitos de las imágenes y los ajustes de DOSBox, en
[`../COMANDOS.md`](../COMANDOS.md).
