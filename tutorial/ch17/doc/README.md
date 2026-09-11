# Capítulo 17 — Enviar y recibir: un chat

**Qué vas a conseguir:** escribir en una máquina y leerlo en la otra.

```
./launch_game_both.sh
```
y en las dos: `cd tutorial\ch17` y `chap17`.

---

## 1. Mensajes, no un flujo de bytes

Esta es la diferencia gorda con TCP, y conviene tenerla clara.

**En TCP** mandas 10 bytes y luego 5, y el otro lado puede leer 15 de golpe, o 3
y luego 12. TCP te da un **flujo**: los bytes llegan en orden pero las fronteras
entre tus envíos **desaparecen**. Tienes que inventarte tú dónde acaba cada
mensaje.

**Aquí no.** Un `net_send()` es un `net_receive()`:

```c
	length = net_receive(incoming, NET_MAX_DATA);
```

Devuelve **un mensaje completo**, o 0 si no hay nada. Nunca medio mensaje, nunca
dos pegados.

A eso se le llama **datagrama**, y te ahorra todo el trabajo de delimitar.

## 2. `net_update()` es obligatorio

```c
		net_update();
```

`net.c` deja **cuatro buzones** preparados con el driver. Cuando llega un
paquete, el driver lo mete en uno y lo marca como lleno.

`net_update()` es quien va a mirar esos cuatro buzones, se lleva lo que haya, y
**vuelve a dejarlos vacíos y preparados**.

Si no lo llamas, los cuatro se llenan y a partir de ahí **todo lo que llegue se
pierde**. El driver no guarda nada por su cuenta.

Se llama *poll*, sondear: **la iniciativa es tuya**. IPX ofrece lo contrario
(que te avise con una interrupción, el `esr_address`) y `net.c` **no lo usa a
propósito**, porque esa rutina saltaría en mitad de cualquier cosa que estuviera
haciendo el juego. Es la misma decisión que en el sonido: la interrupción marca,
el bucle trabaja.

## 3. El bucle de vaciado

```c
		length = net_receive(incoming, NET_MAX_DATA);

		while (length > 0){
			...
			length = net_receive(incoming, NET_MAX_DATA);
		}
```

En una sola vuelta pueden haber llegado varios mensajes. Hay que sacarlos
todos, o se acumulan.

## 4. Enviar no es que llegue

```c
	if (net_send(line, position) == 1){ ... }
```

Ese `1` significa **"se lo he entregado al driver"**. No significa que llegue.

IPX no garantiza nada. El paquete puede perderse y nadie te avisa.

Para un chat eso es un problema de verdad. **Para un juego no**: es preferible
perder un paquete y seguir, que parar la partida para reenviarlo. El capítulo 18
explica cómo se aguanta esa pérdida sin reenviar nada.

## 5. No es solo para texto

```c
	net_send(line, position);
```

`net_send()` manda **bytes**: hasta `NET_MAX_DATA` de una vez. Pueden ser:

- Texto, como aquí
- Una `struct` (`net_send(&mi_struct, sizeof(mi_struct))`)
- Tres bytes que signifiquen algo solo para ti
- Un trozo de un fichero, si quieres hacer un "enviar archivo"

El capítulo 18 manda justo eso: una `struct` de 20 bytes con teclas de tanque.

## 6. Experimentos

1. **Manda una línea larguísima.** Se corta en `NET_MAX_DATA`.
2. **Quita el `net_update()`.** Recibes cuatro mensajes y luego nada más nunca.
   Ese es el fallo que te enseña para qué sirve.
3. **Quita el bucle `while`** y deja un solo `net_receive()`. Escribiendo rápido
   en la otra máquina, se te acumulan.
4. **Manda una struct** en vez de texto. Define una con tres `int`, rellénala y
   mándala con `sizeof`.

## 7. Lo que hay que llevarse

| | |
|---|---|
| **Un `net_send()` = un `net_receive()`** | Datagramas, no flujo |
| **`net_update()` en cada vuelta** | O se llenan los cuatro buzones y se pierde todo |
| Vacía la cola con un `while` | Pueden llegar varios |
| Enviar no es que llegue | Y para un juego eso está bien |
| Manda bytes, no texto | Structs, ficheros, lo que sea |

---

**Anterior:** [Capítulo 16](../../ch16/doc/README.md) ·
**Siguiente:** [Capítulo 18 — Lockstep](../../ch18/doc/README.md)
