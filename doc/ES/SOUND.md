# SOUND.md — cómo funciona el sonido del juego

Documento de `src/sound.c` y `header/sound.h`. Explica qué hace cada función,
en qué orden se llaman, cómo las usa el juego y **por qué** está tomada cada
decisión.

Complementa a [`SBWAV8-FLOW.md`](SBWAV8-FLOW.md), que documenta el reproductor original
`sbwav8.c` del que salió este código.

---

## 1. El problema de partida

`sbwav8.c` reproduce **un** WAV: lo va leyendo del disco y lo envía a la
tarjeta. Para el juego hacen falta dos cosas que ese programa no hace:

**Varios sonidos a la vez.** La Sound Blaster tiene **un solo DAC**. No hay
forma de decirle "reproduce estos tres ficheros": el hardware solo sabe
consumir un flujo de bytes. La única salida es **sumar las muestras nosotros**
antes de dárselas. Eso es un mezclador por software, y es el corazón de este
módulo.

**No bloquear nunca.** `sbwav8.c` se queda parado en un `while (!buffer_listo)`
y lee del disco en mitad de la reproducción. En un juego a 70 fps eso es
inaceptable: un `fread` que tarde 40 ms se come tres frames. Aquí los WAV se
cargan enteros en memoria al arrancar y la función que se llama cada frame se
va de inmediato salvo cuando hay trabajo real.

---

## 2. Los conceptos de hardware

Cuatro ideas que hay que tener claras para que el resto se entienda.

### DMA

La CPU no le da las muestras a la tarjeta una a una. Se le dice al
**controlador DMA** (el chip 8237) "aquí hay un bloque de memoria, dáselo tú a
la tarjeta". A partir de ahí la transferencia ocurre sola, sin que la CPU haga
nada. Por eso puede sonar música mientras el juego dibuja.

### Auto-init

En modo normal el DMA transfiere el bloque y para. En modo **auto-init**, al
llegar al final vuelve al principio y sigue **para siempre**. El buffer se
convierte en un círculo. Nunca hay que rearrancar nada.

### Doble buffer

Si el buffer es un círculo del que la tarjeta va leyendo, escribir en la parte
que está leyendo justo ahora se oiría como un chasquido. La solución es
partirlo en dos mitades:

```
   +------------------+------------------+
   |     mitad 0      |     mitad 1      |
   +------------------+------------------+
     la tarjeta lee      nosotros
     de esta             escribimos en esta

   ...y cuando termina la 0, se cambian los papeles
```

La tarjeta siempre lee de una mitad mientras nosotros rellenamos la otra.
Nunca se pisan.

### La interrupción

¿Cómo sabemos cuándo ha terminado una mitad? La tarjeta lo dice: se le
programa un "tamaño de bloque" igual a media mitad y **levanta una IRQ** cada
vez que consume ese tamaño. Esa IRQ es la que dice "he terminado esta mitad,
rellénamela".

Con esto, el ciclo de vida completo del sonido es:

```
  arranque -> se programa el DMA en auto-init y se arranca el DSP
                      |
                      v
     la tarjeta consume la mitad 0 ---> IRQ ---> sound_isr()
                      |                             |
                      |                    levanta una bandera
                      |                             |
                      |                             v
                      |             (siguiente frame) sound_update()
                      |                             |
                      |                   sound_mix_half() rellena la mitad 0
                      v
     la tarjeta consume la mitad 1 ---> IRQ ---> ... y vuelta a empezar
```

---

## 3. Las dos estructuras de datos

Toda la lógica del mezclador se apoya en distinguir **sonido** de **voz**.

```c
struct sound_sample {          // un WAV cargado en memoria
    unsigned char far *data;   // las muestras, ya a SOUND_SAMPLE_RATE
    unsigned char far *block;  // lo que devolvió farmalloc(), para liberarlo
    unsigned long length;      // cuántas muestras
};

struct sound_voice {           // algo que está sonando ahora mismo
    int is_playing;
    int is_looping;
    int sample_id;             // qué sonido está reproduciendo
    int volume;                // sobre SOUND_VOLUME_MAX (64)
    unsigned long position;    // por dónde va
};
```

**Un sample es el sonido; una voz es una reproducción de ese sonido.** La
distinción importa: `fire.wav` es un único sample cargado una sola vez, pero
suena por dos voces distintas, la del jugador 1 y la del jugador 2. Si fueran
lo mismo, un disparo cortaría el del otro.

Las voces son **fijas**, no se reparten dinámicamente:

| Voz | Qué suena |
|---|---|
| `SOUND_VOICE_ENGINE_1` | motor del jugador 1, en bucle |
| `SOUND_VOICE_ENGINE_2` | motor del jugador 2, en bucle |
| `SOUND_VOICE_FIRE_1` | disparo del jugador 1 |
| `SOUND_VOICE_FIRE_2` | disparo del jugador 2 |
| `SOUND_VOICE_EXPLOSION` | la muerte, compartida |

**Por qué fijas y no un pool.** Con un pool ("dame la primera voz libre")
harían falta reglas de prioridad para decidir a quién robar cuando no queda
ninguna, y un disparo podría cortar un motor. Con voces fijas el
comportamiento es siempre el mismo y se sabe de antemano: disparar dos veces
seguidas solo corta **tu propio** disparo anterior, nunca nada más. En un
juego de dos tanques con cinco sonidos, un pool sería complejidad sin ninguna
ventaja.

La explosión sí comparte voz a propósito: si los dos tanques mueren en el
mismo frame se oye **un** estruendo y no dos superpuestos, que sonaría al
doble de volumen y distorsionaría.

---

## 4. Las funciones, una a una

### 4.1 Arranque

#### `sound_init()` — línea 603

El punto de entrada. Hace, en este orden:

1. Pone todos los samples y voces a cero
2. `sound_detect_card()` — dónde está la tarjeta
3. `sound_dsp_reset()` — ¿responde?
4. `sound_load_sample()` × 4 — carga los WAV
5. `sound_alloc_dma_buffer()` — memoria para el DMA
6. Llena el buffer de **silencio**
7. `sound_mixer_unmute()` — sube los volúmenes
8. `sound_install_isr()` — se apodera de la IRQ
9. `sound_dsp_set_sample_rate()` — 16000 Hz
10. `sound_dma_setup()` — programa el 8237
11. `sound_dsp_start_playback()` — arranca

Devuelve **1 si hay sonido y 0 si no**, y si devuelve 0 pone
`sound_is_ready = 0`, que hace que **todas** las demás funciones se vayan sin
hacer nada. El juego no tiene que comprobar nada: sin tarjeta funciona
exactamente igual, en silencio. Esa es la razón de que `sound_init()` no
aborte el juego cuando falla.

El paso 6 no es cosmético. El DMA empieza a leer **en el instante** en que se
arranca el DSP, y lo que lea es lo que suene. Si el buffer tuviera lo que
hubiera antes en esa memoria, lo primero que oirías al entrar al juego sería
un rugido de basura.

#### `sound_detect_card()` — línea 347

Lee la variable de entorno `BLASTER`, que el driver de la tarjeta deja puesta:

```
BLASTER=A220 I5 D1 H5 T6
         |    |  |
         |    |  +-- canal DMA de 8 bits
         |    +----- IRQ
         +---------- puerto base, en hexadecimal
```

Si no existe, usa los valores típicos (A220 I5 D1). No falla nunca: quien
decide si hay tarjeta de verdad es el siguiente paso.

#### `sound_dsp_reset()` — línea 163

Resetea el DSP y espera a que conteste `0xAA`. Ese `0xAA` es la forma que
tiene la tarjeta de decir "estoy aquí". Si tras 200 intentos no contesta, no
hay tarjeta en ese puerto y `sound_init()` se retira limpiamente.

**Es la detección de verdad.** `sound_detect_card()` solo lee una variable de
entorno, que puede estar mal o apuntar a una tarjeta que ya no está.

#### `sound_load_sample()` — línea 427

Lee un WAV entero a memoria y, si hace falta, **lo remuestrea**.

Sobre el remuestreo: el DSP tiene **una sola frecuencia de salida**. Nuestros
ficheros no coinciden:

| Fichero | Frecuencia original |
|---|---|
| `fire.wav` | 16000 Hz |
| `engip1.wav` | **22255 Hz** |
| `engip2.wav` | 16000 Hz |
| `died.wav` | 16000 Hz |

Si `engip1.wav` se reprodujera a 16000, sonaría **lento y grave**, porque sus
muestras estaban pensadas para consumirse más deprisa. Así que al cargarlo se
reconstruye a 16000:

```c
destination_length = (source_length * SOUND_SAMPLE_RATE) / header.sample_rate;

for (i = 0; i < destination_length; i++){
    destination[i] = source[(i * header.sample_rate) / SOUND_SAMPLE_RATE];
}
```

Es el método más barato que existe, coger la muestra más cercana. No es alta
fidelidad, pero para un motor y una explosión sobra, y **ocurre una sola vez
al arrancar**, así que no cuesta nada mientras juegas. La alternativa habría
sido remuestrear sobre la marcha en el mezclador, que sería una suma en coma
fija por muestra: más código y más CPU cada frame, para nada.

La cabecera se lee de golpe con `fread(&header, sizeof(WavHeader), 1, f)`, lo
que da por hecho que el WAV tiene exactamente 44 bytes de cabecera y ningún
chunk extra. Comprobado en los cuatro ficheros: `fmt ` en el 12 y `data` en el
36. Un WAV con un chunk `LIST` de metadatos rompería esto, y por eso se
validan las cuatro firmas antes de seguir.

#### `sound_alloc_dma_buffer()` — línea 391

Reserva la memoria del buffer, alineada a un límite de 64 KB.

**Por qué.** Los canales DMA de 8 bits llevan un contador de dirección de solo
16 bits, más un registro de página aparte para los bits altos. Al llegar al
final de una página de 64 KB **el contador da la vuelta al principio de esa
misma página** en vez de avanzar a la siguiente. Un buffer que quedase a
caballo de un límite reproduciría su segunda mitad como ruido.

La solución es pedir 64 KB de más y empezar en el primer límite que caiga
dentro del bloque, con lo que cruzar uno es imposible.

```c
block = farmalloc(size + align);
physical = ((unsigned long)FP_SEG(block) << 4) + FP_OFF(block);
remainder = physical % align;
if (remainder != 0){
    physical = physical + (align - remainder);
}
return MK_FP((unsigned int)(physical >> 4), 0);
```

**El puntero que devuelve NO es el que dio `farmalloc()`**, y `farfree()` solo
acepta el original. Por eso el original sale aparte, en `original_block`, y se
guarda en `sound_buffer_block` para poder liberarlo al final. Es un fallo
clásico y silencioso: liberar el alineado corrompe el heap.

> Se probó una versión que solo pedía `size * 2` y se limitaba a **no cruzar**
> un límite (que es la regla real del hardware) para ahorrar 64 KB de memoria
> convencional. Funciona igual sobre el papel, pero se volvió a esta porque es
> la que ya estaba probada en hardware real durante mucho tiempo. Cuando algo
> toca DMA y funciona, no se toca por elegancia.

#### `sound_mixer_unmute()` — línea 192

Sube todos los volúmenes del mixer al máximo. Escribe **los dos juegos de
registros**, el antiguo (`0x22`, `0x04`) y el de SB16 (`0x30`-`0x33`), porque
no sabemos qué tarjeta hay realmente. Escribir en registros que una tarjeta no
tiene es inofensivo; no escribirlos y que estén silenciados, no.

#### `sound_install_isr()` — línea 291

Dos cosas:

1. Guarda el vector de interrupción antiguo y pone el nuestro
2. **Desenmascara la línea en el PIC**, para que la interrupción llegue

```c
outportb(0x21, inportb(0x21) & ~(1 << irq));
```

Sin el segundo paso el PIC sigue bloqueando esa IRQ y no llega nunca, aunque
el vector esté bien puesto. La conversión de IRQ a vector no es directa: las
IRQ 0-7 van a los vectores `0x08 + irq` y las 8-15 a `0x70 + (irq - 8)`,
porque son de dos controladores distintos.

Esto convive sin problema con el `INT 9` del teclado del juego: son vectores
distintos y cada manejador manda su propio EOI.

#### `sound_dma_setup()` — línea 229

Programa el 8237. El orden **importa** y no es negociable:

```c
outportb(0x0A, 0x04 | channel);   // 1. enmascarar: que no transfiera mientras se programa
outportb(0x0C, 0x00);             // 2. limpiar el flip-flop de byte alto/bajo
outportb(0x0B, 0x40|0x10|0x08|channel);  // 3. modo: single + auto-init + memoria->dispositivo
outportb(addr_port,  addr & 0xFF);       // 4. dirección, byte bajo
outportb(addr_port,  (addr >> 8) & 0xFF);//    dirección, byte alto
outportb(page_port,  page);              //    página (bits 16-23)
outportb(count_port, count & 0xFF);      // 5. cuenta, byte bajo
outportb(count_port, (count >> 8) & 0xFF);//   cuenta, byte alto
outportb(0x0A, channel);          // 6. desenmascarar: ya puede transferir
```

El **flip-flop** del paso 2 es la trampa clásica: la dirección y la cuenta se
escriben en el mismo puerto, dos veces, y el chip lleva internamente un bit
que decide si lo que llega es el byte bajo o el alto. Si no se limpia antes,
puede estar en el estado contrario al que esperas y los bytes se intercambian:
la dirección sale al revés y el DMA lee de cualquier sitio.

El registro de página no es consecutivo, cada canal tiene el suyo (`0x87`,
`0x83`, `0x81`, `0x82`), de ahí el `switch`.

#### `sound_dsp_start_playback()` — línea 215

```c
sound_dsp_write(base, 0xC6);   // 8 bits, salida, auto-init
sound_dsp_write(base, 0x00);   // SIN signo
sound_dsp_write(base, count & 0xFF);
sound_dsp_write(base, (count >> 8) & 0xFF);
```

El `0x00` del modo dice **sin signo**, que es lo correcto: en un WAV de 8 bits
las muestras van de 0 a 255 con el silencio en 128, no de -128 a 127. Ponerlo
mal se oye como distorsión brutal, porque el silencio se interpretaría como
volumen máximo.

`count` es el tamaño de **media** mitad menos uno, y es lo que determina cada
cuánto llega la IRQ.

### 4.2 Funcionamiento

#### `sound_isr()` — línea 278

El manejador de la interrupción. Hace lo mínimo posible:

```c
inportb(DSP_ACK8(sound_base_port));   // confirmar la IRQ al DSP
sound_buffer_ready = 1;               // levantar la bandera
outportb(0x20, 0x20);                 // EOI al PIC maestro
if (sound_irq >= 8){ outportb(0xA0, 0x20); }
```

**Por qué no mezcla aquí.** Sería lo natural: ha terminado una mitad, rellénala
ya. Pero una ISR corre con las interrupciones desactivadas, así que todo lo
que tarde retrasa a las demás — el teclado del juego incluido. Mezclar 512
muestras por 5 voces dentro de la interrupción haría que el teclado empezase a
perder pulsaciones. Levantar una bandera y salir tarda microsegundos.

El `inportb` al puerto de ack es **obligatorio**. Si no se hace, el DSP
considera que la interrupción no ha sido atendida y no vuelve a levantar
ninguna: el sonido se para tras la primera. Para 8 bits ese puerto es
`base+0xE`, el mismo que el de estado (para 16 bits sería `base+0xF`, y ese es
justo el fallo del `SBWAV.C` original).

`sound_buffer_ready` es **`volatile`**. Sin eso, el compilador ve que en
`sound_update()` nadie escribe nunca esa variable y puede optimizar la
comprobación hasta hacerla desaparecer. `volatile` le dice "esto cambia por su
cuenta, léelo de memoria siempre".

#### `sound_update()` — línea 714

Se llama **una vez por frame** desde el bucle principal.

```c
if (sound_is_ready == 0){ return; }
if (sound_buffer_ready == 0){ return; }   // <-- casi siempre sale por aquí

sound_buffer_ready = 0;
sound_mix_half(sound_buffer + (sound_half_to_fill * SOUND_HALF_SIZE));
sound_half_to_fill = !sound_half_to_fill;
```

Lo importante es la **segunda línea**: a 16000 Hz con mitades de 512 muestras,
la tarjeta pide datos unas 31 veces por segundo, y el bucle corre a 70. O sea
que **más de la mitad de los frames esta función solo hace dos comparaciones y
se va**. El coste del sonido en el juego es prácticamente cero salvo esas 31
veces.

En el bucle principal está colocada **fuera** del `if` que separa los dos
estados de la ronda:

```c
if (explosion_pause_counter == 0){ ...jugando... }else{ ...ardiendo... }

sound_update();     // <-- fuera, siempre
```

Si estuviera dentro de la rama de "jugando", durante el medio segundo de la
explosión nadie rellenaría el buffer, la tarjeta repetiría la última mitad una
y otra vez, y el estruendo se oiría como un tartamudeo. El sonido tiene que
correr **siempre**, pase lo que pase en el juego.

#### `sound_mix_half()` — línea 531

El mezclador. Es donde ocurre lo de "varios sonidos a la vez".

```c
// 1. acumulador a cero
for (i...) sound_mix_buffer[i] = 0;

// 2. sumar cada voz activa
for (cada voz activa){
    for (i = 0; i < SOUND_HALF_SIZE; i++){
        si se acabó el sample: volver al principio (bucle) o apagar la voz
        value = (int)sample->data[voice->position] - 128;   // a con signo
        value = (value * voice->volume) / 64;               // volumen
        sound_mix_buffer[i] += value;
        voice->position++;
    }
}

// 3. recortar y volver a byte
for (i...){
    value = sound_mix_buffer[i];
    if (value >  127) value =  127;
    if (value < -128) value = -128;
    destination[i] = value + 128;
}
```

Tres decisiones aquí:

**El `- 128` y el `+ 128`.** En 8 bits sin signo el silencio es 128, no 0.
Sumar las muestras en crudo sumaría también los 128 de cada voz y saturaría
al instante con solo dos sonidos. Hay que llevarlas a un rango centrado en
cero, sumar ahí, y devolverlas al formato de la tarjeta al final.

**El acumulador es `int`, no `char`.** Cinco voces sumadas se salen
holgadamente de lo que cabe en un byte. Y eso es precisamente lo que hay que
poder ver para recortarlo bien: si se acumulase en un byte, se daría la vuelta
sin que nos enteremos.

**Se recorta, no se divide.** La alternativa a recortar sería dividir la suma
entre el número de voces, lo que nunca distorsiona pero hace que **el volumen
de todo baje en cuanto suena algo más**. Un disparo sonaría más flojo solo
porque hay un motor en marcha. Recortar es lo que hace una mesa de mezclas de
verdad: si te pasas, distorsiona; y distorsionar un pico suena infinitamente
mejor que dar la vuelta, que convertiría la parte más fuerte de un disparo en
un chasquido horrible.

Por eso los motores están a `SOUND_VOLUME_ENGINE 34` de 64, poco más de la
mitad: dos motores a tope más un disparo se recortarían constantemente.

### 4.3 Lo que usa el juego

Solo cuatro funciones, y ninguna sabe nada del hardware.

#### `sound_play(voice, sample_id, volume)` — línea 744

Un tiro, desde el principio. Si esa voz ya sonaba, la corta.

#### `sound_loop(voice, sample_id, volume)` — línea 762

Igual pero en bucle, **con un detalle que es la clave de su uso**:

```c
if (sound_voices[voice].is_playing == 1 &&
    sound_voices[voice].is_looping == 1 &&
    sound_voices[voice].sample_id == sample_id){
    return;                                  // ya está sonando: no tocar nada
}
```

Sin esa comprobación, llamar a `sound_loop()` cada frame mientras el jugador
tiene la tecla pulsada reiniciaría el motor **70 veces por segundo**: nunca
pasaría de la primera milésima del WAV y solo se oiría un zumbido. Con ella,
el juego puede pedir "motor encendido" en todos los frames sin pensar, que es
justo lo que hace.

#### `sound_stop(voice)` / `sound_stop_all()` — líneas 790 y 802

Silencian. No comprueban `sound_is_ready` a propósito, solo los límites del
índice, para que `sound_init()` pueda usarlas antes de que haya nada listo.

### 4.4 Cierre

#### `sound_shutdown()` — línea 679

**El orden es lo único que importa aquí, y es crítico:**

```c
sound_dsp_write(sound_base_port, 0xDA);   // 1. salir de auto-init
sound_dma_stop(sound_dma);                // 2. enmascarar el canal DMA
sound_restore_isr(sound_irq);             // 3. devolver la interrupción
farfree(sound_buffer_block);              // 4. AHORA sí, liberar
```

Mientras la tarjeta esté en auto-init, **el DMA sigue leyendo esa memoria por
su cuenta**, sin pasar por la CPU. Liberarla antes de pararlo significa que el
DMA seguiría reproduciendo memoria que ya es de otro: en el mejor caso ruido,
en el peor un cuelgue difícil de explicar cuando el heap la reutilice. Y
devolver el vector de interrupción antes de parar la tarjeta significa que
llegaría una IRQ al manejador viejo, que no espera nada de esto.

Se llama en `main.c:330`, **antes** de `player_free()` y `bmp_delete_buffers()`.

---

## 5. Dónde lo llama el juego

| `src/main.c` | Qué |
|---|---|
| `:269` | `sound_init()`, tras `init_graphics()` |
| `:417` | `sound_update()`, una vez por frame, fuera del `if` de estados |
| `:312` | `sound_update()` otra vez, dentro de la espera de red. Ver abajo |
| `:993` | `sound_play()` del disparo, **solo si `player_fire_bullet()` devuelve 1** |
| `:1019` / `:1021` | `sound_loop()` / `sound_stop()` del motor, según `is_driving` |
| `:384` `:385` | `sound_stop()` de los dos motores al impactar |
| `:391` | `sound_play()` de la explosión |
| `:501` | `sound_shutdown()` al salir |

Cuatro decisiones del cableado:

**El motor usa `is_driving`, no `is_moving`.** `is_moving` lo apaga la
animación de orugas en cuanto avanza un frame, así que el motor se cortaría
varias veces por segundo. `is_driving` es simplemente "hay una tecla de
dirección pulsada", que además hace que empujar contra un muro siga rugiendo,
que es lo que debe sonar.

**Al impactar hay que parar los motores a mano.** Durante la pausa de la
explosión no se lee el teclado, así que nadie los apagaría y seguirían en
bucle mientras los tanques arden.

**El disparo suena solo si sale la bala.** `player_fire_bullet()` devuelve 1 o
0 y el sonido cuelga de eso. Si no, pulsar disparar con tu bala aún en el aire
haría "pum" sin que salga nada. La regla vive en `player_fire_bullet()`, no
duplicada en `main.c`, para que no se desincronicen si algún día cambian las
condiciones de disparo.

**En red hay una segunda llamada a `sound_update()`.** En modo red el bucle se
para a esperar las teclas de la otra máquina, y esa espera puede durar más de
un frame si la red va mal. Sin llamar al sonido ahí dentro, la tarjeta
repetiría la última mitad y se oiría un tartamudeo justo en el peor momento.
Es la misma razón por la que está fuera del `if` de estados, aplicada a la
otra forma que tiene el bucle de detenerse. Ver `NETWORK.md`.

Cada tanque lleva en su `struct player` qué voz y qué sample usa
(`sound_engine_voice`, `sound_engine_sample`, `sound_fire_voice`), para que
`process_player_input()` siga siendo genérica y no tenga que saber que el
jugador 1 es el de `engip1.wav`.

---

## 6. Los números que puedes tocar

En `header/sound.h` y arriba de `src/sound.c`:

| Constante | Valor | Qué pasa si la cambias |
|---|---|---|
| `SOUND_SAMPLE_RATE` | 16000 | Más alto = mejor calidad y más CPU. Los WAV se remuestrean solos al valor que pongas. |
| `SOUND_HALF_SIZE` | 512 | **El compromiso importante.** Ver abajo. |
| `SOUND_VOLUME_FIRE` | 64 | De 64. Full. |
| `SOUND_VOLUME_ENGINE` | 34 | Bájalo si los motores tapan los disparos. |
| `SOUND_VOLUME_DIED` | 64 | |

Sobre `SOUND_HALF_SIZE`, que es el único delicado. A 16000 Hz, 512 muestras
son **32 ms**:

- Es el **retardo máximo** entre pulsar disparar y oírlo.
- Es también el **margen** que hay para rellenar antes de que la tarjeta se
  quede sin datos.

El bucle corre a ~70 Hz, o sea 14 ms por frame, así que caben dos frames
dentro de cada mitad: hay margen, pero no de sobra. Si en la máquina real
oyes tartamudeo (señal de que un frame tardó demasiado y la tarjeta repitió
una mitad), **súbelo a 1024**: el margen se dobla a 64 ms, a cambio de que el
disparo se oiga un pelín más tarde.

---

## 7. Dos problemas que costaron encontrar

Quedan aquí porque volverán a aparecer si se añaden más ficheros.

### `Code has no effect` en todos los `outp()`

Al compilar `sound.c` dentro del Makefile del juego salían 21 avisos
`Code has no effect`, uno por cada `outp(...)`, y ninguno en los `inp(...)`.

La diferencia entre unos y otros es que el valor de `inp()` se usa dentro de
un `if`/`while` y el de `outp()` se descarta. Con **`-O2`**, Turbo C considera
que descartar el resultado es código sin efecto. `sbwav8.c` no lo daba nunca
porque su `b.bat` compila con `tcc -ml`, **sin optimizaciones**.

La solución ya estaba en el propio proyecto: `bmp.c` usa **`outportb()`** de
`dos.h`, que devuelve `void`, y compila limpio con `-O2`. Se cambiaron los 26
accesos a puerto de `sound.c` a `outportb()` / `inportb()`. Escriben y leen
exactamente el mismo byte en el mismo puerto.

### `Fatal: Unable to execute command: tcc`

Al añadir `sound.obj` el enlazado dejó de funcionar. No era el Makefile:

```
antes:  tcc -mh -ebin\game.exe  bin\main.obj ... bin\gameloop.obj   → 110 caracteres
ahora:  ...  bin\gameloop.obj  bin\sound.obj                        → 127 caracteres
```

**DOS solo pasa 127 caracteres de argumentos** a un programa (la cola de
comandos del PSP). Con un `.obj` más se agotó justo, y MAKE no pudo lanzar
`tcc`. El error no significa que falte `tcc`, sino que no consiguió arrancarlo.

Se arregló con un **fichero de respuesta**, que es como lo hacían los
proyectos grandes de la época por esta misma razón:

```make
	@echo bin\main.obj bin\util.obj bin\video.obj > bin\link.rsp
	@echo bin\bmp.obj bin\players.obj bin\gameloop.obj bin\sound.obj >> bin\link.rsp
	$(LD) $(LDFLAGS) -ebin\game.exe @bin\link.rsp
```

La línea baja a 36 caracteres. **Si añades otro `.c`, mete su `.obj` en uno de
los dos `echo`** (y si un `echo` se hiciera muy largo, pártelo en otro `>>`).

---

## 8. Si algo no suena

Mira `tanks.log`. `sound_init()` deja ahí el diagnóstico, porque en modo
gráfico no se puede imprimir nada por pantalla:

| Línea en el log | Qué pasa |
|---|---|
| `Sound: ready` | Todo bien |
| `Sound: no Sound Blaster found...` | El DSP no contestó. Revisa la variable `BLASTER`; por defecto se asume A220 I5 D1. |
| `Sound: could not load X.wav` | El fichero no está en `res\`, o no es PCM mono de 8 bits |
| `Sound: not enough memory...` | No cupo el buffer de DMA (64 KB + 1 KB) |

Y si no aparece ninguna línea, `sound_init()` no llegó a llamarse.
