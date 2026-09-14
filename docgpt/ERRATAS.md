# Revisión documental y discrepancias del código

Esta revisión distingue hechos comprobados leyendo fuentes y recursos de resultados que necesitarían ejecutarse en DOS. El material nuevo de `docgpt` contiene Markdown, ilustraciones, HTML, PDF y herramientas para editar esos documentos. El código, los scripts y los recursos originales del juego se conservan sin cambios. No se ha compilado ni lanzado el juego durante este trabajo.

## Dos defectos que el alumno no debe reproducir

### Reserva insuficiente de paleta

En [header/bmp.h](../header/bmp.h), `PALLETA_DATA_SIZE` vale **309**. `bmp_init_buffers()` en [src/bmp.c](../src/bmp.c) reserva esa cantidad para `buffer_palleta_data`. Sin embargo, `bmp_load_pallete_data()` escribe tres bytes para cada una de las 256 entradas: **768 bytes**.

La diferencia es **459 bytes fuera de la reserva**. Es un defecto de memoria presente en el código revisado; no solo una errata tipográfica del manual. Que una ejecución parezca funcionar no demuestra que esa escritura sea válida. Puede afectar a datos o metadatos vecinos.

La reconstrucción del alumno debe reservar 256 × 3 bytes y comprobar el resultado antes de cargar. Las demos ya reservan 768 para su propia paleta. Este trabajo documenta la discrepancia y no modifica la constante original.

### Producto de persiana demasiado grande para `int`

En [demos/blinds/blinds.c](../demos/blinds/blinds.c), el cálculo de `half` multiplica `(seno+256)` por 160 antes de dividir por 512. El producto matemático alcanza **81920**, por encima del máximo 32767 de un `int` de 16 bits. Incluso con seno cero da 40960.

El clamp posterior no recupera el resultado que se perdió. En C, el desbordamiento con signo no tiene un resultado correcto garantizado. La explicación de [blinds](demos/blinds/doc/README.md) muestra el cálculo ampliando a `long` **antes** del producto como propuesta didáctica; el archivo C permanece sin cambios.

## Correcciones de conceptos y cifras

| Tema | Precisión que se usa en este curso |
|---|---|
| Curso existente | Son **24** programas, incluido el radar; este curso nuevo tiene 32 lecciones documentales |
| Lanzador | `play.sh` ejecuta el juego y sus copias; no los `chapNN.exe`. Los laboratorios de red necesitan su propia sesión DOSBox/IPX |
| Cámara | Márgenes 100/70 permiten esquina del tanque en X=100..202, Y=70..112 |
| Margen cero | Amplía la zona muerta; no produce centrado continuo |
| Límites | Mapa 320 × 200 es válido con clamp cero; uno menor necesita otro tratamiento de copia |
| Memoria huge | La aritmética huge normaliza también al incrementar; no atribuir a todo incremento el fallo de un puntero far |
| Paleta | 256 colores requieren 768 componentes almacenados como bytes, no 309 |
| Explosión | `EXPLOSION_TOTAL_FRAMES` vale 35 |
| Laboratorio 20 | Su posición inicial es `(128,90)`, no la salida grande del juego |
| Laboratorio 21 | Tanque fijo en `(228,140)`, no `(300,190)` |
| IPX | Cabecera IPX de 30 bytes; ECB de 42 en memoria; envoltorio propio de 12. Payload lockstep 20 → paquete IPX de 62 bytes |
| P1 | Menor identificador de la pareja, independiente del rol servidor del lanzador |
| Identificadores iguales | La validación rechaza ese caso; no es una negociación resuelta como dos P2 |
| Lockstep | Redundancia acotada, sin retransmisión periódica de entradas durante la espera bloqueada. No garantiza recuperación ante cualquier pérdida |
| Checksum | Resumen con posibles colisiones; un aviso señala el frame comparado, no necesariamente el primer frame del fallo |
| Audio actual | 44100 Hz, DMA 4096 bytes en mitades de 2048, ocho voces y ocho muestras |
| Streaming | 16384 bytes representan 371,52 ms de datos; el DMA exige atención aproximadamente cada 46,44 ms |
| Ampliar streaming | No cambiar a 65536 sin rediseñar los contadores de 16 bits y sus conversiones |
| Volumen | 32 es ganancia unitaria. Puede amplificarse por encima; los setters no imponen un límite automático de 32 |
| `stop_all_sounds` | Detiene efectos; para la canción se usa `stop_song()` |
| Radar | Normalización geométrica grande 813; en las salidas el indicador da 50 % |
| Distancia aproximada | Error continuo máximo de aproximadamente 11,8 %, no una cota estricta del 11 % |
| Literal `100L` | Con `distance` ya `long`, cambiar solo el literal a `100` no convierte el producto a 16 bits |
| Intro | 7 segundos de fase por imagen, más fundidos y carga; diez efectos se reutilizan en quince imágenes |
| Dependencias de intro | Siete funciones gráficas y cinco de sonido; doce en total |
| `demo_show` | Cycle presenta inicialmente y luego sirve paleta/sonido directamente; no copia los píxeles cada frame |
| Sin `-demo` | No se reservan sus buffers dinámicos; código y datos estáticos enlazados sí existen |
| Añadir un efecto | También requiere objeto, regla y enlace en el Makefile principal |

## Límites de las demostraciones

Los laboratorios reutilizan código real, pero no todos contienen todas las reglas de `main.c`. `tut_try_move()` simplifica choques y cambia orientación antes de comprobar pared; el movimiento del juego tiene su propio orden. El capítulo 18 muestra movimiento compartido, no el duelo completo con todos sus subsistemas.

El cargador BMP espera los recursos de formato fijo del proyecto. El lector WAV espera una disposición canónica de cabecera y datos, no cualquier archivo RIFF legal. Algunas funciones imprimen errores y continúan sin propagar un resultado integral de carga. Estas limitaciones deben considerarse al desarrollar una versión más robusta.

Los microsegundos de efectos citados en material histórico no son medidas nuevas de esta revisión ni predicciones de un 386. Las explicaciones renovadas comparan trabajo por fila y por píxel, e indican cómo registrar una prueba reproducible.

## Qué se ha verificado y qué queda para ejecutar

La revisión documental comprueba rutas, navegación, fórmulas, tamaños de recursos y correspondencia con declaraciones y llamadas del código. Las pruebas numéricas ejercitan el modelo de coordenadas y máscara sin usar el hardware DOS. Las ilustraciones son esquemas del algoritmo; no se presentan como capturas de una partida real. La maquetación de PDF y el laboratorio HTML tienen una validación independiente del juego.

Quedan para las ejecuciones del alumno la compilación con Turbo C, comportamiento de IRQ y VGA, audio audible, rendimiento, latencia y partidas entre máquinas. No se presentan como pruebas realizadas aquí.

[Índice del curso](README.md) · [Mapa de fuentes](FUENTES.md)
