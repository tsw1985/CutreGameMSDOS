# Los veinte efectos

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
| 5 | [**melt** — La pantalla que se derrite](melt/doc/README.md) | El barrido de Doom: 320 numeros que crecen | 27 † |
| 6 | [**wobble** — La imagen que ondula](wobble/doc/README.md) | Un seno por FILA: una bandera al viento | 8,0 |
| 7 | [**water** — La foto reflejada en el agua](water/doc/README.md) | El wobble, invertido y con la amplitud creciendo | 2,2 † |
| 8 | [**bounce** — La foto que se pasea](bounce/doc/README.md) | Lissajous, y el recorte CON SIGNO | 8,1 |
| 9 | [**ripple** — La gota en el estanque](ripple/doc/README.md) | Como el wobble, pero la onda se muere al alejarse | 9,0 |
| 10 | [**mosaic** — El pixelado que respira](mosaic/doc/README.md) | Bloques, y no repintar lo que no ha cambiado | 19,6 |
| 11 | [**twister** — La columna retorcida](twister/doc/README.md) | Un 3D que no tiene 3D: todo cae a una dimensión | 35 † |
| 12 | [**zoom** — Acercarse y alejarse](zoom/doc/README.md) | La tabla de columnas: 320 cuentas y no 64.000 | 110 |
| 13 | [**lens** — La lupa que se pasea](lens/doc/README.md) | Precalcular una tabla, pero solo de 6.100 puntos | 9,4 † |
| 14 | [**rotozoom** — Girar y hacer zoom a la vez](rotozoom/doc/README.md) | 2 sumas por pixel, y el tramo visible por fila | 179 |
| 15 | [**carpet** — El rotozoom que no se acaba nunca](carpet/doc/README.md) | El mismo, con máscara: se repite hasta el infinito | 73 † |
| 16 | [**tunnel** — El túnel infinito](tunnel/doc/README.md) | Dos tablas de 64.000 y el frame se queda en sumas | 77 † |
| 17 | [**sphere** — La foto en una bola que gira](sphere/doc/README.md) | Lo mismo con UNA tabla: la latitud es por fila | 22 † |
| 18 | [**voxel** — Volar sobre la foto](voxel/doc/README.md) | El motor del Comanche: un rayo por columna | 135 † |
| 19 | [**cube** — El cubo de alambre](cube/doc/README.md) | 3D de verdad, y de los más baratos: no rellena | 4,7 † |
| 20 | [**texcube** — El cubo con una foto en cada cara](texcube/doc/README.md) | Lo mismo relleno, y cuesta diez veces más | 50 † |

Los costes están medidos ejecutando el código de verdad sobre `res/demo/`, en
microsegundos de CPU del anfitrión por frame. Son relativos: lo que importa es
que un `rotozoom` cuesta veinte veces un `wobble`, no el número absoluto.

† **Los diez marcados se midieron en otra máquina**, así que no se pueden
comparar de frente con los otros diez. Dos anclas para escalarlos: en ese banco
el `rotozoom` sale **55** en vez de 179 y el `zoom` **35** en vez de 110. Ni
siquiera es un factor constante — una máquina pesa distinto la memoria y la
aritmética — así que la columna sirve para ordenar dentro de cada grupo y poco
más.

**Y el coste dejó de seguir a la dificultad hace rato.** Los diez primeros iban
de la mano: lo difícil era lo caro. A partir del `lens` ya no, y por un motivo
que es la lección de la segunda mitad del recorrido:

- **precalcular sale gratis en tiempo y se paga en memoria.** El `sphere` hace
  trigonometría esférica por 22 microsegundos porque resolvió los arcosenos al
  entrar.
- **lo caro de un 3D no son las matemáticas, es el relleno.** El `cube` está en
  el puesto 19 y es de los más baratos de la lista: no rellena nada. El
  `texcube` hace exactamente la misma geometría y cuesta diez veces más.
- **y lo que de verdad se paga es escribir píxeles de uno en uno.** Los tres más
  caros — `voxel`, `rotozoom` y `tunnel` — son los tres que tocan los 64.000
  sin poder usar un `memcpy`.

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

Añadir uno es: escribir su `.c` en su carpeta, incluir su `.h` en `demos.c`,
poner una línea en `demo_effects[]` y una regla en el `Makefile`. No hay nada
más que tocar en ningún sitio: `main.c` no se entera.

Son **veinte**. En `res/demo/` hay quince fotos, así que una pasada enseña
quince efectos y cinco se quedan sin salir. Con veinte fotos salen todos y
ninguna repite; con más de veinte, la lista vuelve a empezar.

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
