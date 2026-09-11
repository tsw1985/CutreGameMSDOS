# Capítulo 11 — Encontrar la Sound Blaster

*[English version](README-EN.md)*

**Qué vas a conseguir:** saber si hay tarjeta de sonido y dónde está. En modo
texto, para poder leerlo.

**Qué código real se usa:** `src/sound.c`.

```
make
chap11
```

---

## 1. En DOS no hay drivers

Esto es lo primero que hay que asumir. No hay sistema de plug and play, ni
registro, ni una capa que te diga qué hardware tienes. **Tu programa habla
directamente con la tarjeta.**

Y para hablar con ella necesitas saber tres cosas: en qué **puerto** está, qué
**IRQ** usa y qué canal de **DMA**.

## 2. La variable BLASTER

El convenio que se impuso en la industria fue una variable de entorno que pone
el instalador de la tarjeta:

```
SET BLASTER=A220 I5 D1 H5 P330 T6
```

| | |
|---|---|
| `A220` | Puerto base, en hexadecimal |
| `I5` | IRQ, la interrupción que usa |
| `D1` | Canal de DMA de 8 bits |
| `H5` | Canal de DMA de 16 bits |
| `P330` | Puerto MIDI |
| `T6` | Tipo de tarjeta |

A `sound.c` le importan **A, I y D**. Si la variable no está, prueba con
A220 I5 D1, que es lo más común.

## 3. Qué es un puerto

Un puerto es una dirección, pero de **un espacio aparte del de la memoria**. Se
lee y se escribe con instrucciones distintas: `IN` y `OUT`, que en Turbo C son
`inportb()` y `outportb()`.

Es el mecanismo por el que el procesador habla con el hardware. La VGA tiene los
suyos (el `0x3DA` del retrazo, el `0x3C8`/`0x3C9` de la paleta), el teclado el
suyo (`0x60`), y la Sound Blaster los suyos.

## 4. Qué es el DSP, y cómo se detecta la tarjeta

El **DSP** es el trozo de la tarjeta que lleva el sonido digital. Se le mandan
órdenes de un byte por sus puertos.

La detección es un apretón de manos:

1. Escribir un **1** en `base + 6` (el puerto de reset)
2. Esperar un poco
3. Escribir un **0** en el mismo puerto
4. Leer del puerto de datos

Si hay una tarjeta, contesta **0xAA**. Ese byte es toda la detección: si llega,
hay tarjeta; si no llega en un plazo, no la hay.

## 5. El log: por qué `sound.c` es una librería

```c
	sound_set_log(my_log);
```

`sound.c` **no sabe escribir en ningún sitio.** No hace `printf`, no abre
ficheros, no conoce `game.log`. Lo único que hace es llamar a una función que le
pasas tú.

- El juego le pasa `tanks_log()`, que escribe en `game.log`.
- Este capítulo le pasa una que hace `printf`, porque seguimos en modo texto.

Eso es lo que convierte a `sound.c` en una **librería** en vez de "el sonido de
este juego": no impone nada sobre el programa que la usa. Podrías llevártelo a
otro proyecto tal cual.

**Llámalo antes de `sound_start()`**, o te pierdes los mensajes del arranque.

## 6. El sonido es opcional, y eso es una decisión

Si `sound_start()` devuelve 0, **el juego funciona exactamente igual, en
silencio**. Todas las llamadas de sonido comprueban si hay tarjeta y no hacen
nada si no la hay.

Nunca dejes que la falta de una tarjeta de sonido impida jugar. Parece obvio y
hay juegos de la época que se niegan a arrancar.

## 7. `sound_end()` no es opcional

```c
	sound_end();
```

Devuelve la IRQ a quien la tenía y **para el DMA**.

Si dejas el DMA corriendo al salir, la tarjeta sigue leyendo una memoria que ya
no es tuya y **oyes ruido hasta que reinicias**. Es el equivalente sonoro de no
devolver el vector del teclado.

## 8. Experimentos

1. **Quita el `sound_set_log()`.** Silencio absoluto: la tarjeta se encuentra
   igual pero no te enteras de nada.
2. **En DOSBox, pon `sbtype=none`** en `dosbox.conf` y ejecútalo. Verás el
   camino del fallo.
3. **Cambia `sbbase=220` por `sbbase=240`** en DOSBox sin tocar BLASTER.
   `sound.c` buscará donde no está.
4. **Imprime `BLASTER` en tu DOSBox.** Compara con lo que detecta.

## 9. Lo que hay que llevarse

| | |
|---|---|
| **En DOS no hay drivers** | Tu programa habla con el hardware |
| La variable `BLASTER` dice dónde está la tarjeta | A, I, D |
| Un puerto es un espacio de direcciones aparte | `IN` / `OUT` |
| La detección es un reset y esperar un `0xAA` | |
| **El log se lo pasas tú** | Por eso es una librería |
| `sound_end()` o ruido hasta reiniciar | |

---

**Anterior:** [Capítulo 10](../../ch10/doc/README.md) ·
**Siguiente:** [Capítulo 12 — Reproducir un WAV](../../ch12/doc/README.md)
