# Laboratorio de cámara y mapas, con respuestas

Resuelve primero en papel. Estas pruebas no requieren ejecutar DOS ni modificar el proyecto. Después puedes reproducir los casos en tu copia de los capítulos 20–24 de `tutorial/`. Todas las dimensiones se expresan en píxeles; los intervalos finales son excluidos.

## 1. La resta y su inversa

Mundo 640 × 400, cámara `(83,117)`, tanque en `(260,230)`.

1. ¿Dónde aparece?
2. ¿Qué píxel del mundo ves en pantalla `(7,9)`?
3. Si la cámara pasa a `(85,117)`, ¿ha cambiado alguna colisión del tanque?

**Respuesta:** `(177,113)`; mundo `(90,126)`; no cambia ninguna por mover solo la cámara. El tanque aparece ahora en `(175,113)`.

## 2. Ventana por filas

Mundo de anchura 640, cámara `(37,103)`. Calcula primera dirección de origen de las filas de pantalla 0, 1 y 199, y el último byte leído.

**Respuesta:** 65957, 66597 y 193317. El último es 193636. Entre comienzos de fila hay 640 bytes; en destino hay 320. Ya la primera dirección excede 65535: una prueba que use solo la esquina superior izquierda no descubre todos los errores de 16 bits.

## 3. Los cuatro extremos

Completa el último origen de cada ventana:

| Cámara | Primer origen | Último origen |
|---|---:|---:|
| `(0,0)` | 0 | 127679 |
| `(320,0)` | 320 | 127999 |
| `(0,200)` | 128000 | 255679 |
| `(320,200)` | 128320 | 255999 |

**Por qué sirve:** si alguna dirección excede 255999, hay un error de límites o de fila final. Si el último destino excede 63999, el fallo está en la ventana de pantalla y no en el tamaño del mundo.

## 4. Recorte de un tanque

Para 18 × 18, completa la región local visible:

| Destino | X local | Y local | Píxeles geométricos |
|---|---|---|---:|
| `(-9,-4)` | `[9,18)` | `[4,18)` | 126 |
| `(315,198)` | `[0,5)` | `[0,2)` | 10 |
| `(-18,40)` | Vacía | — | 0 |
| `(319,199)` | `[0,1)` | `[0,1)` | 1 |

**Pregunta adicional:** ¿126 significa 126 escrituras? **Respuesta:** como máximo; la transparencia puede descartar píxeles dentro de ese rectángulo.

## 5. Cámara con historia

Márgenes 100/70, tanque 18 × 18. Cámara inicial `(100,50)`. Sigue sucesivamente estos objetivos:

| Mundo del tanque | Pantalla antes de seguir | Cámara final |
|---|---|---|
| `(300,160)` | `(200,110)` | `(100,50)` |
| `(304,164)` | `(204,114)` | `(102,52)` |
| `(302,162)` | `(200,110)` | `(102,52)` |
| `(200,120)` | `(98,68)` | `(100,50)` |

**Respuesta razonada:** el segundo paso sobresale dos píxeles por derecha y abajo; el tercero vuelve al interior y deja quieta la cámara; el cuarto sobresale dos por izquierda y arriba.

## 6. Clamp domina a la zona muerta

Cámara inicial `(0,0)`, tanque `(16,16)`. Follow propone `(-84,-54)`.

**Respuesta:** clamp deja `(0,0)` y el tanque sigue apareciendo en `(16,16)`. No existe mapa a la izquierda ni encima. La zona muerta no puede ser una obligación absoluta en el borde.

Repite con tanque `(622,382)` y cámara `(320,200)`: follow propone `(420,270)`, clamp conserva `(320,200)`, pantalla `(302,182)`. El sprite termina exactamente en la esquina inferior derecha.

## 7. Snap y mapa pequeño

1. Haz snap a P1 `(166,299)` en mundo grande.
2. Haz snap a `(110,164)` en mundo 320 × 200.
3. ¿Basta el mismo clamp para dibujar un mundo 200 × 100?

**Respuesta:** `(15,200)`; `(0,0)`; no. Para el mundo menor necesitas una copia recortada y rellenar el espacio que sobra. El bucle actual copia siempre 320 × 200.

## 8. Empaquetar una fila

Mundo de 16 × 1. Paredes en X=0, 3, 7, 8 y 15.

**Respuesta:** bytes `0x89, 0x81`. Consultar X=7 usa byte 0 y máscara 128; consultar X=8 usa byte 1 y máscara 1. Este par descubre errores al cruzar un límite de byte.

Para anchura 640, `(639,399)` usa byte 31999 y máscara 128. Consultar `(640,399)` devuelve pared por estar fuera, antes de buscar un byte.

## 9. La misma partida con dos cámaras

Ambos equipos guardan P1 `(400,200)` y P2 `(450,220)`. A tiene cámara `(100,50)` y B `(300,100)`.

**Respuesta:** P1 aparece en `(300,150)` en A y `(100,100)` en B. La distancia del radar es `50+20/2=60`; porcentaje `100-floor(6000/813)=93` en ambos. Los campos compartidos del checksum deben coincidir. Las capturas no tienen por qué coincidir.

## 10. Propiedades que comprueban muchos casos

Estas pruebas se llaman propiedades porque valen para familias de ejemplos:

- **Ida y vuelta:** transformar mundo a pantalla y sumar la cámara recupera el mundo.
- **Traslación conjunta:** sumar el mismo desplazamiento a objeto y cámara conserva la posición visible, mientras todo siga dentro de rangos válidos.
- **Estabilidad:** aplicar follow dos veces con el mismo objetivo no cambia la cámara tras la primera corrección y clamp.
- **Identidad:** mundo 320 × 200 y cámara cero produce la misma imagen que copiar el fondo completo.
- **Límites:** todo píxel efectivamente escrito por clipping cae en `[0,64000)`.
- **Independencia:** cambiar solo cámara o tema no cambia las paredes consultadas ni las entradas simuladas.

## Diagnóstico por síntomas

| Síntoma | Primera hipótesis que comprobar |
|---|---|
| Imagen inclinada o filas desplazadas | Stride, relleno BMP, offset 1078 |
| Falla al bajar a otra zona del mapa | Multiplicación de 16 bits o puntero sin normalización |
| Sprites desaparecen enteros al entrar por un borde | Rechazo de negativos en lugar de clipping |
| Bala pegada a la pantalla | Falta restar cámara en ese dibujo |
| Colisión cambia al mover la ventana | Se ha usado pantalla al consultar la máscara |
| Primer frame mira una esquina equivocada | Snap antes de establecer dimensiones |
| Radar salta de valor según quién esté a la izquierda | Resta unsigned antes del cast |
| Colores extraños y fallos aparentemente aleatorios | Contrato de paleta y reserva insuficiente; ver erratas |

[Volver al curso](README.md) · [Teoría de cámara](ch25.md) · [Erratas](ERRATAS.md)
