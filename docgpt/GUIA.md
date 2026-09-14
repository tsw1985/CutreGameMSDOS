# Guía de trabajo: ejecutar, observar y localizar fallos

Esta guía reúne el recorrido práctico del juego. Los comandos son para que los ejecutes en tu entorno; no se ha compilado ni iniciado DOSBox durante la elaboración del curso.

## Elige qué estás ejecutando

| Quieres hacer | Punto de entrada |
|---|---|
| Construir el juego completo en DOS | `make` de Borland desde la raíz; requiere Turbo C y NASM |
| Ejecutar el juego en DOS | Entrar en `bin` y ejecutar `game.exe` |
| Construir un laboratorio | Entrar en `tutorial\chNN`, ejecutar `make` y después `chapNN` |
| Jugar desde Linux con el binario existente | `./play.sh` desde la raíz |
| Probar un capítulo con IPX | Dos sesiones DOSBox preparadas como en la [lección 17](ch17.md) |
| Leer sin ejecutar el juego | Markdown, libro PDF o edición HTML de esta carpeta |

El curso nuevo no añade otros `chapNN.exe`. Los números 00–31 de `docgpt` organizan el aprendizaje; los 24 programas de `tutorial` son los laboratorios reutilizados.

## Partidas con el lanzador

```bash
./play.sh local
./play.sh both
./play.sh both -b -t neon -l 3
./play.sh both -b -t war -T neon -l 4
./play.sh local -demo
```

`local` reúne dos jugadores en un teclado y usa el mapa pequeño. `both` abre dos instancias conectadas. `-b` activa el mapa grande; `-t` selecciona tema; `-T` cambia el de la segunda ventana; `-l` selecciona nivel. Temas y niveles requieren mapa grande.

Para dos ordenadores, inicia primero el servidor y usa su IP en el otro:

```bash
./play.sh server -b -t war -l 3 -demo
./play.sh client 192.168.1.45 -b -t neon -l 3
```

El ejemplo de IP es ilustrativo: usa la dirección que corresponda a tu anfitrión. El servidor imprime una orden de cliente adaptada. El descubrimiento tiene un plazo de aproximadamente 30 segundos; arrancar el cliente indefinidamente más tarde no está garantizado.

En red, el servidor del lanzador solicita reproducir la intro y el cliente espera el acuerdo. ESC puede saltarla desde ambos extremos. El rol de servidor no determina quién controla P 1: eso se decide comparando los identificadores de la pareja.

## Opciones del entorno

| Opción | Efecto |
|---|---|
| `-p 5213` | Puerto UDP del túnel de DOSBox, 5213 por defecto |
| `-c auto` | Core del emulador, `auto` por defecto |
| `-y "fixed 30000"` | Configuración de ciclos predeterminada |
| `-demo`, `-d`, `/demo` | Intro donde el modo del lanzador la admite |

Un número de ciclos de DOSBox no es una equivalencia exacta con MHz de un procesador real. Si comparas rendimiento, registra también versión, core, máquina anfitriona y carga. Aumentar ciclos puede permitir completar más frames cuando faltaba tiempo, pero no acelera por encima del límite impuesto por la presentación.

No uses una recomendación de rendimiento como prueba de que tu tarjeta VGA, DMA o red funcionará igual en hardware real. Para ese objetivo ejecuta desde `COMMAND.COM` y registra el hardware y sus controladores.

## Controles del juego

| Modo | Movimiento | Disparo |
|---|---|---|
| Local, P 1 | Flechas | 5 numérico |
| Local, P 2 | W A S D | G |
| Red, jugador local en cada equipo | Flechas | 5 numérico |

ESC sale. Los capítulos tienen controles propios: C en el laboratorio de cámara, T en el de radar o D en el de desincronización son herramientas de esos ejercicios, no un menú universal del juego.

## Lee el log con una hipótesis

El lanzador usa directorios separados:

```bash
cat runlocal/GAME.LOG
cat runserv/GAME.LOG
cat runcli/GAME.LOG
```

En ejecución DOS directa, el log es relativo al directorio de trabajo. No busques necesariamente el archivo en la raíz.

| Mensaje o dato | Qué puedes deducir |
|---|---|
| `Map 640x400 theme ... level ... mem: near ... far ...` | Dimensiones, selección y memoria libre informadas tras cargar gráficos |
| `Demo: playing ...` y `Demo: finished ...` | Puntos donde comparar memoria antes y después de intro |
| `Radar: the original look has no numbers, no radar` | Ausencia esperada de radar con el aspecto original |
| `NET DESYNC at frame ...` | Resúmenes distintos del frame comparado; no identifica por sí solo la causa |
| Detección IPX ausente | Revisar driver o `ipx=true` antes de depurar lockstep |
| Desconexión por timeout | Revisar llegada de paquetes y servicio del bucle, además del enlace físico |

Que el total de memoria antes y después coincida es una comprobación útil, pero no demuestra por sí solo que todos los huecos tengan la misma distribución. Para una reserva grande importa el bloque contiguo disponible.

## Reducir un problema a su capa

**No arranca:** instalación, compilación, enlace y directorio actual. **Arranca sin imagen correcta:** contrato BMP, paleta, reserva y stride. **Imagen correcta con choques incorrectos:** máscara y coordenadas de mundo. **Solo falla con cámara:** transformación y clipping. **Solo falla en red:** compara entradas y estado por frame antes de culpar al render.

En sonido, distingue que no se detecte la tarjeta de que falte un WAV o de que el bucle deje de rellenar DMA. En red, distingue túnel UDP, API IPX, descubrimiento y coordinación de entradas. Son fallos con pruebas diferentes.

Registra siempre una reproducción mínima: modo, nivel, tema, posiciones, teclas, frame si existe y mensaje exacto. Cambia una causa candidata cada vez y conserva el caso que fallaba como control para la siguiente prueba.

[Índice](README.md) · [Fichas de API](APIS.md) · [Proyecto final](ch31.md)
