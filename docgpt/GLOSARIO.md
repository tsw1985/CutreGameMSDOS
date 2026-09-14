# Glosario del curso

| Término | Significado en este juego |
|---|---|
| AABB | Rectángulo alineado con los ejes. Permite detectar solapamiento comparando extremos. |
| Array | Elementos consecutivos del mismo tipo; una imagen es un array de índices de color. |
| Bit | Una decisión binaria. En la máscara, 1 significa pared. |
| Bitmap | Imagen almacenada mediante píxeles. El archivo BMP añade cabeceras y paleta. |
| Buffer | Memoria reservada para datos en tránsito o trabajo: imagen, sonido o paquetes. |
| Byte | Ocho bits. En modo 13h identifica el color de un píxel. |
| C89 | Versión de C usada como base; las declaraciones se colocan al comienzo de los bloques. |
| Cámara | Esquina del mundo que corresponde a la esquina de pantalla. |
| Callback | Función entregada a otro módulo para que la invoque; la intro usa uno para atender la red. |
| Checksum | Resumen pequeño para detectar diferencias. Dos estados distintos pueden coincidir. |
| Clamp | Limitar un valor a un intervalo: impedir que la cámara salga del mapa. |
| Clipping | Recortar el dibujo a una región válida sin mover el objeto. |
| Colisión | Decisión de que un movimiento o impacto encuentra un obstáculo u otro objeto. |
| DAC | Conversor y registros de color de VGA; cada componente de paleta usa 6 bits en este modo. |
| Datagramas | Mensajes separados sin garantía general de entrega u orden. IPX los transporta. |
| Determinismo | Mismo estado inicial y mismas entradas producen el mismo estado posterior. |
| DMA | Transferencia de memoria a un dispositivo sin copiar cada byte mediante el bucle de CPU. |
| DSP | Procesador/controlador de sonido de Sound Blaster al que se envían comandos. |
| ECB | Estructura que describe al driver IPX un envío o recepción; no es una cabecera transmitida. |
| Far | Puntero con segmento y desplazamiento; no basta para recorrer cualquier array mayor de 64 KB. |
| Flanco | Cambio de un estado a otro. El disparo usa la transición de tecla libre a pulsada. |
| Fotograma | Imagen presentada. No confundas su ritmo con una garantía de actualizaciones por segundo. |
| Frame de simulación | Número de paso compartido que identifica entradas y estado en lockstep. |
| Heap | Memoria disponible para reservas dinámicas; importa tanto el total como sus huecos contiguos. |
| HUD | Datos dibujados sobre la pantalla, como el radar, independientes del desplazamiento del mundo. |
| Huge | Puntero de Borland cuya aritmética normaliza direcciones para recorrer objetos grandes. |
| Índice de color | Número 0–255 que selecciona una entrada de la paleta, no un RGB independiente. |
| IPX | Protocolo de red empleado por el juego; DOSBox puede transportarlo a través de UDP. |
| IRQ / ISR | Interrupción hardware / función que la atiende. Debe ser breve y respetar el dispositivo. |
| Lockstep | Simular un paso cuando están disponibles las entradas necesarias de ambos jugadores. |
| Máscara | Datos que seleccionan o describen partes de otros datos; aquí hay máscaras de bits de entrada y paredes. |
| Modelo de memoria | Convenciones del compilador sobre segmentos, punteros y ubicación de datos/código. |
| Muestra de audio | Valor de amplitud en un instante. Aquí salida PCM unsigned de 8 bits, silencio 128. |
| Muestreo inverso | Recorrer el destino y buscar qué posición de origen le corresponde. |
| Mundo | Espacio donde se simulan posiciones y paredes, aunque no se vean. |
| Paleta | Tabla que convierte índices de píxel en colores. 256 entradas × 3 componentes. |
| Pitch / stride | Distancia entre comienzos de filas. En el mundo grande son 640 bytes. |
| Protocolo | Reglas sobre mensajes, estados, orden de intercambio y respuesta a fallos. |
| Punto fijo | Representar fracciones mediante enteros escalados. 384 con escala 256 representa 1,5. |
| Response file | Archivo con argumentos del enlazador para evitar el límite de longitud de la línea DOS. |
| Retrazo vertical | Intervalo del barrido de vídeo que se usa para sincronizar presentación y paletas. |
| Ring buffer | Array reutilizado cíclicamente; necesita distinguir el dato actual del antiguo de la misma casilla. |
| Scan code | Código de tecla del teclado; no es necesariamente un carácter ASCII. |
| Segmento | Base de direccionamiento del modo real; se combina con un desplazamiento. |
| Snap | Colocación inmediata de la cámara sobre el objetivo, limitada por los bordes del mundo. |
| Socket IPX | Identificador de servicio IPX, aquí 0x869C; diferente del puerto UDP del emulador. |
| Sprite | Imagen pequeña que se compone sobre el fondo, con un índice transparente. |
| Streaming | Leer datos durante su consumo en lugar de cargarlos completos de antemano. |
| Tick BIOS | Unidad del contador del reloj BIOS, aproximadamente 1/18,2 segundos. |
| Transparencia | Regla de no escribir ciertos píxeles de origen, aquí los de índice 0. |
| VGA modo 13h | Pantalla de 320 × 200 y 256 colores con 64000 bytes de píxeles. |
| Voz | Una reproducción activa de una muestra. Dos voces pueden usar el mismo sonido cargado. |
| Zona muerta | Región donde el objetivo puede moverse sin desplazar la cámara. |

[Índice del curso](README.md)
