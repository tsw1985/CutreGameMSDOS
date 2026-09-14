# Cuaderno de estudio y evaluación

Puedes imprimir estas páginas o copiar las fichas a tu cuaderno. No necesitas completar el curso en un número fijo de días. Avanza cuando puedas predecir y explicar el resultado de cada práctica.

## Una sesión que produce aprendizaje

Dedica la primera parte a recuperar lo anterior sin mirar: dibuja una fórmula, explica una función o predice un estado. Después estudia la lección y realiza una modificación pequeña en tu copia. Termina anotando una conclusión y una duda precisa.

| Ficha de sesión | Tu anotación |
|---|---|
| Lección y archivo abierto | |
| Resultado que predije | |
| Cambio realizado en mi copia | |
| Resultado observado | |
| Si difiere, primera hipótesis | |
| Explicación con mis palabras | |
| Próxima prueba concreta | |

## Puertas de paso entre bloques

| Después de | Debes poder hacer sin copiar la solución |
|---|---|
| 03 | Dibujar un array de imagen y calcular el índice de cualquier píxel pequeño |
| 07 | Explicar paleta, buffer y transparencia como tres decisiones separadas |
| 10 | Predecir si un candidato se acepta y qué posición se conserva al rechazarlo |
| 13 | Describir el ciclo de bala, explosión y ronda sin perder la puntuación |
| 16 | Dibujar las dos etapas de audio y calcular el plazo de una mitad DMA |
| 20 | Simular unos frames de entradas futuras y comparar el checksum correcto |
| 26 | Calcular ventana, clipping, cámara y máscara con números nuevos |
| 28 | Obtener las cifras de radar y demostrar que no dependen de la cámara |
| 31 | Reunir los módulos con orden de carga, servicio periódico y limpieza |
| Demos | Derivar una variación y justificar rangos y trabajo de su bucle |

## Preguntas de recuperación

1. ¿Por qué 640 ×400 exige pensar en el tipo antes de multiplicar?
2. ¿Por qué una paleta de 256 colores ocupa 768 bytes en memoria de este módulo?
3. ¿Qué diferencia hay entre tanque en mundo X=170 y tanque en pantalla X=50?
4. ¿Cuándo un sprite con X negativa conserva píxeles visibles?
5. ¿Por qué margen de cámara cero no significa centrado continuo?
6. ¿Qué pierde un programa que confunde identificador de muestra con voz?
7. ¿Puede el buffer de canción mantener audio correcto sin `sound_update`?
8. ¿Qué demuestra y qué no demuestra un checksum igual?
9. ¿Qué cambia al sustituir el tema y qué debe permanecer compartido?
10. ¿Por qué step 512 hace más pequeña una imagen en zoom?

Responde antes de leer: 1) producto en 16 bits puede desbordarse; 2) tres componentes por entrada; 3) cámara 120; 4) cuando su extremo derecho supera 0 y hay intersección vertical; 5) amplía la región donde puede moverse sin seguimiento; 6) recurso y reproducción son entidades diferentes; 7) no, falta producir la mitad DMA; 8) coincidencia de un resumen con colisiones posibles; 9) aspecto frente a geometría; 10) lee dos píxeles de origen por paso de destino.

## Evalúa tu proyecto con evidencia

No puntúes por líneas de código. Para cada entrega, asigna 0 si no existe,1 si funciona un caso normal,2 si supera los bordes documentados y 3 si puedes explicar y reproducir un fallo introducido deliberadamente en una copia y su corrección.

| Entrega | Evidencia | Puntuación 0–3 |
|---|---|---|
| Imagen y paleta | Patrón de prueba y salida a texto | |
| Movimiento y choques | Coordenadas de candidato y sondas | |
| Balas y rondas | Secuencia de entradas y estado final | |
| Audio | Mezcla, servicio y salida sin recursos activos | |
| Red | Frames e inputs comparados en dos extremos | |
| Mapa y cámara | Cuatro extremos y recortes parciales | |
| HUD y transiciones | Radar estable y reinicios coherentes | |
| Intro y recursos | Variación propia y limpieza | |

La puntuación te ayuda a elegir qué revisar, no certifica automáticamente un programa completo. El [proyecto final](ch31.md) especifica las pruebas de integración que deben acompañarla.

[Índice](README.md) · [Laboratorio resuelto](LABS.md)
