# Capítulo 15 — Encontrar el driver IPX

*[English version](README-EN.md)*

**Qué vas a conseguir:** saber si hay red y abrir un socket. En modo texto.

**Qué código real se usa:** `src/net.c`.

```
make
chap15
```

---

## 1. Qué es IPX, y qué no es

En 1995 no había TCP/IP en DOS de serie. Lo que había en las oficinas eran redes
**Novell NetWare**, y su protocolo se llamaba **IPX**.

IPX **no es TCP/IP**, y las diferencias importan:

| | IPX |
|---|---|
| Conexiones | **No hay.** Solo mandas paquetes sueltos |
| Entrega garantizada | **No.** Un paquete puede perderse sin avisar |
| Orden garantizado | **No.** Pueden llegar cambiados |
| Direcciones | La MAC de la tarjeta. No se escriben: se descubren |

Suena peor que TCP/IP. **Para un juego es mejor**: no quieres que un paquete
perdido pare la partida mientras se reenvía. Quieres seguir y arreglarlo de otra
forma (capítulo 18).

Lo más parecido en el mundo moderno es UDP.

## 2. Cómo se encuentra el driver

IPX es un **TSR**: un programa que se carga antes que el tuyo y se queda
residente en memoria. Hay que preguntar si está.

El convenio es el **INT 2F**, la "interrupción multiplexada": el tablón de
anuncios de los TSR. Cada uno tiene su número y contesta si está.

```
   AX = 0x7A00   ->   "IPX, ¿estás ahí?"
```

Si vuelve con **AL = 0xFF**, está. Y además te deja en **ES:DI** la dirección de
su punto de entrada.

## 3. Lo raro: un far call, no una interrupción

Y aquí viene lo que hace que `net.c` tenga ensamblador.

A IPX **no se le llama con una interrupción**. Se le llama con un **FAR CALL** a
esa dirección, como a una función normal que estuviera en otro segmento.

Es la única API de DOS que funciona así. Todo lo demás (vídeo, disco, teclado,
el propio DOS) va por interrupciones.

Qué función quieres se pone en **BX**:

| BX | Función |
|---|---|
| 0x0000 | Abrir socket |
| 0x0001 | Cerrar socket |
| 0x0003 | Enviar |
| 0x0004 | Escuchar |
| 0x0009 | Dime mi dirección |
| 0x000A | Cede el control un momento |

## 4. Qué es un socket

Un número de 16 bits que identifica **de qué va** un paquete.

Por el mismo cable pasan paquetes de todo: de tu juego, de otro juego, del
servidor de ficheros. **Todos llegan a tu tarjeta.** El socket es lo que te deja
quedarte solo con los tuyos.

`net.c` usa el **0x869C**. Cualquier número de 0x8000 arriba vale: Novell
reservó ese rango para programas no registrados.

Si otro programa ya lo tiene cogido, abrir falla. Por eso `net_start()` puede
decir *"puede que haya otra copia corriendo"*.

## 5. El id local

```c
	net_get_local_id()
```

Un número aleatorio que `net.c` se saca al arrancar. No sirve para direccionar
nada: sirve para **decidir quién es el jugador 1** en el capítulo 16, sin gastar
ni un paquete.

## 6. `net_end()` no es opcional

Cierra el socket. Un socket que se queda abierto no lo puede usar el siguiente
programa.

## 7. Cómo probarlo

**Lo más fácil, desde Linux:**

```bash
./play.sh both
```

Levanta dos ventanas de DOSBox ya conectadas entre sí. En las dos:
`cd tutorial\ch15` y `chap15`.

**A mano en DOSBox:** `ipx=true` en `dosbox.conf`, y dentro:
`ipxnet startserver` en una, `ipxnet connect <ip>` en la otra.

**En DOS real:** carga `LSL`, el driver ODI de tu tarjeta e `IPXODI` antes.

## 8. Lo que hay que llevarse

| | |
|---|---|
| IPX no tiene conexiones ni garantías | Y para un juego eso está bien |
| Se busca con **INT 2F, AX=7A00** | `AL=0xFF` = está |
| **Se le llama con un far call**, no una interrupción | Único caso en DOS |
| Un **socket** filtra los paquetes que son tuyos | 0x869C aquí |

---

**Anterior:** [Capítulo 14](../../ch14/doc/README.md) ·
**Siguiente:** [Capítulo 16 — Encontrarse](../../ch16/doc/README.md)
