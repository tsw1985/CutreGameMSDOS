# Capítulo 14 — Música: reproducir desde el disco

*[English version](README-EN.md)*

**Qué vas a conseguir:** 2,7 MB de música sonando en una máquina de 640 KB.

```
make
chap14
```

`1` disparo encima · `+`/`-` volumen · `P` parar

---

## 1. La cuenta que lo cambia todo

| | |
|---|---|
| `res/prody8.wav` | **2.719.788 bytes** |
| Memoria de una máquina DOS | 640 KB |

**La canción es más de cuatro veces el total de la memoria de la máquina**,
sistema operativo incluido.

`load_sound()` no puede con eso, y no por un fallo de programación: es
físicamente imposible.

## 2. Streaming: no la cargues, léela a trozos

La solución es no tenerla entera nunca:

1. Lee un trozo del fichero
2. Reprodúcelo
3. Lee el siguiente
4. Cuando se acaba el fichero, vuelve al principio

Eso es el **streaming**. El trozo son **16.384 bytes**: 0,37 segundos a 44100 Hz.

```c
static unsigned char song_buffer[SONG_BUFFER_SIZE];
```

Un buffer fijo, reservado una vez. Y esto es lo bonito:

| | Memoria que cuesta |
|---|---|
| Un efecto | El tamaño del WAV |
| **Una canción** | **16 KB, dure lo que dure** |

Una canción de una hora costaría los mismos 16 KB. Solo cambia cuántas veces
hay que ir al disco.

## 3. El requisito que pilla a todo el mundo

> **La canción tiene que estar EXACTAMENTE a 44100 Hz, mono, 8 bits.**

Los efectos no: `load_sound()` acepta cualquier frecuencia y **convierte** al
cargar. Lo hace **una vez, al arrancar**, y le da igual tardar medio segundo.

`play_song()` no puede hacer eso. Va leyendo el fichero **mientras suena**, y no
hay ningún sitio donde convertir sobre la marcha: no hay tiempo (estás en mitad
de una partida) y no hay memoria (ese es todo el problema).

Si la canción no está a 44100, `play_song()` lo dice en el log y no suena.

## 4. La música y los efectos comparten mezclador

El mismo `sound_update()` del capítulo 12:

- Mezcla los efectos que estén sonando
- **Y además** rellena el buffer de la canción leyendo más fichero cuando hace
  falta

Por eso los efectos se oyen **por encima** de la música sin cortarla: para el
mezclador la canción es una voz más.

## 5. Por qué el disco no da problemas

Leer 16 KB del disco no es instantáneo, pero tienes **0,37 segundos de margen**
antes de que el buffer se agote. En ese tiempo cabe cualquier lectura razonable.

Si tu bucle se atasca mucho más de eso, oirás el corte. Es el mismo síntoma que
el `U` del capítulo 12.

## 6. Experimentos

1. **Convierte la canción a 22050 Hz** y cárgala. `play_song()` lo rechaza y lo
   dice en el log.
2. **Baja `SONG_BUFFER_SIZE`** en `sound.c` a 4096 y recompila. Menos margen:
   con un frame lento, corta.
3. **Sube el volumen de la música al máximo** y dispara. Recorte, como en el
   capítulo 13.
4. **Quita el `sound_update()`.** La música se corta a los 0,37 segundos.

## 7. Lo que hay que llevarse

| | |
|---|---|
| Una canción no cabe en memoria. Ni de lejos | 2,7 MB en 640 KB |
| **Streaming: leer a trozos mientras suena** | 16 KB, dure lo que dure |
| La canción **tiene que venir ya a 44100** | No hay dónde convertir sobre la marcha |
| Música y efectos, el mismo mezclador | Por eso conviven |

---

**Anterior:** [Capítulo 13](../../ch13/doc/README.md) ·
**Siguiente:** [Capítulo 15 — El driver IPX](../../ch15/doc/README.md)
