# Antes del primer efecto: píxeles, tiempo y punto fijo

[Índice de efectos](README.md) · [English](BASES-EN.md) · [Curso completo](../README.md)

Este taller permite leer las diez demos aunque todavía no conozcas senos ni transformaciones. Necesitas entender arrays y bucles; puedes aprenderlos en las [lecciones 02–03](../ch02.md).

## 1. Tres recursos con responsabilidades distintas

Cada efecto recibe `image`, `screen`, `palette` y `end_tick`. `image` contiene los 64000 índices de la imagen original, que se conserva. `screen` es el resultado que puedes sobrescribir. `palette` contiene 768 componentes de color ya adaptados al DAC. `end_tick` indica cuándo terminar.

```text
imagen original → transformación → screen → VGA
paleta ──────────────────────────────────→ DAC
```

Modificar la imagen original acumularía transformaciones: el segundo frame partiría del primero, perdería detalle y podría producir arrastre. Los efectos geométricos reconstruyen cada resultado desde `image`. Cycle modifica la paleta deliberadamente.

Los BMP esperados son 320 × 200, indexados de 8 bits, sin compresión, DIB de 40 bytes, paleta completa y píxeles en offset 1078. Tamaño habitual: 65078 bytes. El índice 0 debe verse negro porque los recortes lo usan de fondo.

## 2. Una fila es un segmento de memoria

`demo_row[y]` equivale a `y*320` y se guarda como `unsigned int`: la última fila empieza en 63680. Un `int` con signo no basta para todos esos offsets.

`memcpy(dest,src,n)` copia n bytes; `memset(dest,color,n)` repite un byte. No hagas 320 llamadas de un byte si puedes describir el tramo completo. Tampoco conviertas una longitud negativa a unsigned: se volvería una petición enorme.

Antes de calcular un puntero verifica fila, columna y longitud. Recortar es determinar el intervalo visible; no es esperar que la biblioteca ignore los bytes fuera del buffer.

## 3. Un seno sin trigonometría previa

Piensa en una rueda y en la altura de un punto de su borde. Al completar una vuelta, su altura pasa por centro, arriba, centro, abajo y centro. Esa evolución suave y repetitiva es el seno.

Aquí una vuelta tiene 256 **fases**, no 360 grados:

| Fase | Parte de la vuelta | `demo_sin` |
|---:|---|---:|
| 0 | Inicio | 0 |
| 64 | Un cuarto | 256 |
| 128 | Media | 0 |
| 192 | Tres cuartos | −256 |
| 256 | Vuelta completa, equivale a 0 | 0 |

El coseno es el mismo movimiento adelantado un cuarto de vuelta: `demo_cos(a)` consulta seno de `a+64`. La tabla guarda aproximaciones enteras; fases vecinas pueden repetir valor.

**Amplitud** es cuánto se separa del centro. Para oscilar ±24 píxeles:

```text
desplazamiento = seno * 24 / 256
```

**Velocidad de fase** es cuánto avanzas cada frame. Sumar 4 vuelve al inicio en 64 frames. Sumar 3 necesita 256: el periodo general es `256 / mcd(256,velocidad)`, para velocidad entera no nula. El mcd es el mayor número que divide ambos. Velocidad cero deja la fase fija.

## 4. Punto fijo: cambiar la unidad del número

En lugar de guardar 1,5 en coma flotante, guardamos `1,5*256 = 384`. Los ocho bits bajos representan fracciones de 1/256.

```text
valor real     entero almacenado
0,5            128
1              256
1,5            384
2              512
```

Al multiplicar dos valores escalados, el producto lleva escala 65536. Dividir por 256 lo devuelve a la escala original:

```text
384 * 512 / 256 = 768 → 3,0
```

El producto 196608 necesita `long`. Ensancha los operandos **antes** de multiplicar. Asignar después a un `long` no repara un producto de 16 bits desbordado.

En el código aparece `>> 8` como reducción de escala. Para valores negativos, el desplazamiento a la derecha de enteros con signo depende de la implementación de C; estas rutinas se estudian para el comportamiento aritmético de su entorno Borland. Un port debe comprobar esa semántica. No todos los valores son literalmente tipos de 16 bits «8.8»: las coordenadas acumuladas de zoom y rotozoom usan `long` con ocho bits fraccionarios.

## 5. Tiempo de efecto frente a ritmo de dibujo

El reloj BIOS avanza aproximadamente 18,2 veces por segundo. Con `DEMO_DEFAULT_SECONDS=7`, el reproductor calcula `7*182/10=127` ticks por fase. Los fundidos van aparte: 33 retrazos de entrada y 33 de salida.

La fase gráfica avanza por frame, no por tick BIOS. Si una máquina dibuja menos frames durante esos 127 ticks, verá menos vueltas del efecto. Esperar retrazo pone un límite de presentación, pero no garantiza alcanzar 70 frames por segundo.

## 6. El final de frame es un servicio compartido

El camino habitual es `demo_show(screen)`: retrazo, volcado de 64000 bytes y `demo_sound()`. Este último atiende sonido y el callback del anfitrión. Cycle muestra una vez y después llama directamente a retrazo, DAC y `demo_sound()`.

Los fundidos también atienden esos servicios. ESC local o un callback que solicita abortar termina por el mismo camino de limpieza. No supongas que un efecto puede bloquear varios segundos porque la música «ya está sonando»: el DMA actual necesita atención aproximadamente cada 46,44 ms.

## 7. Un esqueleto para tu copia de trabajo

Fragmento que requiere `demolib.h` y el enlace del reproductor, no un ejecutable independiente:

```c
int demo_example(unsigned char *image,
                 unsigned char *screen,
                 unsigned char *palette,
                 unsigned long end_tick)
{
    (void)palette;
    while (demo_now() < end_tick) {
        if (demo_escape_pressed() == 1) {
            return 0;
        }
        memcpy(screen, image, DEMO_SCREEN);
        demo_show(screen);
    }
    return 1;
}
```

El efecto conserva la imagen, genera resultado, sirve sonido y permite salir. Al añadirlo necesitas su declaración, entrada en la tabla y objeto en el Makefile y response file. Las declaraciones manuales de funciones externas deben coincidir con su definición: el enlazador de C no garantiza detectar prototipos incompatibles.

## 8. Cómo experimentar y medir

Primero predice un frame con una imagen de prueba que tenga letras o una cuadrícula. Cambia un parámetro en una copia, registra su valor y describe la dirección del movimiento. Después observa una secuencia completa.

Para comparar rendimiento, fija máquina/emulador, versión, core, ciclos, imagen, duración, fase inicial y sonido. Separa generación de imagen, espera de retrazo y copia a VGA. Los microsegundos históricos de CPU anfitriona no predicen el tiempo del mismo C bajo Turbo C en un 386.

Los tutoriales cuentan operaciones para explicar dónde está el coste. Son análisis del algoritmo, no benchmarks ejecutados en esta revisión.

**Ejercicio de entrada:** seno=128, amplitud=20. ¿Qué desplazamiento produce? ¿Qué periodo tiene una fase que suma 4? **Solución:** 10 píxeles y 64 frames.

Continúa con [cycle](cycle/doc/README.md).
