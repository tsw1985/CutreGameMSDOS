# Fichas de módulos: qué recibe cada pieza y quién la mantiene viva

Estas fichas sirven para volver al código después de estudiar los capítulos. Las cabeceras enlazadas contienen las firmas exactas; esta guía añade responsabilidades, resultados y condiciones que no conviene perder al reutilizarlas.

## Gráficos y mundo

Fuente: [bmp.h](../header/bmp.h) y [bmp.c](../src/bmp.c).

| Operación | Responsabilidad y condición |
|---|---|
| `bmp_init_buffers(width,height)` | Establecer dimensiones, cámara y reservas. La reserva de paleta actual tiene la discrepancia documentada en erratas |
| `bmp_fill_background_in_main_buffer(path)` | Leer filas BMP al mapa completo; espera dimensiones y formato del proyecto |
| `bmp_fill_background_collision_in_buffer(path)` | Marcar en la máscara los píxeles de índice 252; la máscara debe empezar limpia |
| `bmp_extract_pallete_from_file(path)` | Leer la paleta del recurso para la presentación común |
| `bmp_open_sprite_sheet` / `bmp_extract_sprite` | Leer fragmentos sin conservar otra hoja completa en memoria |
| `bmp_draw_world_window(destination)` | Copiar 200 filas de 320; requiere cámara válida y mapa al menos del tamaño de pantalla |
| `bmp_camera_follow` | Corregir solo lo que sobresale de la zona y aplicar límites |
| `bmp_camera_snap` | Colocar el centro del objetivo en el centro de pantalla cuando los límites lo permiten |
| `bmp_is_wall(x,y)` | Consultar mundo; fuera cuenta como pared. No recibe coordenadas de pantalla |

El módulo gráfico guarda datos globales: no es una API para crear varios mundos independientes a la vez. Una ampliación a múltiples ventanas o escenas necesitaría decidir cómo representar esas instancias.

## Estado del jugador

Fuente: [players.h](../header/players.h) y [players.c](../src/players.c). La estructura conserva posición, orientación, sprites, datos de bala, animación, explosión y otros estados del tanque.

La inicialización de memoria no equivale a configurar todas las reglas de una partida. `main.c` asigna velocidades, contadores, sonidos y salidas. `player_reset` restablece datos de ronda y conserva victorias según su uso en el juego.

Para integrar movimiento, calcula un candidato, actualiza sus sondas, comprueba paredes y rival, y solo entonces acepta la posición. Las funciones auxiliares del tutorial simplifican esa secuencia; compara el laboratorio con `process_player_input` antes de trasladarlo al duelo final.

## Audio: muestra cargada frente a voz activa

Fuente: [sound.h](../header/sound.h) y [sound.c](../src/sound.c).

| Función | Resultado o efecto |
|---|---|
| `sound_start()` | 1 si prepara hardware y buffers; 0 si falla |
| `load_sound(path)` | Identificador de muestra, o −1 |
| `set_sound_volume(id,volume)` | Configurar volumen de esa muestra para reproducciones que lo toman al iniciarse |
| `play_sound(id)` | Voz asignada, o −1; puede reemplazar una voz no cíclica cerca de terminar |
| `loop_sound(id)` | Iniciar bucle o devolver la voz del bucle ya existente para esa muestra |
| `stop_sound(voice)` | Detener una voz concreta, no liberar la muestra |
| `stop_looping_sound(id)` | Detener el bucle identificado por muestra |
| `stop_all_sounds()` | Detener voces de efectos; la canción tiene control separado |
| `play_song(path)` | 1 al iniciar la canción en streaming, 0 si falla; sustituye la anterior |
| `set_song_volume(volume)` | Ajustar ganancia de la canción |
| `stop_song()` | Detener canción y cerrar su archivo |
| `sound_update()` | Mezclar la mitad libre y mantener la lectura anticipada |
| `sound_end()` | Devolver hardware, ficheros y memoria al terminar |
| `sound_set_log(callback)` | Elegir dónde enviar mensajes del módulo |

No intercambies identificador de muestra con índice de voz aunque ambos sean enteros. Una muestra puede sonar en varias voces; detener una no borra el recurso para las otras.

La escala de ganancia usa 32 como unidad. Los parámetros no se limitan automáticamente a 32; los productos de mezcla deben mantenerse dentro de sus tipos antes de la saturación final.

### Programa mínimo para escuchar un efecto

Ejemplo completo para una copia situada en la raíz del proyecto y ejecutada desde allí; utiliza sus `header` y `res`. Se compila por el alumno con la misma configuración de inclusión y bibliotecas de Turbo C del proyecto. Es un ejemplo documental, no un ejecutable nuevo entregado ni probado en DOS:

```c
#include <stdio.h>
#include <conio.h>
#include "header/sound.h"

int main(void)
{
    int sample;
    int key;
    int running;

    if (sound_start() == 0) {
        printf("Sound is unavailable.\n");
        return 1;
    }
    sample = load_sound("res\\fire.wav");
    if (sample == -1) {
        printf("Cannot load the effect.\n");
        sound_end();
        return 1;
    }
    set_sound_volume(sample, 16);
    printf("SPACE: play. ESC: exit.\n");
    running = 1;
    while (running == 1) {
        sound_update();
        if (kbhit()) {
            key = getch();
            if (key == 27) {
                running = 0;
            } else if (key == ' ') {
                play_sound(sample);
            }
        }
    }
    sound_end();
    return 0;
}
```

Este bucle evita bloquearse esperando una tecla: primero comprueba `kbhit` y sigue atendiendo audio mientras no llega ninguna. No es el lector de teclas mantenidas del juego; la repetición BIOS puede generar varias pulsaciones de espacio. Para disparo por flanco utiliza el modelo de [la lección 12](ch12.md).

## Transporte IPX

Fuente: [net.h](../header/net.h) y [net.c](../src/net.c).

| Función | Contrato |
|---|---|
| `net_start()` / `net_end()` | Abrir y cerrar la interfaz y socket |
| `net_update()` | Procesar recepción, estado y trabajo periódico del transporte |
| `net_find_peer(seconds)` | Descubrimiento simétrico de una pareja |
| `net_wait_for_client(seconds)` | Espera con rol de servidor para programas que lo necesiten |
| `net_connect_to_server(seconds)` | Conexión con rol de cliente para esos programas |
| `net_is_connected()` | Consultar si existe pareja establecida |
| `net_send(data,length)` | 1 si se lanzó el envío; no confirma entrega. Máximo 480 bytes |
| `net_receive(buffer,max_length)` | Longitud recibida, o 0 sin mensaje entregado; descarta uno que no quepa |
| `net_get_local_id()` / `net_get_remote_id()` | Identificadores usados para elegir roles de aplicación |
| `net_connection_lost()` | Detectar falta de recepción durante el plazo del módulo |
| `net_set_log(callback)` | Seleccionar destino de diagnóstico |

La cola contiene hasta 8 mensajes y descarta el más antiguo al llenarse. Una aplicación que necesite todos los bloques de un archivo debe añadir número de secuencia, acuse, reintento y validación de longitud. Un chat o una transferencia de archivos no hereda entrega fiable por llamar a `net_send`.

Una cadena recibida tampoco es automáticamente texto terminado en cero. Reserva espacio para el terminador y colócalo solo tras validar la longitud. Para datos binarios define campos y tamaños explícitos; no envíes estructuras con punteros.

## Lockstep: protocolo del juego sobre el transporte

Fuente: [lockstep.h](../header/lockstep.h) y [lockstep.c](../src/lockstep.c).

`net_init` y `net_shutdown` son la capa del juego; no sustituyen por simple renombrado las funciones del transporte en cualquier ejemplo. `net_find_opponent` y `net_is_player1` resuelven la pareja y asignación.

El bucle recoge entradas con `net_set_local_input`, envía con `net_send_input`, procesa mensajes con `net_poll` y espera hasta `net_has_remote_input`. Obtiene las entradas del frame mediante `net_get_local_input` y `net_get_remote_input`, aplica reglas y avanza con `net_advance_frame`.

Durante la espera debe seguir atendiendo red y sonido. El protocolo actual incluye ocho entradas redundantes por mensaje y un retardo de cinco frames, pero no una recuperación ilimitada de pérdidas ni un reenvío periódico de entradas durante toda espera bloqueada.

Los acuerdos de nivel e intro y `net_wait_together` pertenecen al arranque. Los checksums sirven para comparar estado por frame; no convierten el transporte en fiable. Conserva esas responsabilidades separadas al reutilizar los módulos.

[Índice](README.md) · [Guía práctica](GUIA.md) · [Erratas](ERRATAS.md)
