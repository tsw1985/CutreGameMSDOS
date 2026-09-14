# Libros, ilustraciones y laboratorio interactivo

## Qué contiene cada edición

| Archivo | Contenido |
|---|---|
| [curso_completo.pdf](pdf/curso_completo.pdf) | Índice del curso, lecciones 00–31, guías desde cero de checksum y cámara, bases y diez demos en español, laboratorio, guía práctica, fichas de API, cuaderno, glosario, fuentes, erratas y esta guía de edición |
| [camara_y_mapas.pdf](pdf/camara_y_mapas.pdf) | Lecciones 21–28 completas, guía de cámara con una tira de papel y laboratorio con soluciones |
| [camara_paso_a_paso.pdf](pdf/camara_paso_a_paso.pdf) | Guía extra 25A completa, como documento independiente |
| [checksum_paso_a_paso.pdf](pdf/checksum_paso_a_paso.pdf) | Guía extra 20A completa, como documento independiente |
| [demos.pdf](pdf/demos.pdf) | Introducción, bases y los diez efectos en español |
| [demos_en.pdf](pdf/demos_en.pdf) | El taller completo de demos en inglés |

La edición completa conserva el texto de los Markdown incluidos, sus tablas, fórmulas, ejemplos y soluciones. Los volúmenes especializados son selecciones de esas mismas lecciones. La versión inglesa de las demos se reúne en su propio libro para evitar duplicar cada tema en dos idiomas dentro del curso español.

Las guías [20A](ch20-paso-a-paso.md) y [25A](ch25-paso-a-paso.md) aparecen antes de los capítulos correspondientes. Están pensadas para aprender primero con ejemplos independientes; los capítulos 20 y 25 continúan después con una explicación ampliada del código del juego.

Los originales de `doc/`, `tutorial/` y `demos/` sirven de fuentes y comparación. Los libros contienen esta versión nueva dentro de `docgpt`; no son una impresión indiscriminada de archivos históricos ni incluyen el archivo privado de trabajo.

## Cómo usar los PDF

Abre el libro y pulsa una entrada del índice para saltar a su lección. El texto se puede seleccionar y buscar: prueba `stride`, `sound_update`, `zona muerta` o `rotozoom_span`. Las figuras son vectoriales y mantienen las líneas y etiquetas al ampliar.

El color identifica responsabilidades, pero las cifras, rótulos y fórmulas permiten leer también en impresión sin color. Si quieres escribir en papel, utiliza el cuaderno de estudio y resuelve el laboratorio antes de consultar las respuestas.

Los enlaces a capítulos incluidos saltan dentro del mismo libro. Las referencias al código apuntan a las fuentes del repositorio cuando existe una URL pública identificable; los Markdown conservan sus rutas locales para trabajar con el proyecto abierto.

## Leer y experimentar en el navegador

[El libro HTML](lectura/curso_completo.html) contiene el mismo recorrido en una página con índice. No requiere servicio web ni conexión: puedes abrirlo como archivo local.

[El laboratorio de cámara](camara.html) sí añade interacción: modelos de seguimiento, tamaño de mundo, márgenes, posición de tanque y cámara manual. Calcula los mismos límites, transformación y rectángulo visible explicados en el curso. Es un modelo didáctico; no emula Turbo C ni reproduce todas las reglas del juego.

Los ejemplos «Salida P1», «Salida P2», «Borde inferior derecho» y «Recorte» permiten obtener de inmediato casos resueltos en las lecciones. Las fórmulas visibles cambian a medida que mueves los controles.

## Dónde vive cada formato

```text
docgpt/
  README.md, ch00.md ... ch31.md   curso fuente
  demos/                          taller ES y EN
  LABS.md, APIS.md, ...            apéndices
  figuras/                        ilustraciones SVG y su índice
  camara.html                     laboratorio interactivo
  lectura/                        libros HTML generados
  pdf/                            libros PDF generados
  edicion/                        herramientas y registro de edición
```

Las herramientas de edición trabajan sobre documentos dentro de `docgpt`. No invocan el compilador del juego, MAKE de Borland ni DOSBox.

## Actualizar la edición

Para cambiar una explicación, edita su Markdown. Para cambiar un esquema, edita [ilustrar.py](edicion/ilustrar.py). Después puedes generar las figuras y los libros desde la raíz del repositorio con Python 3, el paquete Python `markdown` y Chrome disponible en tu sistema:

```bash
python3 docgpt/edicion/ilustrar.py
python3 docgpt/edicion/libros.py
python3 docgpt/edicion/libros.py --pdf
```

El segundo comando prepara HTML y el manifiesto de contenidos. El tercero produce los PDF con Chrome sin ventana. Si tu entorno aplica permisos adicionales al navegador, autoriza su ejecución conforme a la configuración de tu sistema.

El [manifiesto de edición](edicion/manifest.json) registra qué Markdown entra en cada libro y sus huellas de contenido. Permite comprobar que no se olvidó un capítulo al regenerar la colección.

[Volver al índice](README.md)
