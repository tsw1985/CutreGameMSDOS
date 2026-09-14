# Taller de demos: diez efectos, desde los colores hasta el rotozoom

[English](README-EN.md) · [Curso completo](../README.md) · [Bases: empieza aquí](BASES.md)

Esta es la versión ampliada del taller dentro de `docgpt`. Los archivos C que estudias siguen en la carpeta `demos` original. Cada lección deriva la idea, sigue la implementación, resuelve un ejemplo, comprueba límites y propone una práctica con resultados esperados.

## Recorrido de aprendizaje

| Orden | Efecto | Lo que aprendes | Trabajo principal por frame, sin incluir presentación |
|---:|---|---|---|
| 0 | [Bases comunes](BASES.md) | Fases, punto fijo, buffers y tiempo | Preparar las herramientas |
| 1 | [Cycle](cycle/doc/README.md) | Índice frente a color y permutación | Paleta cada tres frames; imagen quieta |
| 2 | [Scroll](scroll/doc/README.md) | Wrap y dos tramos por fila | 400 copias, 64000 bytes |
| 3 | [Blinds](blinds/doc/README.md) | Apertura, desfase y rango del producto | 20 senos; 600 operaciones de tramos |
| 4 | [Stripes](stripes/doc/README.md) | Velocidades y posiciones por banda | 25 offsets; 400 copias |
| 5 | [Wobble](wobble/doc/README.md) | Amplitud, densidad y velocidad | 200 senos; 400 copias |
| 6 | [Bounce](bounce/doc/README.md) | Traslación y recorte con signo | Borrado y hasta 200 copias |
| 7 | [Ripple](ripple/doc/README.md) | Distancia y atenuación | 200 ondas atenuadas; 400 copias |
| 8 | [Mosaic](mosaic/doc/README.md) | Muestreo por bloques y reutilización | Depende del tamaño y de si ha cambiado |
| 9 | [Zoom](zoom/doc/README.md) | Muestreo inverso y tabla de columnas | 320 columnas precalculadas y muestras visibles |
| 10 | [Rotozoom](rotozoom/doc/README.md) | Transformación lineal e intervalos | Incrementos, tramos por fila y muestras visibles |

Son cuentas del algoritmo, no tiempos medidos en este trabajo. Para comparar rendimiento consulta el método de las bases comunes. Una operación de tramo puede copiar cero, uno o cientos de bytes: el número de llamadas por sí solo tampoco es un tiempo.

## Cómo funciona el reproductor

La API pública está en [demos.h](../../demos/demos.h): `demo_run()` y `demo_set_idle()`. [demos.c](../../demos/demos.c) administra memoria, imágenes, efectos, fundidos, tiempo y salida. [demolib.h](../../demos/demolib.h) comparte tablas y servicios con cada efecto.

```text
reservar image + screen + palette
  → iniciar tablas y sonido
  → cargar imagen y paleta
  → presentar imagen bajo negro
  → fundido de entrada
  → ejecutar efecto hasta end_tick o ESC
  → fundido de salida
  → siguiente imagen
  → cerrar sonido, liberar buffers y volver a texto
```

Se reservan 64000 + 64000 + 768 bytes para los tres buffers. La intro usa `res/demo.wav`. La paleta propia de las demos tiene el tamaño correcto; la discrepancia de reserva del módulo gráfico del juego está explicada en las [erratas](../ERRATAS.md).

## Quince imágenes, diez efectos

El orden de reproducción es **rotozoom, wobble, blinds, zoom, stripes, cycle, scroll, ripple, mosaic, bounce**. Con quince imágenes válidas se repiten los cinco primeros. El orden didáctico de la tabla empieza por técnicas más sencillas.

`DEMO_DEFAULT_SECONDS` vale **7**: es el intervalo de efecto, al que se añaden los fundidos y las lecturas. Cada pareja de fundidos contiene 66 esperas de retrazo. El total nominal de quince imágenes ronda dos minutos a un ritmo sostenido de 70 Hz; no es una duración exacta ni quince efectos distintos.

Una imagen que no se puede abrir se salta sin avanzar el índice de efecto. Eso cambia la asociación de las siguientes imágenes con la tabla.

## El contrato de todos los efectos

```c
int demo_name(unsigned char *image,
              unsigned char *screen,
              unsigned char *palette,
              unsigned long end_tick);
```

Conserva `image`, dibuja en `screen` y devuelve 1 al completar su intervalo o 0 al interrumpirse. Cycle modifica la paleta deliberadamente. Los demás efectos geométricos no necesitan hacerlo.

El camino habitual por frame pasa por `demo_show()`, que presenta y atiende sonido y callback. Cycle presenta inicialmente y después sirve retrazo, paleta y `demo_sound()` directamente. Los fundidos también atienden esos servicios.

Las dependencias externas son **siete funciones gráficas y cinco de sonido**. Las primeras están declaradas en `demolib.h`, las segundas en `demos.c`. Al reutilizarlas en otro proyecto debes proporcionar esas funciones y conservar sus firmas y convenciones del entorno DOS.

## Ejecutar y añadir una demo

Desde Linux, con el ejecutable del proyecto ya construido:

```bash
./play.sh local -demo
```

Desde DOS, situado en `bin`:

```bat
game.exe /demo
```

No son ejecutables separados por efecto. Para experimentar con uno, cambia en una copia de trabajo la tabla de selección o su función y construye allí el juego.

Añadir un efecto requiere su `.c`, su declaración, inclusión y entrada en `demo_effects[]`, **y** su objeto con reglas y enlace en el [Makefile principal](../../Makefile). El registro C no hace que el enlazador encuentre automáticamente otro archivo.

En red, el juego conecta un callback para mantener el enlace y aceptar ESC remoto durante la intro. Los recursos dinámicos se liberan antes de cargar el mundo. Sin `-demo` se evita esa reserva y ejecución; los datos estáticos y el código enlazado siguen existiendo.

## Entrega del taller

Completa las diez prácticas y añade una variación propia. Entrega su fórmula de origen, dos frames calculados a mano, límites numéricos, tratamiento del borde y una explicación de qué se precalcula. Así podrás defender tanto su apariencia como su implementación.

[Comenzar por las bases](BASES.md) · [Aplicarlo al juego](../ch30.md)
