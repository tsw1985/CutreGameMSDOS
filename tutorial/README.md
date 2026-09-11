# Curso: de una pantalla en negro a dos tanques peleando en red

Un capítulo por idea. Cada uno es un programa que compila, se ejecuta y hace
una sola cosa, y cada uno añade **una** pieza al anterior.

---

## Cómo funciona

Cada capítulo es una carpeta:

```
tutorial/
   ch01/
      chap01.c          <- la leccion. Corto, todo comentado
      Makefile          <- make, y sale chap01.exe aqui mismo
      doc/
         README.md      <- la explicacion completa
   ch02/
      ...
```

Desde DOS, capítulo a capítulo:

```
cd tutorial\ch01
make
chap01
```

O **todos de una vez**, desde `tutorial\`:

```
make              compila los 22
makeall 07        compila solo el 07
make clean        borra todos los .exe y .obj
```

Cada `chapNN.exe` se queda **en la carpeta de su capítulo**, no en un `bin`
común, para que puedas entrar, ejecutarlo y estudiarlo por separado.

## La regla del curso: **no hay código duplicado**

Esto no es un juego de juguete escrito aparte. Cada capítulo **enlaza contra
los ficheros reales** de `src/` e incluye los headers reales de `header/`:

```make
$(CC) $(CFLAGS) -echap02.exe chap02.c ..\..\src\bmp.c
```

Lo que hay en `chapNN.c` es **solo la lección**: el `main()` que llama a las
funciones del juego en el orden que toca. Todo lo demás es el código de verdad.

Eso tiene tres consecuencias buenas:

- **El curso no envejece.** Si arreglas un fallo en `bmp.c`, todos los
  capítulos se arreglan solos.
- **Estás leyendo el código que se ejecuta de verdad**, no una versión
  didáctica simplificada que luego no se parece a nada.
- **El `diff` entre capítulos es la lección.** Prueba esto:

  ```
  diff tutorial/ch04/chap04.c tutorial/ch05/chap05.c
  ```

  Eso te dice exactamente qué hace falta añadir para pasar de "un sprite
  quieto" a "un sprite que se mueve con el teclado". Sin ruido.

**La única excepción**, y está señalada donde aparece: `set_text_mode()` y
`wait_retrace()` viven dentro de `src/main.c`, que tiene su propio `main()`, así
que no se pueden enlazar. Son ocho líneas en total. Su sitio natural sería
`src/bmp.c`; si algún día se mudan ahí, desaparecen de los capítulos.

## Los recursos

Los capítulos usan los BMP y los WAV del juego, sin copiarlos:

```c
bmp_fill_background_in_main_buffer("..\\..\\res\\cutre.bmp");
```

Desde `tutorial\ch01\`, ese `..\..\res\` es la carpeta `res\` del proyecto.

---

## El temario

### Bloque 1 — Gráficos

| | Capítulo | La idea |
|---|---|---|
| ✅ | [**ch01** — El modo 13h y la memoria de vídeo](ch01/doc/README.md) | La pantalla es memoria. El byte es un índice, no un color |
| ✅ | [**ch02** — Cargar un BMP](ch02/doc/README.md) | Píxeles y paleta son dos cargas. Los BMP van del revés |
| ✅ | [**ch03** — Doble buffer y retrazo](ch03/doc/README.md) | Por qué parpadea, por qué se desgarra, y cómo se arregla |
| ✅ | [**ch04** — Sprites](ch04/doc/README.md) | Recortar, transparencia, y el recorte que evita corromper memoria |
| ✅ | [**ch05** — El teclado](ch05/doc/README.md) | Scancodes, INT 9, y varias teclas a la vez |

### Bloque 2 — El juego

| | Capítulo | La idea |
|---|---|---|
| ✅ | [**ch06** — Animación](ch06/doc/README.md) | Dos fotogramas de oruga y un contador de velocidad |
| ✅ | [**ch07** — Colisiones contra el mapa](ch07/doc/README.md) | El BMP de colisión, y por qué el dibujo miente |
| ✅ | [**ch08** — Dos jugadores](ch08/doc/README.md) | Separar "las teclas" de "de dónde vienen las teclas" |
| ✅ | [**ch09** — Balas](ch09/doc/README.md) | Disparar, volar, chocar. El detector de flanco |
| ✅ | [**ch10** — Impacto, explosión y ronda](ch10/doc/README.md) | La pausa, el marcador, el reinicio |

### Bloque 3 — Sonido

| | Capítulo | La idea |
|---|---|---|
| ✅ | [**ch11** — Encontrar la Sound Blaster](ch11/doc/README.md) | La variable BLASTER, el DSP, el reset |
| ✅ | [**ch12** — Un WAV por DMA](ch12/doc/README.md) | La tarjeta lee la memoria sola. `sound_update()` |
| ✅ | [**ch13** — Mezclar](ch13/doc/README.md) | Varios sonidos a la vez, el volumen y el recorte |
| ✅ | [**ch14** — Música](ch14/doc/README.md) | Streaming: 2,7 MB en una máquina de 640 KB |

### Bloque 4 — Red

| | Capítulo | La idea |
|---|---|---|
| ✅ | [**ch15** — Encontrar IPX](ch15/doc/README.md) | El INT 2F, el far call, abrir un socket |
| ✅ | [**ch16** — Emparejar dos máquinas](ch16/doc/README.md) | Broadcast, HELLO y ACK. Nadie escribe ninguna dirección |
| ✅ | [**ch17** — Enviar y recibir](ch17/doc/README.md) | Un chat completo. Datagramas, no flujo |
| ✅ | [**ch18** — Lockstep](ch18/doc/README.md) | Por qué no se mandan posiciones. Determinismo y retardo |
| ✅ | [**ch19** — Desincronización](ch19/doc/README.md) | Checksums. Pulsa D y provócala |

### Bloque 5 — El mapa grande

| | Capítulo | La idea |
|---|---|---|
| ✅ | [**ch20** — El mundo deja de ser la pantalla](ch20/doc/README.md) | El problema, ANTES que la solución |
| ✅ | [**ch21** — La cámara](ch21/doc/README.md) | La ventana, la zona muerta, el clamp |
| ✅ | [**ch22** — Máscara de 1 bit y la memoria](ch22/doc/README.md) | Fragmentación: la lección cara |

---

## Si ya sabes algo

- ¿Solo te interesa la **cámara**? Los capítulos 1, 4, 20, 21 y 22. Y el
  [manual completo](../doc/ES/MANUAL-CAMARA.md).
- ¿Solo la **red**? Del 15 al 19, más el [manual de red](../doc/ES/MANUAL-RED.md).
- ¿Solo el **sonido**? Del 11 al 14, más el [tutorial de sonido](../doc/ES/TUTORIAL-SONIDO.md).

Los manuales de `doc/` son la referencia profunda. Este curso es el camino de
subida.

## Requisitos

- Borland Turbo C++ 3.0
- DOSBox, o una máquina DOS de verdad
- Para los capítulos 11 a 14, una Sound Blaster (o la que emula DOSBox)
- Para los capítulos 15 a 19, dos instancias: usa `launch_game_both.sh`

Ningún capítulo necesita NASM.
