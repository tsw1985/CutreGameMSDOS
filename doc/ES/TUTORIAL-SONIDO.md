# Tutorial: usar el sonido en cualquier proyecto DOS

`src/sound.c` es un **módulo independiente**. No sabe nada de tanques ni de
este juego: le dices qué ficheros WAV cargar, te devuelve un número por cada
uno, y los reproduce cuando se lo pidas.

Este documento explica cómo usarlo en un programa nuevo. Si lo que quieres es
entender cómo funciona por dentro (el DMA, la interrupción, la mezcla), eso
está en [SOUND.md](SOUND.md).

---

## 1. Qué copio a mi proyecto

**Dos ficheros, y nada más:**

```
src/sound.c
header/sound.h
```

No dependen de ningún otro fichero de este repositorio. Sus únicas
dependencias son cabeceras estándar de Turbo C:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <alloc.h>
#include <conio.h>
```

Para compilarlo, una línea en tu Makefile como cualquier otra:

```
tcc -c -O2 -mh -Iheader -obin\sound.obj src\sound.c
```

Y añadir `bin\sound.obj` a la lista del enlazado.

---

## 2. Los cuatro pasos

Siempre son estos cuatro, en este orden. No hay más.

```c
#include "header\sound.h"

int boom;

int main()
{
    /* ---- PASO 1: encender la tarjeta ---- */
    if (sound_start() == 1){

        /* ---- PASO 2: cargar los sonidos ---- */
        boom = load_sound("..\\res\\boom.wav");

    }

    while (jugando){

        /* ---- PASO 3: una vez por vuelta, SIEMPRE ---- */
        sound_update();

        if (algo_ha_explotado){
            play_sound(boom);
        }

    }

    /* ---- PASO 4: apagar ---- */
    sound_end();

    return 0;
}
```

Y ya está. Eso es un programa con sonido.

### Los cuatro pasos, uno a uno

**`sound_start()`** busca la Sound Blaster, se apodera de su interrupción y
arranca el DMA reproduciendo silencio. Devuelve **1 si hay tarjeta y 0 si no
la hay**.

Lo importante: si devuelve 0, **no tienes que hacer nada especial**. Todas las
demás funciones comprueban solas que no hay tarjeta y no hacen nada. Tu
programa funciona exactamente igual, en silencio, sin un solo `if` extra
repartido por el código.

**`load_sound(ruta)`** carga un WAV y te devuelve **un número**, que es como
lo llamarás a partir de ahora. Devuelve **-1** si no pudo (fichero que no
existe, formato equivocado, sin huecos libres).

Un -1 también es seguro de guardar y usar: `play_sound(-1)` simplemente no
hace nada. No necesitas comprobarlo si no quieres.

**`sound_update()` NO ES OPCIONAL.** Es la única de las cuatro que se te puede
olvidar y romper el sonido. Si dejas de llamarla, la tarjeta repite una y otra
vez el trozo que ya tenía y suena entrecortado.

Llámala también **dentro de cualquier bucle que espere** por algo: mientras
carga un nivel, mientras espera una tecla, mientras espera por la red. En
cuanto tu bucle principal se para más de un instante, hay que seguir llamándola.

Es barata: casi todas las veces mira si la tarjeta ha pedido más datos, ve que
no, y vuelve inmediatamente.

**`sound_end()`** es obligatorio antes de salir. Si no lo llamas, el DMA sigue
leyendo una memoria que ya no es tuya y la interrupción sigue apuntando a un
programa que ya no está. Eso cuelga la máquina.

---

## 3. Reproducir y parar

Tres funciones, y no hace falta más:

```c
play_sound(boom);              /* suena UNA vez */

loop_sound(engine);            /* suena en bucle, sin parar */
stop_looping_sound(engine);    /* ya vale */
```

### El truco de `loop_sound()`

Se puede llamar **en todos los frames** sin problema. Si ese sonido ya está
sonando en bucle, no hace absolutamente nada.

Por eso el motor de un coche se escribe así de simple:

```c
if (tecla_pulsada){
    loop_sound(engine);
}else{
    stop_looping_sound(engine);
}
```

Ese `loop_sound()` se ejecuta setenta veces por segundo mientras la tecla esté
pulsada, y el sonido **no se reinicia**. Sin esa protección oirías un clic
setenta veces por segundo en lugar de un motor.

### No tienes que pensar en voces

Una "voz" es un hueco del mezclador: puede haber **8 sonidos a la vez**.
`play_sound()` busca un hueco libre él solo.

Te devuelve el número de voz que usó, pero **puedes ignorarlo tranquilamente**.
Solo sirve si quieres cortar ese sonido concreto antes de que acabe:

```c
int voz;

voz = play_sound(alarma);
/* ...mas tarde... */
stop_sound(voz);
```

Y si quieres silencio total:

```c
stop_all_sounds();
```

### Si se llenan las 8 voces

`play_sound()` roba la voz que esté más cerca de terminar, porque era la que
iba a callarse de todos modos. **Nunca roba un sonido en bucle**, así que un
disparo no puede callar el motor. Si absolutamente todo estuviera en bucle,
devuelve -1 y el sonido nuevo no se oye.

---

## 4. El volumen

El volumen es **del sonido**, no de cada llamada. En la práctica un motor
siempre quiere estar de fondo y un disparo siempre quiere estar delante:

```c
engine = load_sound("..\\res\\engine.wav");
set_sound_volume(engine, 16);     /* la mitad: de fondo */

boom = load_sound("..\\res\\boom.wav");
set_sound_volume(boom, 64);       /* el doble: por encima de todo */
```

La escala es `SOUND_VOLUME_MAX`, que vale **32**:

| Valor | Qué hace |
| --- | --- |
| 32 | El sonido tal cual se grabó |
| menos de 32 | Más bajo |
| más de 32 | **Amplificado** |

Amplificar está permitido y es normal. Si la suma de todo se pasa de lo que
cabe en un byte, el mezclador la **recorta** en lugar de dar la vuelta: el
sonido se distorsiona un poco, que es infinitamente mejor que el chasquido
horrible que produciría el desbordamiento.

Todos los sonidos empiezan en 32, así que si no llamas a `set_sound_volume()`
nunca, todo suena a su nivel original.

---

## 5. Música de fondo

Una canción **no se puede cargar en memoria**. A 44100 bytes por segundo, un
minuto son 2,6 MB, y en una máquina DOS eso no cabe de ninguna manera.

Por eso `play_song()` no la carga: **la va leyendo del fichero mientras suena**.

```c
play_song("..\\res\\micancion.wav");
```

Y ya está. Suena de fondo, en bucle, hasta que llames a `stop_song()`. Los
disparos, los motores y todo lo demás siguen funcionando exactamente igual
por encima.

### Lo que cuesta

| | |
|---|---|
| Memoria | **16 KB**, dure lo que dure la canción |
| Lecturas de disco | ~5 por segundo, de unos 8 KB |
| Velocidad de disco necesaria | 44 KB/s |
| Margen antes de que se oiga un corte | ~0,37 segundos |

La duración **da igual**: un minuto, cinco o media hora cuestan los mismos
16 KB, porque en memoria solo hay un tercio de segundo de música por delante.

### El bucle no tiene costura

Cuando llega al final del fichero vuelve al principio **en mitad de una
lectura**, así que el último byte de la canción y el primero de la vuelta
siguiente acaban pegados dentro del buffer. No hay ni silencio ni clic entre
vueltas.

### La condición: 44100 Hz exactos

Es el único requisito extra respecto a `load_sound()`, y es importante:

**La canción tiene que estar ya a 44100 Hz, 8 bits, mono.**

Los efectos se convierten solos porque eso se hace una vez, al arrancar. Una
canción que se lee a trozos no tiene dónde convertirse sobre la marcha, así
que la conviertes tú antes:

```
sox cancion.mp3 -b 8 -c 1 -e unsigned-integer -r 44100 micancion.wav
```

Un minuto te dará un fichero de unos **2,6 MB en disco**. En disco no importa,
solo importaba en memoria.

Si la frecuencia no es exacta, `play_song()` devuelve 0 y el log te dice qué
encontró:

```
Song: the WAV is at 22050 Hz and it has to be 44100 Hz
```

### El volumen

La música arranca a **16 sobre 32**, la mitad, y es a propósito: está sonando
*todo el rato*, así que se suma a todo lo demás en cada muestra. A tope no
dejaría sitio a los efectos y el mezclador se pasaría la partida recortando.

```c
set_song_volume(10);    /* todavia mas de fondo */
```

Si oyes distorsión al disparar con la música puesta, esto es lo primero que
hay que bajar.

### Si el disco no llega a tiempo

Si el buffer se vacía porque el juego se quedó parado más de medio segundo,
**se mete silencio** y la música continúa desde donde iba en cuanto haya
datos otra vez. No salta ni se adelanta: es un hueco, no un tirón.

Si pasa a menudo, sube `SONG_BUFFER_SIZE` en `sound.c` de 16384 a 32768 o
65536. Cuesta memoria, pero te da el doble o el cuádruple de margen.

### Solo una canción a la vez

`play_song()` sustituye a la que estuviera sonando y cierra su fichero. Para
cambiar de pista, la llamas otra vez y ya está.

---

## 6. Cómo tienen que ser los WAV

Esto es lo primero que hay que mirar cuando un sonido "no se oye":

| Requisito | Valor |
| --- | --- |
| Bits | **8** (no 16) |
| Canales | **1**, mono (no estéreo) |
| Codificación | **PCM** sin comprimir |
| Frecuencia | La que sea |

Los tres primeros son obligatorios: si no se cumplen, `load_sound()` devuelve
-1 y ese sonido no existe.

La frecuencia da igual **en los efectos**: si el WAV está grabado a 22050 Hz
y la tarjeta va a 44100, **se convierte solo al cargarlo**. Se hace una vez, al
arrancar, así que no cuesta nada mientras juegas. (La música es la excepción:
como se lee del disco sobre la marcha, esa sí tiene que venir ya a 44100.)

> **Ojo con la duración de los efectos.** Ningún sonido puede pasar de **65535
> bytes** una vez convertido, porque las muestras se recorren con un puntero
> `far` y su desplazamiento da la vuelta a los 64 KB. A 44100 Hz eso son
> **1,49 segundos por efecto**. Si te pasas, la parte que sobra sonaría a
> basura. Para algo más largo, o lo dejas en música, o bajas
> `SOUND_SAMPLE_RATE`.

Para convertir un fichero con `sox`:

```
sox entrada.wav -b 8 -c 1 -e unsigned-integer -r 44100 salida.wav
```

Y con Audacity: *Pista → Convertir a mono*, luego *Archivo → Exportar →
WAV 8-bit PCM sin signo*.

---

### Recomendación: prepáralos ya a 44100 de todas formas

La conversión automática funciona, pero **conviértelos tú fuera del juego
igualmente**. No es una manía: nos costó un sonido desaparecido descubrirlo.

Cuando un WAV llega con otra frecuencia, `load_sound()` tiene que:

1. reservar memoria para el fichero original,
2. reservar memoria para la versión convertida,
3. convertir,
4. **liberar el original**, dejando un agujero en la memoria.

Con cuatro efectos eso son cuatro picos de memoria y tres agujeros. En un DOS
de 640 KB, el último efecto en cargarse no encontró un hueco de su tamaño y
simplemente dejó de sonar, **sin ningún mensaje de error**.

Si el fichero ya viene a 44100, la función entra por la otra rama: reserva un
único buffer, se lo queda, y no libera nada. Ni picos ni agujeros.

Y de regalo suena mejor: `load_sound()` remuestrea por la vía rápida (repite
la muestra más cercana) y `sox` hace un trabajo bastante más fino.

---

## 7. Enterarte de lo que pasa (opcional)

La librería es **muda por defecto**. No imprime nada, nunca.

Es a propósito: un programa en modo gráfico no puede escribir en pantalla, un
`printf()` pintaría basura encima del juego. Así que la librería no decide por
ti dónde va el texto; se lo dices tú:

```c
void mi_log(char *mensaje)
{
    FILE *f;

    f = fopen("debug.log", "a");

    if (f != NULL){
        fprintf(f, "%s\n", mensaje);
        fclose(f);
    }
}

    /* antes de sound_start() */
    sound_set_log(mi_log);
```

A partir de ahí te contará cosas como `Sound: ready` o `Sound: no Sound
Blaster found, playing without sound`.

Si nunca llamas a `sound_set_log()`, no pasa nada: sigue funcionando, callada.

El juego de este repositorio hace exactamente eso: le pasa `tanks_log`, que
escribe en **`game.log`**, en el mismo directorio desde el que se ejecuta (o
sea, `bin\game.log`). Desde el sistema anfitrión se puede seguir en vivo:

```
tail -f bin/game.log
```

Y esto es además lo que hace que `sound.c` sea copiable: no tiene que incluir
ningún fichero tuyo para escribir en tu log.

---

## 8. Un programa completo, de principio a fin

Esto compila y funciona tal cual:

```c
#include <stdio.h>
#include <conio.h>
#include "header\sound.h"

int main()
{
    int boom;
    int engine;
    int key;

    if (sound_start() == 0){
        printf("No hay Sound Blaster. Saliendo.\n");
        return 1;
    }

    boom   = load_sound("boom.wav");
    engine = load_sound("engine.wav");

    if (boom == -1 || engine == -1){
        printf("No se pudieron cargar los WAV.\n");
        sound_end();
        return 1;
    }

    set_sound_volume(engine, 16);

    printf("ESPACIO = explosion   M = motor on/off   ESC = salir\n");

    while (1){

        /* SIEMPRE, en cada vuelta */
        sound_update();

        if (kbhit()){

            key = getch();

            if (key == 27){                  /* ESC */
                break;
            }

            if (key == ' '){
                play_sound(boom);
            }

            if (key == 'm' || key == 'M'){
                loop_sound(engine);
            }

            if (key == 'n' || key == 'N'){
                stop_looping_sound(engine);
            }

        }

    }

    sound_end();

    return 0;
}
```

Fíjate en que `sound_update()` está fuera del `if (kbhit())`. Tiene que
ejecutarse en **todas** las vueltas del bucle, se pulse una tecla o no.

---

## 9. Referencia completa

| Función | Qué hace |
| --- | --- |
| `sound_start()` | Enciende la tarjeta. **1 = hay sonido, 0 = no lo hay** |
| `sound_end()` | Apaga y libera. **Obligatorio antes de salir** |
| `sound_update()` | **Una vez por vuelta del bucle, siempre** |
| `load_sound(ruta)` | Carga un WAV. Devuelve su número, o **-1** |
| `set_sound_volume(id, vol)` | Sobre 32. Por encima amplifica |
| `play_sound(id)` | Suena una vez. Devuelve la voz (se puede ignorar) |
| `loop_sound(id)` | Suena en bucle. Llamarla repetida no molesta |
| `stop_looping_sound(id)` | Para ese sonido en bucle |
| `stop_sound(voz)` | Para esa voz concreta |
| `stop_all_sounds()` | Silencio total |
| `play_song(ruta)` | **Música de fondo en bucle, leída del disco** |
| `stop_song()` | Para la música y cierra el fichero |
| `set_song_volume(vol)` | Volumen de la música. Empieza en 16 |
| `sound_set_log(función)` | Dónde contar los problemas. Opcional |

Constantes que puedes tocar en `sound.h`:

| Constante | Valor | Qué es |
| --- | --- | --- |
| `SOUND_MAX_SAMPLES` | 8 | Cuántos WAV distintos puedes tener cargados |
| `SOUND_MAX_VOICES` | 8 | Cuántos sonidos pueden oírse a la vez |
| `SOUND_VOLUME_MAX` | 32 | La escala del volumen |

Y dentro de `sound.c`, si alguna vez lo necesitas:

| Constante | Valor | Qué es |
| --- | --- | --- |
| `SOUND_SAMPLE_RATE` | 44100 | La frecuencia a la que sale todo |
| `SOUND_HALF_SIZE` | 2048 | Cuánto tarda la tarjeta en pedir más (46 ms) |
| `SONG_BUFFER_SIZE` | 16384 | Cuánta música se lleva por delante (0,37 s) |
| `SONG_REFILL_LEVEL` | 8192 | Cuándo se va al disco a por más |

Bajar `SOUND_HALF_SIZE` hace que los disparos se oigan antes, pero deja menos
margen si un frame tarda mucho. Subirlo es más seguro pero el sonido va por
detrás de la imagen.

---

## 10. Errores típicos

| Síntoma | Causa casi segura |
| --- | --- |
| El sonido se entrecorta | No llamas a `sound_update()` en algún bucle que espera |
| `load_sound()` devuelve -1 | El WAV no es 8 bits mono PCM, o la ruta está mal |
| `sound_start()` devuelve 0 | No hay tarjeta, o la variable `BLASTER` no está puesta |
| Se oye un clic rapidísimo | Estás usando `play_sound()` cada frame donde querías `loop_sound()` |
| La máquina se cuelga al salir | Falta `sound_end()` |
| Un sonido corta a otro | Se han llenado las 8 voces. Sube `SOUND_MAX_VOICES` |
| Todo suena distorsionado | Volúmenes demasiado altos sumándose. Baja los de fondo |
| `play_song()` devuelve 0 | La canción no está a 44100 Hz exactos, o no es 8 bits mono. Mira el log |
| La música se corta a ratos | El disco no llega. Sube `SONG_BUFFER_SIZE` en `sound.c` |
| Distorsiona al disparar con música | Baja `set_song_volume()` y los volúmenes de fondo |

### Sobre la variable BLASTER

La librería lee la variable de entorno `BLASTER`, que es donde el driver de la
tarjeta deja apuntado dónde está:

```
SET BLASTER=A220 I5 D1 H5 T6
```

`A` es el puerto, `I` la interrupción, `D` el canal DMA de 8 bits. Si esa
variable no existe, se prueban los valores habituales (A220 I5 D1) y se deja
que la tarjeta conteste o no.

En DOSBox ya viene puesta sola.

---

## 11. Y para el juego de este repositorio

Así es exactamente como lo usa `src/main.c`:

```c
sound_set_log(tanks_log);

if (sound_start() == 1){

    sound_fire     = load_sound("..\\res\\fire.wav");
    sound_engine_1 = load_sound("..\\res\\engip1.wav");
    sound_engine_2 = load_sound("..\\res\\engip2.wav");
    sound_died     = load_sound("..\\res\\died.wav");

    set_sound_volume(sound_fire,     64);
    set_sound_volume(sound_engine_1, 34);
    set_sound_volume(sound_engine_2, 34);
    set_sound_volume(sound_died,     64);

    // Musica de fondo. Basta con dejar el WAV en res\ con ese nombre; si no
    // esta, play_song() lo dice en el log y el juego sigue sin musica.
    play_song("..\\res\\micancion.wav");

}
```

Los motores a 34 y los disparos a 64 para que un tiro se oiga por encima de
dos motores a todo gas sin que la suma reviente.

Y en el bucle principal, una sola línea:

```c
sound_update();
```
