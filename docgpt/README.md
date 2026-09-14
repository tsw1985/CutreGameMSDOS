# Construye un juego de tanques para MS-DOS, desde cero

Este curso empieza con una pregunta pequeña —«¿cómo guardo un número?»— y termina con dos ordenadores simulando la misma batalla, cada uno mirando su parte de un mapa de 640 × 400 píxeles. Por el camino escribirás movimiento, colisiones, disparos, sonido, red, una cámara y una intro de efectos gráficos.

Está pensado para alguien que todavía no programa. No necesitas conocer C, trigonometría, VGA ni redes. Sí necesitas practicar: leer un algoritmo y reconocerlo no es lo mismo que poder construirlo.

El material está en español y los identificadores de los ejemplos están en inglés. Los ejemplos nuevos usan declaraciones al comienzo del bloque y `if`/`else`, para encajar con el estilo de Turbo C del proyecto.

Todo el material nuevo está reunido en **`docgpt/`**. La documentación y el código originales del proyecto se conservan para contrastar esta versión con sus fuentes.

## Escoge cómo estudiar

| Formato | Abrir |
|---|---|
| **Libro completo ilustrado** | [Curso completo en PDF](pdf/curso_completo.pdf): las 32 lecciones, el taller de demos y todos los apéndices en español |
| Cámara y mapas por separado | [PDF de cámara y mapas](pdf/camara_y_mapas.pdf): lecciones 21–28 y laboratorio resuelto |
| Los diez efectos | [PDF de demos](pdf/demos.pdf) · [Demo workshop in English](pdf/demos_en.pdf) |
| Lectura en navegador | [Libro HTML](lectura/curso_completo.html), con índice y enlaces internos |
| **Experimentar con la cámara** | [Laboratorio interactivo](camara.html): mueve tanque, cámara y márgenes y observa las cuentas |
| Markdown | El temario enlazado de esta página; cada lección incluye sus ilustraciones |

Los PDF contienen texto seleccionable, diagramas vectoriales, índice con enlaces y numeración de páginas. [Guía de formatos y reproducción de la edición](PDFS.md).

## Dos explicaciones para avanzar con más calma

Si cámara o checksum se hacen difíciles, estas guías empiezan con ejemplos ajenos al juego y después conectan con sus funciones reales:

- [25A. Cámara con una tira de papel](ch25-paso-a-paso.md): veinte letras, una ventana de diez casillas, un programa de consola y seguimiento explicado paso a paso. Continúa con la [lección 25 ampliada](ch25.md).
- [20A. Checksum con dos cuadernos](ch20-paso-a-paso.md): inventarios de fruta, sumas, coincidencias, etiquetas de tiempo e historial circular. Continúa con la [lección 20 ampliada](ch20.md), con la receta completa del juego y el recorrido de un mensaje.

Ambas guías están en el libro completo, justo antes de su lección. La de cámara también está en el volumen de cámara y mapas.

También puedes abrirlas como **PDF independientes**: [cámara paso a paso](pdf/camara_paso_a_paso.pdf) y [checksum paso a paso](pdf/checksum_paso_a_paso.pdf).

## Tres piezas que se estudian juntas

| Material | Para qué lo utilizarás |
|---|---|
| **Este curso, `docgpt/`** | Aprender conceptos, deducir algoritmos, resolver ejercicios y reconstruir el juego |
| [Los 24 programas de `tutorial/`](../tutorial/README.md) | Observar una idea aislada en un ejecutable pequeño y estudiar su `main()` |
| [Código del juego](../src/main.c) y [manuales originales](../doc/README.md) | Integrar las piezas y consultar detalles de implementación |

Los capítulos nuevos son documentos; no generan otros `chapNN.exe`. Cuando una lección usa un programa existente, indica su ruta exacta. Los números de este curso y de `tutorial/` **no coinciden**: consulta la columna «Laboratorio» del temario.

Los programas de `tutorial/` reutilizan módulos reales, pero también tienen simplificaciones y utilidades propias en `tutlib.h`. Por ejemplo, `ch18` demuestra movimiento en red; no contiene todas las reglas, sonidos y transiciones de `main.c`. El proyecto final enseña a completar esa integración.

## Cómo estudiar cada lección

1. Lee qué vas a construir y abre el código enlazado.
2. Antes de ejecutar, predice un resultado concreto: una coordenada, un byte, un estado o un sonido.
3. Haz la práctica en una copia de trabajo. Modifica una sola idea cada vez.
4. Compara el resultado con la explicación y resuelve el ejercicio sin mirar su solución.
5. Escribe dos frases en tu cuaderno: qué cambió y por qué cambió.

No hay un plazo obligatorio. Una sesión de 45–90 minutos permite trabajar una idea; cámara, memoria y lockstep suelen necesitar varias. Avanza cuando puedas explicar y reproducir el resultado, aunque tardes más.

## El recorrido completo

| Lección | Construyes o comprendes | Laboratorio existente |
|---|---|---|
| [00. El juego como sistema](ch00.md) | Un mapa mental y tu cuaderno de trabajo | `src/main.c` |
| [01. Preparar el taller](ch01.md) | Rutas, herramientas y primer ejecutable | `tutorial/ch01` |
| [02. C desde la primera variable](ch02.md) | Condiciones, bucles, funciones y números | Ejemplos de consola |
| [03. Arrays, punteros y estructuras](ch03.md) | La memoria de una imagen y de un tanque | `header/players.h` |
| [04. Encender la VGA](ch04.md) | Píxeles e índices de color | `tutorial/ch01` |
| [05. Leer imágenes BMP](ch05.md) | Cabecera, paleta, filas y errores de carga | `tutorial/ch02` |
| [06. Construir un fotograma](ch06.md) | Doble buffer y ritmo de presentación | `tutorial/ch03` |
| [07. Dibujar un sprite](ch07.md) | Extracción, transparencia y composición | `tutorial/ch04` |
| [08. Escuchar el teclado](ch08.md) | Teclas mantenidas e interrupciones | `tutorial/ch05` |
| [09. Dar vida al tanque](ch09.md) | Estado, orientación y animación | `tutorial/ch06` |
| [10. Chocar con el mundo](ch10.md) | Movimiento candidato y máscara lógica | `tutorial/ch07` |
| [11. Dos jugadores](ch11.md) | Entradas en bits y choque entre tanques | `tutorial/ch08` |
| [12. Disparar](ch12.md) | Bala, flanco de pulsación e impactos | `tutorial/ch09` |
| [13. Terminar una ronda](ch13.md) | Explosiones, puntos y reinicio | `tutorial/ch10` |
| [14. Encontrar la tarjeta de sonido](ch14.md) | DSP, IRQ y DMA | `tutorial/ch11–12` |
| [15. Mezclar sonidos](ch15.md) | Muestras, voces, volumen y saturación | `tutorial/ch13` |
| [16. Música sin cargarla entera](ch16.md) | Streaming y plazos de servicio | `tutorial/ch14` |
| [17. Abrir la puerta de IPX](ch17.md) | Driver, socket y datagramas | `tutorial/ch15` |
| [18. Encontrarse y conversar](ch18.md) | Descubrimiento, envío y recepción | `tutorial/ch16–17` |
| [19. Simular al mismo paso](ch19.md) | Lockstep, retardo y anillo de entradas | `tutorial/ch18` |
| [20. Detectar divergencias](ch20.md) | Checksums con número de frame | `tutorial/ch19` |
| [21. Separar mundo y pantalla](ch21.md) | Coordenadas y transformaciones | `tutorial/ch20` |
| [22. Abrir una ventana al mapa](ch22.md) | Stride y copia por filas | `tutorial/ch21` |
| [23. Recorrer 256.000 bytes](ch23.md) | Memoria segmentada, `huge` y presupuesto | `tutorial/ch23` |
| [24. Recortar en los cuatro bordes](ch24.md) | Rectángulos visibles y direcciones válidas | `tutorial/ch04`, `src/bmp.c` |
| [25. Construir la cámara](ch25.md) | Zona muerta, seguimiento, límites y reinicio | `tutorial/ch22` |
| [26. Empaquetar las paredes](ch26.md) | Una máscara de un bit por píxel | `tutorial/ch23` |
| [27. Diseñar mapas jugables](ch27.md) | Temas, niveles, puertas y puntos de salida | `res/15Level` |
| [28. Orientarse con el radar](ch28.md) | HUD, distancia aproximada y cifras | `tutorial/ch24` |
| [29. Integrar y pulir](ch29.md) | Fundidos, barreras, carga y limpieza | `src/main.c` |
| [30. Crear una intro](ch30.md) | Un reproductor y diez efectos razonados | `demos/` |
| [31. Reconstrucción final](ch31.md) | Tu versión del juego, por entregas verificables | El repositorio completo |

## La ruta especial de cámara y mapas

El corazón del curso son las lecciones **21–28**. No se limitan a presentar `pantalla = mundo − cámara`: deducen las direcciones de memoria, explican qué se recorta, calculan la zona muerta y prueban los bordes y el mapa pequeño.

Si ya sabes C, empieza por [21](ch21.md). Si no, llega por el recorrido normal: los arrays, los tipos con signo y los bucles de los primeros capítulos son justamente las herramientas que necesitarás.

## Material de apoyo

- [Laboratorio de cámara con soluciones](LABS.md): cuentas y casos límite que puedes verificar sin una VGA.
- [Guía práctica de ejecución y diagnóstico](GUIA.md): modos, controles, directorios y lectura del log.
- [Fichas de API](APIS.md): contratos de gráficos, jugadores, audio, IPX y lockstep.
- [Cuaderno de estudio](CUADERNO.md): sesiones, preguntas de recuperación y evaluación por entregas.
- [Taller completo de demos](demos/README.md): bases y diez efectos reescritos, con versión inglesa.
- [Glosario](GLOSARIO.md): vocabulario explicado con ejemplos del juego.
- [Mapa de fuentes](FUENTES.md): dónde vive cada responsabilidad y qué documentación consultar.
- [Revisión y discrepancias del código](ERRATAS.md): diferencias que debes conocer al leer el material anterior.

La revisión ha encontrado, entre otras cosas, una reserva de paleta de 309 bytes frente a 768 bytes escritos y un producto de la persiana que excede un `int` de 16 bits. Están explicados con evidencia en las erratas. Este trabajo añade documentación y recursos didácticos; no modifica esas implementaciones ni presenta una compilación DOS como comprobada.

**Empieza aquí:** [00. El juego como sistema](ch00.md).
