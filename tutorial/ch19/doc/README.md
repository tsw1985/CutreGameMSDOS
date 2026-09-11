# Capítulo 19 — El checksum: cazar la desincronización

*[English version](README-EN.md)*

**Qué vas a conseguir:** provocar una desincronización a propósito con la tecla
`D` y verla detectada.

```
./launch_game_both.sh
```
y en las dos: `cd tutorial\ch19` y `chap19`.

---

## 1. El bug más desagradable que existe

El capítulo 18 funciona **mientras** las dos máquinas hagan las mismas cuentas.

¿Y si una calcula algo distinto? Eso es una **desincronización**. Y lo peor no es
que ocurra: es **que no se ve**.

- Las dos partidas siguen corriendo
- **Ninguna da ningún error**
- Cada una está convencida de ir bien
- Simplemente **cuentan historias distintas**

El jugador ve al otro tanque disparar al aire, chocar con nada, morir sin
motivo. Y no hay un solo mensaje en ningún sitio.

Compáralo con un fallo normal: si tu programa lee un puntero nulo, se cuelga y
sabes dónde. Una desincronización **no se cuelga**. Sigue.

## 2. Por qué no se puede comparar el estado entero

La idea evidente sería: *"que cada máquina mande su estado completo y se
comparen"*.

No vale, y por dos motivos:

- El estado de la partida son **cientos de bytes** (dos tanques con sus
  posiciones, direcciones, balas, animaciones, marcadores). Mandarlo cada frame
  es justo lo que el lockstep evita.
- Y si lo mandaras, **ya no necesitarías el lockstep**: estarías mandando
  posiciones, que es lo que el capítulo 18 descartó.

Lo que se quiere es **detectar** que hay diferencia, no **transmitir** el estado.
Y para detectar basta con un resumen.

## 3. Qué es un checksum

Un **checksum** es un número pequeño calculado a partir de un montón de datos,
de forma que:

> Si los datos son iguales, el número sale igual.
> Si los datos cambian, el número cambia (casi seguro).

Es un resumen de un solo sentido: del estado sacas el número, pero del número no
puedes reconstruir el estado. Y no hace falta: **solo quieres compararlo**.

Aquí el resumen cabe en **2 bytes** y representa el estado entero de la partida.

## 4. Cómo se genera

```c
static unsigned int compute_state_checksum(){

	unsigned int checksum;

	checksum = 0;

	checksum = checksum + (player1.position_x * 3);
	checksum = checksum + (player1.position_y * 5);
	checksum = checksum + (player1.current_direction * 7);

	checksum = checksum + (player2.position_x * 31);
	checksum = checksum + (player2.position_y * 37);
	checksum = checksum + (player2.current_direction * 41);

	return checksum;

}
```

Coge cada dato del estado, lo multiplica por un número, y suma todo.

### Por qué se multiplica: para que la posición importe

Si sumaras a pelo:

```c
	checksum = player1.position_x + player1.position_y + ...;
```

entonces un tanque en **(100, 50)** y otro en **(50, 100)** darían **el mismo
número**. Dos estados completamente distintos, mismo resumen. El checksum no
vería nada.

Multiplicando cada campo por una constante distinta, cada dato aporta de forma
distinta y el intercambio se nota:

| | Suma a pelo | Con multiplicadores |
|---|---|---|
| Tanque en (100, 50) | 150 | 100·3 + 50·5 = **550** |
| Tanque en (50, 100) | 150 | 50·3 + 100·5 = **650** |
| ¿Se distinguen? | **No** | **Sí** |

### Por qué números primos

Podrías usar 2, 4, 6, 8... pero los múltiplos entre sí crean **coincidencias**:
con multiplicadores 2 y 4, un cambio de +2 en el primer campo se cancela con un
cambio de −1 en el segundo.

Los primos (3, 5, 7, 11, 31, 37, 41…) no tienen factores comunes, así que las
cancelaciones accidentales son mucho más raras.

No es criptografía: es un truco barato que funciona de sobra para esto.

### El desbordamiento no importa

`unsigned int` son 16 bits. Multiplica posiciones por 41 y suma seis campos y te
pasas de 65.535 enseguida.

**Da igual.** El desbordamiento **también es determinista**: las dos máquinas lo
hacen exactamente igual y llegan al mismo número. Lo único que se pierde es que
dos estados distintos podrían dar el mismo resumen por casualidad, y con 65.536
valores posibles eso es raro y, sobre todo, **se detectaría en el siguiente
chequeo**.

## 5. Qué entra y, sobre todo, qué NO

**Entra:** todo lo que las dos máquinas tienen que calcular igual. Posiciones,
direcciones, balas, marcador, el contador de la pausa.

**No entra, y es igual de importante:** nada que sea local de cada máquina.

En el capítulo 22 aparecerán `camera_x` y `camera_y`. Son **legítimamente
distintas** en cada lado, porque cada máquina sigue a su propio tanque. Meterlas
aquí daría una desincronización **falsa** en el frame 1 de todas las partidas.

> **La regla:** si un valor lo calcula el código que dibuja, no entra en el
> checksum.

Lo mismo con `local_player_is_1`: vale 1 en una máquina y 0 en la otra, **por
diseño**.

## 6. El cable trampa del juego real

`src/main.c` mete además dos cosas que **no son estado**:

```c
	checksum = checksum + ((unsigned int)map_width * 73);
	checksum = checksum + ((unsigned int)map_height * 79);
```

`map_width` no cambia nunca durante una partida. ¿Qué hace ahí?

Es una **trampa deliberada**. Si arrancas una máquina con `/bigmap` y la otra
sin él, los mapas son de distinto tamaño, los muros están en sitios distintos, y
las dos simulaciones se separan de una forma imposible de diagnosticar.

Metiéndolo en el checksum, ese caso se convierte en **una línea clara en el log**
en la primera comprobación, en vez de en dos pantallas que divergen sin
explicación.

Coste: dos sumas cada 30 frames. Es de las mejores relaciones coste/beneficio
del proyecto.

## 7. Cómo viaja: gratis

```c
	net_set_local_checksum(compute_state_checksum());
```

`lockstep.c` lo mete **dentro del mismo paquete que las teclas**:

```c
struct lockstep_message {
	unsigned long  base_frame;
	unsigned long  checksum_frame;
	unsigned int   checksum_value;
	unsigned char  count;
	unsigned char  has_checksum;
	unsigned char  inputs[NET_REDUNDANCY];
};
```

Y ahí está el detalle bonito: **ese paquete ya existía**. La cabecera de IPX son
**42 bytes** y este mensaje son **20**. Mandar el checksum **no cuesta ni un
paquete extra ni un frame de latencia**: viaja en un hueco que ya estabas
pagando.

Por eso puede permitirse mandarse a menudo.

## 8. No cada frame: cada 30

```c
	checksum_countdown = NET_CHECKSUM_INTERVAL;   /* 30 */
```

No hace falta comprobar todos los frames. Una desincronización **no se cura
sola**: una vez que las dos partidas se separan, siguen separadas. Así que da
igual enterarse en el frame 100 o en el 130.

Comprobar cada 30 frames (menos de medio segundo) da un diagnóstico
suficientemente preciso y ocupa el campo del paquete solo de vez en cuando.

## 9. El detalle sutil: no se comparan a la vez

Aquí hay algo que no es evidente y que mucha gente implementa mal.

Tú calculas el checksum del frame 100. Lo mandas. **Llega a la otra máquina
varios frames después**, porque el paquete tarda y porque el retardo de entrada
del capítulo 18 desplaza todo.

Cuando llega, la otra máquina **ya va por el frame 104**. No puede compararlo
con "su checksum actual": tiene que compararlo con **el que ella misma sacó en
el frame 100**.

Por eso `lockstep.c` guarda los suyos en un buffer circular:

```c
static unsigned long local_checksum_frame[NET_INPUT_BUFFER_SIZE];
static unsigned int  local_checksum_value[NET_INPUT_BUFFER_SIZE];
static unsigned char local_checksum_valid[NET_INPUT_BUFFER_SIZE];
```

Y al llegar uno de fuera:

```c
static void net_check_remote_checksum(unsigned long frame, unsigned int value){

	/* demasiado viejo: ya no lo tengo guardado */
	if (frame < simulation_frame){ ... }

	/* demasiado nuevo: imposible */
	if (frame >= simulation_frame + NET_INPUT_BUFFER_SIZE){ ... }

	/* el hueco del anillo no es de ese frame */
	if (local_checksum_frame[index] != frame){ ... }

	if (local_checksum_value[index] == value){
		return;                     /* coinciden: todo bien */
	}

	/* NO coinciden */
	sprintf(lockstep_log_text, "NET DESYNC at frame %lu: mine %u theirs %u",
	        frame, local_checksum_value[index], value);
	tanks_log(lockstep_log_text);

	desync_detected = 1;

}
```

Fíjate en las tres comprobaciones de antes de comparar. **Comparar el checksum
de dos frames distintos daría una desincronización falsa siempre**, porque el
estado cambia cada frame. El número de frame viaja con el checksum precisamente
para poder emparejarlos.

## 10. Qué sale en el log

```
NET DESYNC at frame 412: mine 51230 theirs 51237
```

Tres datos, y los tres importan:

| | |
|---|---|
| `frame 412` | **Dónde buscar.** El fallo está en ese frame o justo antes |
| `mine` / `theirs` | Confirman que hay diferencia real, no un paquete corrupto |

Y solo se escribe **una vez**: `desync_detected` se queda a 1 y no vuelve a
avisar. Si no, tendrías miles de líneas idénticas, porque una vez separadas las
partidas ya no vuelven a coincidir nunca.

## 11. Pulsa `D`

```c
		if (keys[KEY_D] && desync_forced == 0){
			player1.position_x = player1.position_x + 1;
			desync_forced = 1;
		}
```

Mueve el tanque **un píxel**, solo en esta máquina. Uno.

La otra no se entera y sigue calculando sin él. A partir de ese frame las dos
partidas son distintas y **a simple vista no se nota todavía**: un píxel no se
ve.

Pero el checksum lo caza en la siguiente comprobación, como muy tarde 30 frames
después.

**Eso es exactamente para lo que sirve:** convertir un fallo invisible en una
línea del log.

## 12. No arregla nada, y está bien así

El checksum **no repara** la desincronización. Las dos partidas siguen
separadas.

Los juegos que sí lo arreglan (mandando el estado completo para resincronizar)
necesitan mucha más maquinaria, y en un juego de dos tanques no compensa.

Aquí el checksum hace una sola cosa: **decirte el frame exacto en que se
rompió**. Y eso es todo lo que necesitas, porque ahí es donde está el fallo.

Sin checksum buscarías a ciegas en toda la partida, con dos máquinas, sin saber
siquiera si el problema es tuyo o de la red.

## 13. Las causas típicas de desincronización

Para cuando te pase de verdad, por orden de probabilidad:

1. **Algo que depende del tiempo real** en lugar del número de frame. Es la
   número uno con diferencia.
2. **Un número aleatorio** sin semilla compartida.
3. **Memoria sin inicializar** que resulta contener cosas distintas en cada
   máquina.
4. **Las dos máquinas cargaron datos distintos**: otro mapa, otro fichero. Para
   esto está el cable trampa del punto 6.
5. **Aplicar tu propia entrada antes que la del otro**, saltándote el retardo.
   Se siente mejor y desincroniza en el primer segundo.

## 14. Experimentos

1. **Pulsa `D` en una sola máquina.** Aviso al salir, y línea en el log.
2. **Púlsalo en las DOS a la vez.** Las dos aplican el mismo +1 → **no hay
   desincronización**. Demuestra que lo que importa es la *diferencia*, no el
   cambio.
3. **Quita los multiplicadores** y suma a pelo. Intercambia a mano las
   posiciones de los dos tanques: el checksum no lo nota.
4. **Mete algo local** en el checksum, por ejemplo `local_player_is_1`.
   Desincronización en el frame 1, siempre. Ese es el fallo del punto 5.
5. **Baja `NET_CHECKSUM_INTERVAL` a 1** en `header/lockstep.h`. Comprueba cada
   frame: se caza antes y el paquete lleva el campo siempre.
6. **Quita el número de frame** de la comparación (compara con el checksum
   actual sin mirar de qué frame es). Desincronización constante y falsa.

## 15. Lo que hay que llevarse

| | |
|---|---|
| Una desincronización **no se ve**: las dos partidas siguen tan tranquilas | |
| Un **checksum** es un resumen pequeño que solo sirve para comparar | 2 bytes para todo el estado |
| **Se multiplica por primos** para que el orden y la posición importen | Si no, (100,50) = (50,100) |
| El desbordamiento da igual: **también es determinista** | |
| **Nada local dentro**: la cámara, nunca | Daría un falso positivo siempre |
| Viaja **gratis** dentro del paquete de teclas | La cabecera ya costaba 42 bytes |
| **Se comparan por número de frame**, no al vuelo | El paquete llega tarde |
| No arregla; **dice el frame exacto** | Y con eso basta |

---

**Anterior:** [Capítulo 18](../../ch18/doc/README.md) ·
**Siguiente:** [Capítulo 20 — El mundo deja de ser la pantalla](../../ch20/doc/README.md)
