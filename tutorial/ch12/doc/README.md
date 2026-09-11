# Capítulo 12 — Reproducir un WAV

*[English version](README-EN.md)*

**Qué vas a conseguir:** sonido. Y entender qué es el DMA, que es la pieza que
hace posible el audio en una máquina de esta época.

```
make
chap12
```

`1` disparo, `2` explosión, **`U` desactiva `sound_update()`** ← prueba eso.

---

## 1. Cargar no es sonar

```c
	fire = load_sound("..\\..\\res\\fire.wav");   /* tarda: hay disco */
	...
	play_sound(fire);                              /* instantáneo */
```

`load_sound()` abre el WAV, lo mete en memoria, lo convierte a 44100 Hz si hace
falta, y devuelve **un número**.

`play_sound()` coge ese número y lo pone a sonar.

Por eso **todos los sonidos se cargan al arrancar** y durante la partida solo se
llama a `play_sound()`. Si cargaras el WAV en el momento del disparo, el juego
se pararía medio segundo cada vez.

Si `load_sound()` devuelve **-1** es que no pudo. Y fíjate en que eso **no se
comprueba antes de cada `play_sound()`**: `play_sound(-1)` no hace nada. Así el
juego suena igual de bien con los ficheros que falten.

## 2. Qué es el DMA

Aquí está la idea del capítulo.

La tarjeta **no sabe leer de tus arrays**. Lo que sabe hacer es **DMA**: acceso
directo a memoria. Le dices:

> "Empieza a leer en esta dirección, tantos bytes, a esta velocidad"

y a partir de ahí **la tarjeta va sacando esos bytes por el altavoz ella sola,
sin molestar al procesador.**

Eso es lo que hace posible el sonido en un 486: si el procesador tuviera que
entregar 44.100 muestras por segundo a mano, no le quedaría tiempo para el
juego.

## 3. El buffer es diminuto, y está partido en dos

```
   4096 bytes en total, menos de una decima de segundo

   +------------------+------------------+
   |     mitad 0      |     mitad 1      |
   +------------------+------------------+
```

La tarjeta va leyendo en bucle: mitad 0, mitad 1, mitad 0...

Cuando **termina una mitad**, dispara una interrupción.

## 4. Por qué la interrupción no hace el trabajo

La rutina de interrupción de `sound.c` hace lo mínimo: apunta *"toca rellenar la
mitad 0"* y se va.

**No mezcla ahí dentro.** Una rutina de interrupción se ejecuta en mitad de
cualquier cosa que estuviera haciendo el programa, así que tiene que durar lo
menos posible. Mezclar 2048 muestras dentro de una interrupción es pedir
problemas.

Es exactamente la misma decisión que en el manejador del teclado: la
interrupción marca, el bucle normal trabaja.

## 5. Y por eso existe `sound_update()`

```c
		sound_update();
```

Desde tu bucle normal, ve la nota que dejó la interrupción y **rellena la mitad
que toque**, mezclando todos los sonidos que estén sonando.

**Hay que llamarlo en cada vuelta del bucle.** Si no, la tarjeta vuelve a leer
lo que ya había en el buffer y oyes el último trocito repetido en bucle.

Pulsa `U` en el programa y escúchalo. Ese ruido es exactamente lo que pasa
cuando tu bucle se queda atascado más de una décima de segundo.

Por eso el juego llama a `sound_update()` **incluso dentro de la espera de red**
del capítulo 18: un hipo de la red no puede convertirse en un hipo del sonido.

## 6. El volumen es por sonido

```c
	set_sound_volume(fire, 32);
```

No hay volumen global. Cada sonido tiene el suyo, de 0 a `SOUND_VOLUME_MAX`.
El capítulo 13 explica por qué eso importa tanto.

## 7. Experimentos

1. **Pulsa U y dispara.** El ruido que oyes es el buffer repitiéndose.
2. **Mete un `for` vacío larguísimo en el bucle** para simular un frame lento.
   Mismo efecto, sin tocar `sound_update()`.
3. **Dispara dos veces seguidas rápido.** Suenan las dos copias solapadas: eso
   ya es mezcla, y es el capítulo siguiente.
4. **Carga un WAV que no exista.** `load_sound()` devuelve -1 y `play_sound(-1)`
   no hace nada. El programa no se inmuta.

## 8. Lo que hay que llevarse

| | |
|---|---|
| Cargar y sonar son cosas distintas | Carga al arrancar, suena en el juego |
| **DMA: la tarjeta lee la memoria sola** | Sin eso no hay audio en un 486 |
| Buffer pequeño partido en dos mitades | Menos de 0,1 s en total |
| La interrupción solo marca; el bucle trabaja | Igual que el teclado |
| **`sound_update()` en cada vuelta** | Si no, se repite el último trozo |

---

**Anterior:** [Capítulo 11](../../ch11/doc/README.md) ·
**Siguiente:** [Capítulo 13 — Mezclar](../../ch13/doc/README.md)
