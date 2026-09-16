# Los diecinueve efectos

*[English version](README-EN.md)*

La intro que sale con `game.exe /demo`. Una carpeta por efecto, y cada uno
explicado paso a paso: la idea, el código, las trampas y qué romper para ver
cómo funciona.

**Léelos en este orden.** No es el orden en que se ven, es de más fácil a más
difícil: cada uno añade una técnica sobre el anterior, y para los dos últimos
hace falta todo lo que enseñan los primeros.

| | Efecto | La idea | us/frame |
|---|---|---|---|
| 1 | [**scroll** — El desplazamiento infinito](scroll/doc/README.md) | Dos `memcpy` por fila y da la vuelta sola | 7,6 |
| 2 | [**blinds** — La persiana veneciana](blinds/doc/README.md) | Tiras que se abren desde el centro, desfasadas | 7,6 |
| 3 | [**stripes** — Bandas a distinta velocidad](stripes/doc/README.md) | Parallax al reves, con las velocidades de un seno | 7,6 |
| 4 | [**melt** — La pantalla que se derrite](melt/doc/README.md) | El barrido de Doom: 320 numeros que crecen | 36 † |
| 5 | [**wobble** — La imagen que ondula](wobble/doc/README.md) | Un seno por FILA: una bandera al viento | 8,0 |
| 6 | [**water** — La foto reflejada en el agua](water/doc/README.md) | El wobble, invertido y con la amplitud creciendo | 3,1 † |
| 7 | [**bounce** — La foto que se pasea](bounce/doc/README.md) | Lissajous, y el recorte CON SIGNO | 8,1 |
| 8 | [**ripple** — La gota en el estanque](ripple/doc/README.md) | Como el wobble, pero la onda se muere al alejarse | 9,0 |
| 9 | [**mosaic** — El pixelado que respira](mosaic/doc/README.md) | Bloques, y no repintar lo que no ha cambiado | 19,6 |
| 10 | [**twister** — La columna retorcida](twister/doc/README.md) | Un 3D que no tiene 3D: todo cae a una dimensión | 41 † |
| 11 | [**zoom** — Acercarse y alejarse](zoom/doc/README.md) | La tabla de columnas: 320 cuentas y no 64.000 | 110 |
| 12 | [**lens** — La lupa que se pasea](lens/doc/README.md) | Precalcular una tabla, pero solo de 6.100 puntos | 12 † |
| 13 | [**rotozoom** — Girar y hacer zoom a la vez](rotozoom/doc/README.md) | 2 sumas por pixel, y el tramo visible por fila | 179 |
| 14 | [**carpet** — El rotozoom que no se acaba nunca](carpet/doc/README.md) | El mismo, con máscara: se repite hasta el infinito | 79 † |
| 15 | [**tunnel** — El túnel infinito](tunnel/doc/README.md) | Dos tablas de 64.000 y el frame se queda en sumas | 97 † |
| 16 | [**sphere** — La foto en una bola que gira](sphere/doc/README.md) | Lo mismo con UNA tabla: la latitud es por fila | 27 † |
| 17 | [**voxel** — Volar sobre la foto](voxel/doc/README.md) | El motor del Comanche: un rayo por columna | 117 † |
| 18 | [**cube** — El cubo de alambre](cube/doc/README.md) | 3D de verdad, y de los más baratos: no rellena | 6,5 † |
| 19 | [**texcube** — El cubo con una foto en cada cara](texcube/doc/README.md) | Lo mismo relleno, y cuesta diez veces más | 88 † |

Los costes están medidos ejecutando el código de verdad sobre `res/demo/`, en
microsegundos de CPU del anfitrión por frame. Son relativos: lo que importa es
que un `rotozoom` cuesta veinte veces un `wobble`, no el número absoluto.

† **Los diez marcados se midieron en otra máquina**, así que no se pueden
comparar de frente con los otros nueve. Dos anclas para escalarlos: en ese banco
el `rotozoom` sale **49** en vez de 179 y el `zoom` **24** en vez de 110.

**Y el coste dejó de seguir a la dificultad hace rato.** Los primeros iban de la
mano: lo difícil era lo caro. A partir del `lens` ya no, y por un motivo que es
la lección de la segunda mitad del recorrido:

- **precalcular sale gratis en tiempo y se paga en memoria.** El `sphere` hace
  trigonometría esférica por 27 microsegundos porque resolvió los arcosenos al
  entrar.
- **lo caro de un 3D no son las matemáticas, es el relleno.** El `cube` está en
  el puesto 18 y es de los más baratos de la lista: no rellena nada. El
  `texcube` hace exactamente la misma geometría y cuesta trece veces más.
- **y lo que de verdad se paga es escribir píxeles de uno en uno.** Los más
  caros — `rotozoom`, `voxel`, `tunnel` y `texcube` — son los que tocan la
  pantalla entera sin poder usar un `memcpy`.

## Hubo un vigésimo, y se quitó

`cycle` giraba la paleta sin tocar un solo píxel: el efecto más barato de todos
y el más de la época. Pero solo funciona con dibujos hechos a propósito, con los
colores en rampa. Sobre una **foto** no da un efecto, da una papilla de colores
en la que no se distingue nada, y esta es una presentación de fotos. Fuera.

## Y un epílogo, que no es un efecto

Después de la última foto aparece [**outro** — la pantalla de
despedida](outro/doc/README.md): un cartel que respira suavemente y espera a que
pulses una tecla.

No está en la tabla y no entra en el recorrido, porque **no termina sola**: los
veinte efectos reciben un `end_tick` y se paran; este espera. Metido en la tabla
colgaría la presentación en la foto que le tocara.

Está para que la intro no acabe de golpe. Antes se apagaba la última foto y el
juego arrancaba en el mismo instante; delante de gente eso es un salto. El
cartel convierte el corte en una parada.

La imagen es `res/demo/fin.bmp` y se cambia como cualquier otra — 320x200, 256
colores, 65.078 bytes — con una condición: **el texto tiene que caber en el 91%
central**, porque el cartel se acerca hasta 1,09 aumentos. Está explicado con
los números exactos en [su documento](outro/doc/README.md). Si el fichero no
está, se salta y no pasa nada.

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
   outro/           el epilogo: el cartel de despedida, fuera de la tabla
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

Son **diecinueve**. En `res/demo/` hay quince fotos, así que una pasada enseña
quince efectos y cuatro se quedan sin salir. Con diecinueve fotos salen todos y
ninguna repite; con más de diecinueve, la lista vuelve a empezar.

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
