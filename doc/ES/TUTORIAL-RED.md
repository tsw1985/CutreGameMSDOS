# Tutorial: usar el módulo de red en cualquier proyecto DOS

`src/net.c` es un **módulo independiente**. No sabe nada de tanques, ni de
juegos, ni de lo que le estás mandando: le das unos bytes y salen por el otro
lado.

Este documento explica cómo usarlo en un programa nuevo: un chat, enviar un
fichero, dos programas hablando entre ellos, u otro juego. Si lo que quieres
es entender cómo funciona por dentro (IPX, los ECB, el lockstep), eso está en
[MANUAL-RED.md](MANUAL-RED.md).

---

## 1. Qué copio a mi proyecto

**Dos ficheros, y nada más:**

```
src/net.c
header/net.h
```

No dependen de ningún otro fichero de este repositorio. Sus únicas
dependencias son cabeceras estándar de Turbo C:

```c
#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <bios.h>
#include <string.h>
#include <stdlib.h>
```

Para compilarlo, una línea en tu Makefile como cualquier otra:

```
tcc -c -O2 -mh -Iheader -obin\net.obj src\net.c
```

Y añades `bin\net.obj` a la lista de enlazado.

> **`src/lockstep.c` NO es parte de la librería.** Es la forma que tiene este
> juego de usarla, y es un fichero aparte precisamente para que copiar `net.c`
> a un chat no se lleve de propina los frames, los checksums y los buffers
> circulares. Ignóralo salvo que estés haciendo un juego de acción para dos;
> la sección 10 explica para qué sirve.

---

## 2. Los cinco pasos

Siempre son estos cinco, en este orden. No hay más.

```c
#include "header\net.h"

int main()
{
    char mensaje[100];
    int  longitud;

    /* ---- PASO 1: encender la red ---- */
    if (net_start() == 1){

        /* ---- PASO 2: encontrar a la otra máquina ---- */
        net_connect_to_server(30);

    }

    while (funcionando){

        /* ---- PASO 3: una vez por bucle, SIEMPRE ---- */
        net_update();

        /* ---- PASO 4: decir algo, y escuchar ---- */
        if (ha_pasado_algo){
            net_send("hola", 4);
        }

        longitud = net_receive(mensaje, sizeof(mensaje));

        while (longitud > 0){
            /* ...haz algo con los primeros `longitud` bytes... */
            longitud = net_receive(mensaje, sizeof(mensaje));
        }

    }

    /* ---- PASO 5: apagarla ---- */
    net_end();

    return 0;
}
```

Y ya está. Eso es un programa que habla con otra máquina.

### Los cinco pasos, uno a uno

**`net_start()`** busca el driver de IPX y abre nuestro socket. Devuelve **1
si hay red y 0 si no la hay** (no hay driver cargado, o ya hay otra copia de
tu programa con el socket cogido).

Lo importante: si devuelve 0, **no tienes que hacer nada especial**. Todas las
demás funciones comprueban por su cuenta y no hacen nada, calladas. Tu
programa funciona exactamente igual, él solo, sin un `if` extra repartido por
tu código.

No conecta con nadie. Con quién hablas es el paso 2.

**`net_update()` NO ES OPCIONAL.** Es el de los cinco que se te puede olvidar
y rompértelo todo. Es lo **único** que mueve datos hacia dentro: mientras no
se llame, no llega nada, por mucho que el otro lado mande.

Llámalo también dentro de **cualquier bucle que espere** algo. Es barato: casi
siempre mira, ve que el driver no tiene nada y vuelve.

**`net_send()`** le pasa tus bytes al driver y vuelve enseguida. **Nunca
espera**, así que puede devolver 0 queriendo decir "no enviado". Mira la
sección 5.

**`net_receive()`** te da **un** mensaje y devuelve lo largo que era, o **0**
cuando no hay nada esperando. Llámalo en bucle hasta que devuelva 0: pueden
llegar varios mensajes entre dos vueltas del tuyo.

**`net_end()`** es obligatorio antes de salir. Si te lo saltas, el driver se
queda con nuestro socket y nuestros buffers, y la siguiente ejecución no puede
abrir el mismo socket.

---

## 3. Conectar: ¿quién es el servidor?

Aquí viene la parte honesta, y merece treinta segundos de tu tiempo.

**IPX no tiene servidores ni clientes.** No hay conexión que abrir ni conexión
que aceptar. Se parece más a gritar en una habitación que a descolgar un
teléfono.

Pero casi siempre quieres *pensar* en esos términos, así que la librería te los
da como **convenio**. La única diferencia entre los dos es **quién grita y
quién escucha**:

| | Lo que hace en realidad |
|---|---|
| `net_wait_for_client(30)` | Se calla y espera. Contesta a quien aparezca |
| `net_connect_to_server(30)` | Grita cuatro veces por segundo hasta que alguien contesta |
| `net_find_peer(30)` | Los dos gritan Y contestan. Nadie manda |

El número son **cuántos segundos** insistir antes de rendirse. Los tres
devuelven **1 cuando han encontrado a la otra máquina** y 0 si se agotó el
tiempo o el usuario pulsó una tecla.

**Nadie teclea una dirección en ningún sitio.** Se encuentran por broadcast.
Eso es sinceramente mejor que TCP/IP, donde alguien tiene que saberse una IP.

Una vez se han encontrado, **los dos lados son completamente idénticos**.
Cualquiera puede enviar cuando le apetezca. "Servidor" y "cliente" sólo
describían los dos primeros segundos.

### ¿Cuál uso?

- Si haces una **herramienta**, donde una máquina es claramente a la que se
  conectan (un receptor de ficheros, un servidor de impresión, un chat con
  anfitrión): usa `net_wait_for_client()` en ese lado y
  `net_connect_to_server()` en el otro.
- Si haces un **juego**, donde las dos copias son el mismo programa y ninguna
  tiene motivo para mandar: usa `net_find_peer()` en los dos. Así ningún
  jugador tiene que acordarse de arrancar primero.

Los tres imprimen lo que están haciendo, así que **llámalos antes de pasar a
modo gráfico**.

### ¿Y quién decide las cosas?

Antes o después uno de los dos tiene que ser "el primero" para algo: quién es
el jugador 1, quién envía primero, quién elige el nivel. Hay un truco para
esto que no cuesta ni un paquete:

```c
if (net_get_local_id() < net_get_remote_id()){
    /* voy yo primero */
}else{
    /* van ellos primero */
}
```

Cada copia elige un número al azar en `net_start()`, y los dos números se
intercambian al emparejarse. **Las dos máquinas comparan los mismos dos
números y llegan solas a la misma respuesta**, sin negociar nada y sin enviar
nada. Así decide este juego quién lleva cada tanque.

---

## 4. Lo que viaja: mensajes, no un flujo

Ésta es la idea más importante de toda la librería.

**Un `net_send()` es un MENSAJE.** Si envías 3 bytes, el otro lado recibe
exactamente 3 bytes en un `net_receive()`.

```c
    net_send("abc", 3);
    net_send("de", 2);
```

llega como **`abc`** y luego **`de`**. Nunca como `abcde`, y nunca como `ab`
más `cde`.

Si has usado sockets TCP, esto es justo lo contrario de lo que estás
acostumbrado, y es **mucho más fácil**: no hay flujo que volver a trocear, ni
prefijo de longitud que escribir, ni medio mensaje sobrante que recordar para
la próxima vez.

Así que la forma natural de usarlo es enviar un `struct`:

```c
struct mi_mensaje {
    unsigned char  que_ha_pasado;
    unsigned int   x;
    unsigned int   y;
};

    struct mi_mensaje salida;
    struct mi_mensaje entrada;

    salida.que_ha_pasado = 7;
    salida.x = 100;
    salida.y = 50;

    net_send(&salida, sizeof(struct mi_mensaje));

    /* ...y en la otra máquina... */

    if (net_receive(&entrada, sizeof(struct mi_mensaje)) == sizeof(struct mi_mensaje)){
        /* entrada.x y entrada.y están listos para usar */
    }
```

Los dos programas los compila el mismo Turbo C con las mismas opciones, así
que la estructura tiene la misma disposición en los dos lados y no hay nada
que convertir.

**Comprueba lo que devolvió `net_receive()`.** Si no es el tamaño que
esperabas, hay alguien más usando el socket, o has enviado otro tipo de
mensaje. Leer los campos de algo que nunca fue tuyo es como se consigue un
bug de una semana.

### Si envías más de un tipo de mensaje

Pon un byte de tipo delante y míralo primero. Exactamente lo que hace la
librería por dentro:

```c
#define MSG_CHAT     1
#define MSG_FICHERO  2
#define MSG_ADIOS    3

struct mi_mensaje {
    unsigned char tipo;
    unsigned char datos[100];
};
```

### Los límites

| | |
|---|---|
| Máximo en un mensaje | **`NET_MAX_DATA`, 480 bytes** |
| Mensajes esperando a ser recogidos | **`NET_QUEUE_SIZE`, 8** |
| Lo que cuesta en memoria | unos **6 KB**, hagas lo que hagas |

480 no es un número arbitrario: IPX garantiza 546 bytes para el paquete
entero, su propia cabecera se lleva 30, y nuestro sobre 12.

Si pides enviar más, `net_send()` **se niega y devuelve 0** en vez de partirte
los datos por la mitad sin decir nada. Trocéalo tú, como hace la sección 6 con
un fichero.

---

## 5. Lo que NO tienes

La librería no te da más de lo que da IPX, y ser claro con eso vale más que
una mentira cómoda:

| | |
|---|---|
| ¿Llega? | **No garantizado.** Un mensaje se puede perder y nadie se entera |
| ¿En orden? | **No garantizado.** Dos pueden llegar al revés |
| ¿Hay conexión? | **No.** "Conectado" sólo significa que se han encontrado |

En una red local, y en DOSBox, las pérdidas son raras. Que eso importe o no
depende enteramente de lo que estés enviando:

**No importa** para nada que envíes una y otra vez: una posición, un marcador,
las teclas pulsadas este frame. El siguiente lo arregla. Esperar a una
retransmisión sería peor que la pérdida.

**Importa muchísimo** para cualquier cosa que se envíe una sola vez: un
fichero, un "se acabó la partida", una línea de chat. Perderla es perderla
para siempre.

Además hay **otras dos formas de que un mensaje se pierda**, y éstas son cosa
tuya:

**`net_send()` devolvió 0.** El driver seguía ocupado con el paquete anterior.
En un bucle rápido esto pasa de vez en cuando y es perfectamente normal:

```c
    if (net_send(&mensaje, sizeof(mensaje)) == 0){
        /* no enviado. Vuelve a intentarlo en la siguiente vuelta si importa */
    }
```

**No has leído lo bastante deprisa.** Sólo se guardan `NET_QUEUE_SIZE`
mensajes esperando. Si se acumulan más, se tira el **más viejo**, porque en un
enlace que se queda atrás las noticias que interesan son las nuevas. Lee tus
mensajes en bucle, en cada vuelta, y esto no pasa nunca.

### Cómo arreglarlo cuando sí importa

La cura es la misma que usa TCP, y aquí son unas veinte líneas. **Numera tus
mensajes y haz que el otro lado diga lo que ha recibido:**

```c
struct mi_mensaje {
    unsigned int  numero;              /* 0, 1, 2, ... */
    unsigned char datos[400];
};

struct mi_ack {
    unsigned int  numero;              /* "éste lo tengo" */
};
```

**El que envía** guarda una copia del último mensaje enviado, lo reenvía cada
poco, y sólo pasa al siguiente cuando llega el acuse de ese número:

```c
    while (queda_algo_por_enviar){

        net_update();

        /* reenvía unas cuatro veces por segundo hasta que digan que lo tienen */
        if (biostime(0, 0L) >= siguiente_reenvio){
            siguiente_reenvio = biostime(0, 0L) + 5L;
            net_send(&salida, sizeof(salida));
        }

        if (net_receive(&ack, sizeof(ack)) == sizeof(ack)){
            if (ack.numero == salida.numero){
                /* recibido. Rellena el siguiente mensaje y sigue */
                salida.numero = salida.numero + 1;
                ...
            }
        }

        if (net_connection_lost() == 1){
            break;
        }

    }
```

**El que recibe** acusa todo e ignora lo que ya ha visto:

```c
    if (net_receive(&entrada, sizeof(entrada)) == sizeof(entrada)){

        ack.numero = entrada.numero;
        net_send(&ack, sizeof(ack));       /* siempre, incluso si es repetido */

        if (entrada.numero == esperado){
            /* nuevo. Úsalo */
            esperado = esperado + 1;
        }

        /* si no es el esperado es un repetido: acusado y tirado. Eso es lo
           que hace que perder un acuse no tenga consecuencias */

    }
```

Eso cubre un mensaje perdido (se reenvía), un acuse perdido (el repetido se
vuelve a acusar) y mensajes que llegan desordenados (sólo se usa el número
esperado). Es lento — un mensaje en el aire cada vez — pero para enviar un
fichero por una red local va perfectamente, y es *correcto*, que importa más.

---

## 6. Un programa completo: un chat entre dos máquinas

Esto compila y funciona tal cual. Ejecútalo en dos máquinas: en una escribes
`chat s`, en la otra `chat c`.

```c
#include <stdio.h>
#include <conio.h>
#include <string.h>
#include "header\net.h"

#define LONGITUD_LINEA 		80

int main(int argc, char *argv[])
{
    char linea_enviada[LONGITUD_LINEA];
    char linea_recibida[LONGITUD_LINEA];
    int  longitud_tecleada;
    int  longitud_recibida;
    int  tecla;
    int  conectado;

    if (net_start() == 0){
        printf("No hay driver IPX. Carga uno y vuelve a probar.\n");
        return 1;
    }

    if (argc > 1 && argv[1][0] == 's'){
        conectado = net_wait_for_client(60);
    }else{
        conectado = net_connect_to_server(60);
    }

    if (conectado == 0){
        net_end();
        return 1;
    }

    printf("Escribe y pulsa ENTER. ESC para salir.\n\n");

    longitud_tecleada = 0;

    while (1){

        /* SIEMPRE, en cada pasada */
        net_update();

        /* ---- ¿han dicho algo? ---- */

        longitud_recibida = net_receive(linea_recibida, LONGITUD_LINEA - 1);

        while (longitud_recibida > 0){

            linea_recibida[longitud_recibida] = '\0';
            printf("\nELLOS: %s\n", linea_recibida);

            longitud_recibida = net_receive(linea_recibida, LONGITUD_LINEA - 1);

        }

        /* ---- ¿estamos escribiendo algo? ---- */

        if (kbhit()){

            tecla = getch();

            if (tecla == 27){                            /* ESC */
                break;
            }

            if (tecla == 13){                            /* ENTER */

                if (longitud_tecleada > 0){

                    if (net_send(linea_enviada, longitud_tecleada) == 0){
                        printf("\n(ocupado, no enviado)\n");
                    }

                    longitud_tecleada = 0;
                    printf("\n");

                }

            }else{

                if (longitud_tecleada < LONGITUD_LINEA - 1){
                    linea_enviada[longitud_tecleada] = (char)tecla;
                    longitud_tecleada = longitud_tecleada + 1;
                    putch(tecla);
                }

            }

        }

        /* ---- ¿se han ido? ---- */

        if (net_connection_lost() == 1){
            printf("\n\nLa otra máquina se ha ido.\n");
            break;
        }

    }

    net_end();

    return 0;
}
```

Fíjate en tres cosas:

1. **`net_update()` está fuera de todos los `if`.** Tiene que ejecutarse en
   cada pasada.
2. **`net_receive()` está en un `while`, no en un `if`.** Pueden llegar dos
   líneas entre dos pulsaciones de tecla.
3. **`net_connection_lost()` es lo que te saca.** Sin él el programa se
   quedaría ahí para siempre si la otra máquina se cuelga.

Un chat es el caso donde sí deberías pensar en la sección 5: una línea perdida
se pierde en silencio. En una red local no va a pasar prácticamente nunca, y
si te preocupa, la numeración de la sección 5 es la respuesta.

---

## 7. Enviar un fichero

La misma librería, nada nuevo, sólo las piezas juntas. **Trocea el fichero en
mensajes, numéralos, y avisa cuando se acaba.**

```c
#define TROZO_FICHERO 	400

#define MSG_NOMBRE 		1
#define MSG_TROZO 		2
#define MSG_FIN 		3

struct mensaje_fichero {
    unsigned char  tipo;
    unsigned char  relleno;
    unsigned int   numero;
    unsigned int   longitud;
    unsigned char  datos[TROZO_FICHERO];
};
```

**Enviar**, a grandes rasgos:

```c
    fichero = fopen("imagen.bmp", "rb");

    /* primero el nombre, para que el otro lado sepa qué está escribiendo */
    mensaje.tipo = MSG_NOMBRE;
    strcpy((char *)mensaje.datos, "imagen.bmp");
    mensaje.longitud = strlen("imagen.bmp");
    enviar_y_esperar_acuse(&mensaje);

    /* y luego el fichero, de 400 en 400 bytes */
    mensaje.numero = 0;

    leidos = fread(mensaje.datos, 1, TROZO_FICHERO, fichero);

    while (leidos > 0){

        mensaje.tipo     = MSG_TROZO;
        mensaje.longitud = leidos;

        enviar_y_esperar_acuse(&mensaje);

        mensaje.numero = mensaje.numero + 1;

        leidos = fread(mensaje.datos, 1, TROZO_FICHERO, fichero);

    }

    mensaje.tipo = MSG_FIN;
    enviar_y_esperar_acuse(&mensaje);

    fclose(fichero);
```

donde `enviar_y_esperar_acuse()` es el bucle de reenvío de la sección 5.
**Úsalo aquí.** Un fichero con un agujero en medio no es un fichero, y éste es
exactamente el caso donde "un mensaje se puede perder" deja de ser aceptable.

**Recibir** es la imagen especular: acusa todo, escribe a disco sólo los
números que no hayas visto antes, y para con `MSG_FIN`.

A 400 bytes por mensaje y un mensaje en el aire cada vez, un fichero del
tamaño de un disquete tarda un rato. Si eso te molesta, envía varios y acusa
el número más alto recibido en orden — eso es una ventana deslizante, y es de
donde saca TCP su velocidad. Pero primero haz que funcione el sencillo.

---

## 8. Referencia completa

| Función | Qué hace |
| --- | --- |
| `net_start()` | Enciende la red. **1 = hay red, 0 = no la hay** |
| `net_end()` | Devuelve el socket. **Obligatorio antes de salir** |
| `net_update()` | **Una vez por bucle, siempre.** Sin esto no llega nada |
| `net_wait_for_client(segundos)` | SERVIDOR: espera a alguien. **1 = ha llegado** |
| `net_connect_to_server(segundos)` | CLIENTE: busca un servidor. **1 = encontrado** |
| `net_find_peer(segundos)` | Los dos hacen ésta, nadie manda |
| `net_is_connected()` | 1 cuando ya se sabe quién es la otra máquina |
| `net_send(datos, longitud)` | Envía un mensaje. **1 = entregado, 0 = no enviado** |
| `net_receive(buffer, max)` | Coge un mensaje. **Devuelve su longitud, 0 = nada** |
| `net_get_local_id()` | Nuestro número al azar de esta ejecución |
| `net_get_remote_id()` | El suyo. Compara los dos para decidir cosas gratis |
| `net_connection_lost()` | 1 cuando no llega nada desde hace 10 segundos |
| `net_set_log(funcion)` | Dónde informar de los problemas. Opcional |

Constantes que puedes cambiar en `net.h`:

| Constante | Valor | Qué es |
| --- | --- | --- |
| `NET_MAX_DATA` | 480 | Máximo de bytes en un mensaje |
| `NET_QUEUE_SIZE` | 8 | Mensajes guardados esperando a `net_receive()` |
| `NET_TIMEOUT_SECONDS` | 10 | Silencio antes de que `net_connection_lost()` se rinda |

Y dentro de `net.c`, si alguna vez lo necesitas:

| Constante | Valor | Qué es |
| --- | --- | --- |
| `NET_SOCKET_NUMBER` | 0x869C | **Cámbialo** para que dos programas tuyos no se oigan |
| `NET_LISTEN_ECB_COUNT` | 4 | Buffers de recepción dejados al driver |

Subir `NET_MAX_DATA` por encima de 480 es el único cambio que te puede morder:
el paquete pasaría de los 546 bytes que IPX garantiza, y que siga funcionando
depende del driver.

---

## 9. Enterarte de lo que pasa (opcional)

La librería está **callada por defecto**, salvo las funciones de emparejar,
que imprimen a propósito y así lo dicen.

Es deliberado: un programa en modo gráfico no puede escribir en pantalla. Así
que la librería no decide por ti dónde va el texto; se lo dices tú:

```c
void mi_log(char *mensaje)
{
    FILE *f;

    f = fopen("debug.log", "a");

    if (f != NULL){
        fprintf(f, "%s\n", mensaje);
        fclose(f);
    }
}

    /* antes de net_start() */
    net_set_log(mi_log);
```

A partir de ahí te cuenta cosas como:

```
NET sizes: ecb=42 header=30 overhead=42 (want 42/30/42)
NET: IPX driver entry at 0300:0010
NET: node 000000000001 id 1839472
NET: paired with id 993822 node 000000000002
NET: sent 4211, received 4198, dropped 0
```

**Esa primera línea merece que la leas.** Si esos tres números no son 42, 30 y
42, el compilador ha rellenado las estructuras e IPX está leyendo cada campo
del sitio equivocado. Compila con `-a-` (alineación a byte), que es lo que
Turbo C hace por defecto y lo que usa el Makefile de aquí.

Si nunca llamas a `net_set_log()` no pasa nada malo: sigue funcionando,
calladito. Y esto es también lo que hace que `net.c` se pueda copiar — no
tiene que incluir ningún fichero tuyo para escribir en tu log.

---

## 10. Errores típicos

| Síntoma | Casi seguro que |
| --- | --- |
| No llega nunca nada | No estás llamando a `net_update()` en algún bucle que espera |
| Sólo llega el primer mensaje | `net_receive()` está en un `if` y debería estar en un `while` |
| `net_start()` devuelve 0 | No hay driver IPX, u otra copia tiene el socket |
| El emparejado se agota | La otra máquina no está corriendo, o DOSBox no está en la misma red IPX |
| Se pierden mensajes de vez en cuando | Normal. Mira lo que devolvió `net_send()` y lee la sección 5 |
| La línea de tamaños no dice 42/30/42 | Se están rellenando las estructuras. Compila con `-a-` |
| Basura en los campos que lees | Comprueba la longitud que devolvió `net_receive()` antes de usarla |
| Se queda esperando para siempre | No estás comprobando `net_connection_lost()` |
| Dos programas tuyos se interfieren | Dale a cada uno un `NET_SOCKET_NUMBER` distinto |

### Montarlo en DOSBox

En una máquina:

```
ipxnet startserver
```

En la otra:

```
ipxnet connect 192.168.1.50
```

Ése es el túnel IPX-sobre-UDP del propio DOSBox y ocurre **antes** de que tu
programa arranque. No tiene nada que ver con `net_wait_for_client()` y
`net_connect_to_server()`, que son la idea que tiene tu programa de quién
manda — puedes perfectamente poner el servidor de DOSBox en la máquina que
luego hace de cliente. Hay más sobre esto en
[NETWORK-TESTING.md](NETWORK-TESTING.md).

En máquinas reales cargas un driver de verdad (`LSL` + `IPXODI`, o el puente
del packet driver) y no hay nada que conectar: el mismo `.exe`, sin
recompilar.

---

## 11. Y cómo lo usa el juego de este repositorio

El juego **no** usa `net.c` directamente. Tiene una capa propia,
`src/lockstep.c`, encima, y merece la pena entenderla porque es un buen
ejemplo de para qué sirve la librería.

Un juego de acción para dos no puede limitarse a enviar posiciones: acabarían
sin ponerse de acuerdo. Así que éste envía **un byte de teclas pulsadas por
jugador y por frame**, las dos máquinas ejecutan el juego entero, y calculan
los mismos píxeles. Todo lo que `lockstep.c` añade encima de `net.c` está al
servicio de eso:

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

Veinte bytes, enviados setenta veces por segundo con `net_send()`, recogidos
con `net_receive()`. **`net.c` no mira nunca dentro.**

Hay dos ideas ahí que merece la pena robar:

**Redundancia en vez de retransmisión.** Cada paquete lleva los **últimos 8
frames** de teclas, no sólo éste. Un paquete cuesta 42 bytes de cabecera, así
que 1 byte de datos o 8 cuesta prácticamente lo mismo — la redundancia es
*gratis*. Un paquete perdido lo cubre el siguiente, sin pedir nada de nuevo y
sin esperar a nada. Cuando de todas formas estás enviando algo una y otra vez,
esto le gana a los acuses de recibo siempre.

**Checksums para pillar lo que no se ve.** Cada 30 frames cada lado envía un
checksum de todo su estado de juego. Si alguna vez no coinciden, las máquinas
están calladamente simulando juegos distintos — y sin esto no te enterarías
nunca, porque las dos pantallas siguen teniendo todo el sentido del mundo.
Cuesta 6 bytes en un paquete que iba a salir de todas formas.

Todo `lockstep.c` habla con la red a través de exactamente dos funciones,
`net_send()` y `net_receive()`. No sabe que la red es IPX, y `net.c` no sabe
que lo que lleva son frames de teclas.

Ése es el sentido de haberlos separado.
