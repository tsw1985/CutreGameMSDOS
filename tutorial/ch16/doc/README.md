# Capítulo 16 — Que dos máquinas se encuentren

*[English version](README-EN.md)*

**Qué vas a conseguir:** dos máquinas emparejadas **sin escribir ninguna
dirección**.

```
./play.sh both
```
y en las dos ventanas: `cd tutorial\ch16` y `chap16`.

---

## 1. El problema

En TCP/IP escribes una IP. En IPX **no hay IP**: la dirección de una máquina es
la **MAC de su tarjeta**, doce dígitos hexadecimales que nadie se sabe y que
cambian si cambias de tarjeta.

Obligar al jugador a escribir `00:1A:2B:3C:4D:5E` sería horrible.

## 2. La solución: el broadcast

Existe una dirección especial:

```
   FF:FF:FF:FF:FF:FF
```

Significa **"todas las máquinas de este segmento de red"**. Mandas ahí y le
llega a todo el mundo.

Y entonces el protocolo es de tres pasos:

```
   Maquina A                          Maquina B
      |                                   |
      |----- HELLO a todos -------------->|
      |                                   |
      |<---- HELLO_ACK (directo) ---------|
      |                                   |
      |  ya se donde esta, porque todo    |
      |  paquete IPX lleva quien lo manda |
```

1. Grito **HELLO** a todos, cada cuarto de segundo
2. Si alguien me oye, me contesta **HELLO_ACK**, y en esa respuesta viene **su
   dirección**
3. Ya sé dónde está. A partir de ahí le hablo solo a él

**Nadie ha escrito nada.** Ese es el objetivo.

## 3. Las tres puertas al mismo mecanismo

```c
	net_wait_for_client(s);     /* escucha y no grita    -> "servidor" */
	net_connect_to_server(s);   /* grita hasta que le contestan -> "cliente" */
	net_find_peer(s);           /* grita Y escucha -> dos iguales */
```

Las tres usan **el mismo bucle** por dentro.

Y hay algo importante ahí: **"servidor" y "cliente" no existen en IPX**. Son un
convenio construido encima. En IPX las dos máquinas son exactamente iguales, y
la única diferencia es quién grita y quién escucha.

Para un juego de dos, `net_find_peer()` es lo cómodo: arrancas las dos en
cualquier orden y se encuentran solas.

## 4. Quién es el jugador 1, sin gastar un paquete

Alguien tiene que llevar el tanque azul y alguien el rojo, y **las dos máquinas
tienen que estar de acuerdo**.

Se podría negociar: *"yo quiero ser el 1"*, *"vale, pues yo el 2"*... Paquetes,
esperas, y un caso raro si los dos piden lo mismo a la vez.

Se hace mucho más simple. Cada máquina sacó un número aleatorio al arrancar, y
tras el emparejamiento **cada una conoce los dos números**. Así que las dos
aplican la misma regla:

```c
	if (net_get_local_id() < net_get_remote_id()){
		/* soy el jugador 1 */
	}
```

> El del número más pequeño es el jugador 1.

Las dos hacen la misma cuenta con los mismos datos, así que **llegan a la misma
conclusión**. Cero paquetes, cero esperas, cero casos raros.

## 5. Y eso es el lockstep en miniatura

Ese truco — **que las dos partes calculen lo mismo en vez de preguntárselo** —
es exactamente la idea del capítulo 18, aplicada a una decisión pequeña.

Si te convence aquí, el lockstep te va a parecer natural.

## 6. Un detalle que importa

`net.c` contesta a un HELLO **siempre**, no solo mientras está emparejando. Eso
cubre dos casos: que se pierda un ACK, y que el otro arranque más tarde.

## 7. Experimentos

1. **Arranca las dos en orden distinto.** Da igual: `net_find_peer()` grita y
   escucha a la vez.
2. **Arranca solo una.** A los 30 segundos se rinde.
3. **Mira los dos ids** en las dos pantallas. Comprueba que el menor se declara
   jugador 1 y el mayor jugador 2, sin haberlo negociado.
4. **Arranca tres copias.** Se emparejan dos y la tercera se queda fuera.

## 8. Lo que hay que llevarse

| | |
|---|---|
| No hay IP que escribir: se descubren solas | |
| **Broadcast a FF:FF:FF:FF:FF:FF** | HELLO / HELLO_ACK |
| Servidor y cliente son un convenio, no IPX | Las dos máquinas son iguales |
| **El rol se calcula, no se negocia** | Comparando dos ids |

---

**Anterior:** [Capítulo 15](../../ch15/doc/README.md) ·
**Siguiente:** [Capítulo 17 — Un chat](../../ch17/doc/README.md)
