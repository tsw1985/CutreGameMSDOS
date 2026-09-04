# SBWAV8 - Flujo del codigo

Documento de **flujo**, no de teoria: quien llama a quien, en que orden, y donde
esta cada cosa. La parte de "que es el DSP" o "que es el DMA" ya la tienes en tu
manual; aqui solo se explica el recorrido del programa.

Fichero principal: `sbwav8.c` (reproductor de WAV **8 bits** por DMA con doble buffer).

---

## 1. Ficheros de la carpeta

| Fichero | Que es |
|---|---|
| `sbwav8.c` | Reproductor de 8 bits. **Es el que funciona / el que vamos a usar.** |
| `SBWAV.C` | Version anterior, de **16 bits**. Mismo esqueleto, distinto canal DMA. |
| `SBWAV8.EXE`, `*.OBJ` | Binarios compilados en la maquina DOS. |
| `b.bat` | Script de compilar + probar. |
| `prodigy.wav` | WAV original de 16 bits (para `SBWAV.C`). |
| `prody8.wav` | El mismo, convertido a 8 bits (para `sbwav8.c`). |

`b.bat` hace tres cosas:

```
del sbwav8.exe
tcc -ml sbwav8.c      <- modelo LARGE obligatorio (punteros far para el buffer DMA)
sbwav8.exe prody8.wav
```

---

## 2. Mapa de llamadas (call graph)

```
main()
 |
 |-- fopen(argv[1])
 |-- leer_header_wav(f, &h) ................ valida RIFF/WAVE/fmt/data, PCM, 8 bits
 |
 |-- detectar_sb(&sb) ...................... lee la variable de entorno BLASTER
 |     `-- getenv("BLASTER")                 -> base_port, irq, dma8, dma16
 |
 |-- dsp_reset(sb_base) .................... resetea el DSP y espera el 0xAA
 |     |-- outp(DSP_RESET)
 |     `-- inp(DSP_READ_STATUS) / inp(DSP_READ)
 |
 |-- mixer_unmute_max(sb_base) ............. sube todos los volumenes
 |     `-- mixer_write() x6
 |
 |-- mixer_mostrar_canales_dma(sb_base) .... SOLO DIAGNOSTICO (registro 0x81)
 |
 |-- asignar_buffer_alineado(BUF_SIZE, 64K, &buffer_bloque_original)
 |     `-- farmalloc() ..................... reserva 32 KB alineados a 64 KB;
 |                                           devuelve el puntero alineado y deja
 |                                           el original en buffer_bloque_original
 |
 |-- fread() x2 ............................ precarga LAS DOS mitades del buffer
 |
 |-- instalar_isr(irq_num) ................. engancha isr_sb() al vector de la IRQ
 |     |-- getvect() / setvect()
 |     `-- outp(0x21 / 0xA1)                 desenmascara la IRQ en el PIC
 |
 |-- dsp_set_sample_rate(sb_base, rate)
 |     `-- dsp_write() x3                    comando 0x41 + frecuencia
 |
 |-- dma8_setup(sb.dma8, fisica, BUF_SIZE)   programa el 8237 en auto-init
 |     `-- outp(0x0A, 0x0C, 0x0B, puertos del canal, puerto de pagina)
 |
 |-- dsp_start_playback_8(base, HALF_SIZE, stereo)
 |     `-- dsp_write() x4                    comando 0xC6 + modo + cuenta
 |
 |-- [ BUCLE DE REPRODUCCION ]  <---- ver seccion 4
 |
 `-- fin:
       |-- dsp_write(base, 0xDA) ........... salir de auto-init de 8 bits
       |-- restaurar_isr(irq_num)
       |     |-- outp(0x21 / 0xA1)           vuelve a enmascarar la IRQ
       |     `-- setvect(vector, old_isr)    devuelve el vector original
       |-- farfree(buffer_bloque_original) . libera el bloque (ya sin DMA ni IRQ)
       `-- fclose(f)


isr_sb()   <---- NO la llama nadie desde C: la dispara el hardware
 |-- inp(DSP_ACK8(sb_base)) ................ confirma la IRQ ante el DSP
 |-- buffer_listo = 1 ...................... unica comunicacion con main()
 `-- outp(0x20, 0x20) ...................... EOI al PIC
```

---

## 3. Fase de arranque, paso a paso

El orden **importa**, y esta pensado asi a proposito:

1. **Abrir y validar el WAV** (`leer_header_wav`). Se lee la struct `WavHeader`
   de golpe con un solo `fread`. Por eso lleva `#pragma pack(push,1)`: sin eso
   Turbo C alinearia los campos y la lectura saldria descuadrada.
   Solo acepta WAV "canonicos" de 44 bytes de cabecera: si el fichero trae
   chunks extra (LIST, fact...), el `strncmp` de `data_id` falla y se rechaza.

2. **Averiguar donde esta la tarjeta** (`detectar_sb`). Parsea `BLASTER`
   (ej: `A220 I5 D1 H5 T6`) letra a letra. Si no existe la variable, deja los
   valores por defecto (0x220 / IRQ 5 / DMA 1) y devuelve 0, **pero el programa
   sigue igualmente**: el retorno no se comprueba en `main`.

3. **Resetear el DSP** (`dsp_reset`). Es el unico chequeo real de "hay tarjeta
   ahi?": si no llega el `0xAA`, se aborta.

4. **Subir volumenes** (`mixer_unmute_max`). Escribe tanto los registros del
   mixer antiguo (0x22, 0x04) como los del SB16 (0x30-0x33), asi funciona sea
   cual sea el modelo.

5. **Reservar el buffer** (`asignar_buffer_alineado`). Pide `32 KB + 64 KB` con
   `farmalloc`, calcula la direccion **fisica** (`SEG<<4 + OFF`) y la redondea
   hacia arriba hasta el siguiente multiplo de 64 KB. Esto es obligatorio:
   un canal DMA de 8 bits no puede cruzar un limite de pagina de 64 KB, y si lo
   cruza el sonido se corta o se repite en bucle.

   Como el puntero alineado normalmente **no** coincide con el que devolvio
   `farmalloc`, y `farfree` solo acepta el original, la funcion devuelve **los
   dos**: el alineado como valor de retorno (el que se usa para reproducir) y el
   original por el parametro `bloque_original`, que `main` guarda en
   `buffer_bloque_original` para liberarlo en `fin:`.

6. **Precargar las dos mitades** con dos `fread` de 16 KB. Cuando arranque el
   DMA ya hay 32 KB de audio esperando.

7. **Instalar la ISR** (`instalar_isr`). Traduce IRQ -> vector
   (IRQ 0-7 -> 0x08+irq, IRQ 8-15 -> 0x70+irq-8), guarda el vector antiguo en
   `old_isr` y desenmascara la linea en el PIC.

8. **Programar frecuencia, DMA y arrancar** en este orden exacto:
   `dsp_set_sample_rate` -> `dma8_setup` -> `dsp_start_playback_8`.
   El DMA se programa con el buffer **entero** (32 KB) y el DSP con **una
   mitad** (16 KB). Esa asimetria es la clave del doble buffer: el DMA da
   vueltas sin parar por los 32 KB (auto-init), y el DSP avisa con una IRQ cada
   vez que consume 16 KB, o sea, al terminar cada mitad.

---

## 4. El bucle de reproduccion

```c
while (!feof(f)) {
    while (!buffer_listo) {              /* espera activa */
        if (kbhit()) { getch(); goto fin; }
    }
    buffer_listo = 0;

    destino = buffer + (mitad_a_rellenar * HALF_SIZE);
    leidos  = fread(destino, 1, HALF_SIZE, f);
    if (leidos < HALF_SIZE)
        memset(destino + leidos, 128, HALF_SIZE - leidos);

    mitad_a_rellenar = !mitad_a_rellenar;
}
while (!buffer_listo) ;                  /* deja sonar la ultima mitad */
```

Como se reparten el trabajo los dos "hilos":

```
        HARDWARE (DMA + DSP)                     PROGRAMA (main)
        --------------------                     ---------------
        va leyendo el buffer solo                espera en while(!buffer_listo)
        termina la mitad 0
             |
             `--> IRQ --> isr_sb()
                            buffer_listo = 1  -->  se despierta
                                                   rellena la mitad 0 con fread
        ya esta sonando la mitad 1                 mitad_a_rellenar = 1
        termina la mitad 1
             |
             `--> IRQ --> isr_sb()
                            buffer_listo = 1  -->  rellena la mitad 1
        ...
```

Es decir: **el hardware siempre esta reproduciendo una mitad mientras el
programa rellena la otra**. Por eso no hay cortes.

Detalles del bucle:

- `buffer_listo` es `volatile` **obligatoriamente**: sin eso, el compilador ve
  que dentro del `while` nadie la cambia y optimiza el bucle a un cuelgue infinito.
- `memset(..., 128, ...)`: en 8 bits sin signo el silencio es 128, no 0. Rellena
  la cola del ultimo bloque para que no suene basura al final del fichero.
- `mitad_a_rellenar` empieza en 0 y va alternando con `!`, siempre una mitad por
  detras del DMA.
- La salida por teclado es un `goto fin`, que es la unica via de limpieza: hay
  que pasar si o si por `dsp_write(0xDA)` + `restaurar_isr()`. **Si el programa
  termina sin eso, la maquina queda con la IRQ enganchada y el DMA girando.**

---

## 5. La ISR

`isr_sb()` es minima a proposito. Solo hace tres cosas:

1. `inp(DSP_ACK8(sb_base))` - avisa al DSP de que la IRQ esta atendida. En 8
   bits el puerto de ack es el **mismo** que el de estado (`base+0xE`); en la
   version de 16 bits es `base+0xF`. Confundirlos es el fallo clasico: suena la
   primera mitad y se queda mudo, porque nunca llega la segunda IRQ.
2. Pone el flag `buffer_listo = 1`. **No lee del disco ni llama a DOS.** Todo el
   trabajo pesado lo hace `main()`.
3. Manda el EOI al PIC (0x20 al maestro, y tambien al esclavo si IRQ >= 8).

Variables globales que comparte con `main()`: `sb_base`, `irq_num`, `old_isr` y
`buffer_listo`. Son globales precisamente porque la ISR no puede recibir
parametros.

---

## 6. Diferencias con `SBWAV.C` (la version de 16 bits)

Mismo esqueleto, misma secuencia de llamadas. Cambia solo lo especifico del
tamano de muestra:

| | `SBWAV.C` (16 bits) | `sbwav8.c` (8 bits) |
|---|---|---|
| Bits aceptados | 16 | 8 |
| Comando DSP | `0xB6` | `0xC6` |
| Byte de modo | `0x10` (con signo) | `0x00` (sin signo) |
| Cuenta del DSP | en **palabras** (bytes/2) | en **bytes** |
| Ack de IRQ | `base+0xF` | `base+0xE` |
| Canales DMA | 4-7 (controlador de 16 bits) | 0-3 (controlador de 8 bits) |
| Puertos DMA | tablas `0xC0/0xC2/0x8F...` | `canal*2` y `canal*2+1` |
| Alineacion | 128 KB | 64 KB |
| Silencio de relleno | 0 | 128 |
| Mixer | no lo toca | `mixer_unmute_max` + diagnostico |

La `8` nacio como version de diagnostico para descartar el canal DMA de 16 bits
de la tarjeta; al final es la que suena bien.

---

## 7. Que hubo que tocar para meterlo en el juego

> Esta seccion era el plan. **Ya esta todo hecho**: el resultado es
> `src/sound.c`, documentado en [`SOUND.md`](SOUND.md). Se conserva aqui
> porque explica por que ese modulo es como es.

Ahora mismo es un programa autonomo. Para usarlo dentro de `CutreGameMSDOS`:

- **`main()` tiene que desaparecer** y partirse en algo tipo
  `sound_init(fichero)` / `sound_update()` / `sound_stop()`, porque el juego ya
  tiene su propio `main()` en `src/main.c`.
- **La espera activa `while (!buffer_listo)` no puede quedarse.** En el juego,
  `sound_update()` seria una funcion que se llama una vez por frame y que hace
  `if (buffer_listo) { rellenar; }` y vuelve enseguida, sin bloquear el bucle.
- **Convivencia de IRQs:** el juego ya instala su propio vector de INT 9 (IRQ 1,
  teclado) en `install_kbd()`. La tarjeta usa IRQ 5, asi que son vectores
  distintos y no chocan; pero las dos ISR tienen que seguir siendo cortas.
- **Limpieza obligatoria al salir:** `dsp_write(0xDA)`, `restaurar_isr()` y el
  `farfree(buffer_bloque_original)` tienen que ejecutarse en la misma ruta de
  salida donde el juego restaura el modo de video y el vector de teclado. Si se
  sale por otro camino, la maquina se queda tocada. **El orden importa:** primero
  parar el DSP y quitar la IRQ, y solo despues liberar el buffer; al reves se
  estaria soltando memoria que el DMA todavia esta leyendo.
- **El `fread` desde disco** con el juego corriendo puede dar tirones en maquinas
  lentas. Para efectos cortos (disparo, explosion) lo razonable es cargar el WAV
  entero en memoria una vez y no tocar el disco durante la partida.
- `printf` no se puede usar una vez el juego esta en modo grafico 13h: los
  mensajes de error de estas funciones habria que sacarlos por `tanks_log()`.
