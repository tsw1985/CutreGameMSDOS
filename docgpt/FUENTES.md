# Mapa de fuentes y rutas de lectura

El curso se ha contrastado con los archivos de este repositorio. Los nombres de funciones y constantes indican dónde verificar un comportamiento; las cifras temporales calculadas no constituyen mediciones de una ejecución DOS.

## Qué abrir para cada pregunta

| Pregunta | Código | Lecciones |
|---|---|---|
| ¿Qué orden sigue una partida? | [src/main.c](../src/main.c), `main`, `update_game`, `render`, `restart_game` | [00](ch00.md), [29](ch29.md) |
| ¿Cómo se cargan y dibujan imágenes? | [src/bmp.c](../src/bmp.c), [header/bmp.h](../header/bmp.h) | [04–07](ch04.md), [21–26](ch21.md) |
| ¿Qué datos guarda cada tanque? | [header/players.h](../header/players.h), [src/players.c](../src/players.c) | [09–13](ch09.md) |
| ¿Qué hace el teclado de los laboratorios? | [tutorial/tutlib.h](../tutorial/tutlib.h) | [08](ch08.md) |
| ¿Cómo funciona el audio actual? | [header/sound.h](../header/sound.h), [src/sound.c](../src/sound.c) | [14–16](ch14.md) |
| ¿Cómo se transportan mensajes? | [header/net.h](../header/net.h), [src/net.c](../src/net.c) | [17–18](ch17.md) |
| ¿Cómo se coordinan entradas y arranque? | [header/lockstep.h](../header/lockstep.h), [src/lockstep.c](../src/lockstep.c) | [19–20](ch19.md), [29](ch29.md) |
| ¿Cómo se construye el radar? | [src/main.c](../src/main.c), `compute_proximity_percent`, `draw_proximity_radar` | [28](ch28.md) |
| ¿Cómo se reproduce la intro? | [demos/demos.c](../demos/demos.c), [demos/demos.h](../demos/demos.h), [demos/demolib.h](../demos/demolib.h) | [30](ch30.md) |
| ¿Cómo se compila y enlaza? | [Makefile](../Makefile), [tutorial/Makefile](../tutorial/Makefile) y Makefile de cada capítulo | [01](ch01.md) |
| ¿Cómo se ejecuta el juego desde Linux? | [play.sh](../play.sh), [COMANDOS.md](../COMANDOS.md) | [01](ch01.md), [17](ch17.md) |

Los enlaces a intervalos de lecciones abren la primera; continúa con su navegación. Para los programas, la tabla del [índice](README.md) da la correspondencia exacta: los números de los dos cursos difieren.

## Cómo encajan los manuales anteriores

- [Índice bilingüe de doc](../doc/README.md): referencia por subsistema.
- [Manual de cámara](../doc/ES/MANUAL-CAMARA.md): explicación extensa del diseño y de problemas de memoria encontrados durante su desarrollo.
- [Manual de red](../doc/ES/MANUAL-RED.md): razonamiento de protocolo e integración.
- [Tutorial de sonido](../doc/ES/TUTORIAL-SONIDO.md): uso de la API desde otro programa.
- [Tutorial de red](../doc/ES/TUTORIAL-RED.md): API de datagramas y ejemplos independientes.
- [SOUND](../doc/ES/SOUND.md) y [NETWORK](../doc/ES/NETWORK.md): también conservan historia de implementaciones anteriores. Atiende sus notas de revisión antes de copiar constantes o nombres antiguos.
- [SBWAV8-FLOW](../doc/ES/SBWAV8-FLOW.md): describe el reproductor original, no sustituye el contrato del mezclador actual.

## Recursos contrastados sin modificarlos

Se comprobaron cabeceras y tamaños de las imágenes activas, las dimensiones de mapas y hojas y los formatos WAV. Las tres máscaras temáticas de cada nivel 1–5 coinciden byte a byte. Las duraciones del curso se calculan a partir de bytes de datos y frecuencia, no del tamaño total del fichero con cabecera.

Los directorios de respaldo pueden contener imágenes históricas distintas. No los uses para deducir el formato de las imágenes que carga `main.c` sin comprobar primero su ruta real.

## Leer código con criterio

Un comentario puede describir una versión pasada. Primero encuentra la declaración, después quién la escribe y finalmente quién la consume. Comprueba tamaños y tipos antes de repetir una afirmación numérica. Las [erratas](ERRATAS.md) muestran ejemplos concretos de por qué esta lectura importa.

[Índice del curso](README.md)
