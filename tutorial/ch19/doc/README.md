# Capítulo 19 — Desincronización: cazar el desvío

**Qué vas a conseguir:** provocar una desincronización a propósito, con la tecla
`D`, y verla detectada.

```
./launch_game_both.sh
```
y en las dos: `cd tutorial\ch19` y `chap19`.

---

## 1. Por qué es el bug más desagradable que hay

El capítulo 18 funciona **mientras** las dos máquinas hagan las mismas cuentas.

¿Y si una calcula algo distinto? Una desincronización. Y lo peor es que **no se
ve**:

- Las dos partidas siguen corriendo
- Ninguna da ningún error
- Cada una está convencida de ir bien
- Simplemente **cuentan historias distintas**

El jugador ve al otro tanque hacer cosas absurdas: disparar al aire, chocar con
nada, morir sin motivo. Y no hay ningún mensaje en ningún sitio.

En este juego la causa más habitual sería un `if` que se comporta distinto según
el orden en que se evalúa algo, o un valor que depende de la máquina.

## 2. La única forma de enterarse: comprobarlo

Cada máquina resume su estado en un número y lo compara con el de la otra. Si
difieren, se separaron.

```c
static unsigned int compute_state_checksum(){

	checksum = checksum + (player1.position_x * 3);
	checksum = checksum + (player1.position_y * 5);
	checksum = checksum + (player1.current_direction * 7);

	checksum = checksum + (player2.position_x * 31);
	...
}
```

**Los primos son para que el orden importe.** Si sumaras todo a pelo, dos
tanques con las posiciones intercambiadas darían el mismo número y no lo
detectarías.

## 3. Qué entra y, sobre todo, qué NO

**Entra:** todo lo que las dos máquinas tienen que calcular igual. Posiciones,
direcciones, balas, marcador.

**No entra, y es igual de importante:** nada que sea local de cada máquina.

En el capítulo 21 aparecerán `camera_x` y `camera_y`. Esas son
**legítimamente distintas** en cada lado, porque cada máquina sigue a su propio
tanque. Meterlas en el checksum daría una desincronización **falsa** en el frame
1 de todas las partidas.

> **Regla:** si un valor lo calcula el código de dibujado, no entra en el
> checksum.

## 4. El cable trampa del juego real

`src/main.c` mete una cosa más que no es estado:

```c
	checksum = checksum + ((unsigned int)map_width * 73);
	checksum = checksum + ((unsigned int)map_height * 79);
```

`map_width` no cambia nunca durante una partida. ¿Qué hace ahí?

Es una **trampa deliberada**. Si arrancas una máquina con `/bigmap` y la otra
sin él, los mapas son distintos, los muros están en sitios distintos, y las dos
simulaciones se separan de una forma imposible de leer.

Metiéndolo en el checksum, esa situación se convierte en **un informe limpio en
el log** en la primera comprobación. Cuesta dos sumas cada 30 frames.

## 5. Es gratis mandarlo

```c
	net_set_local_checksum(compute_state_checksum());
```

`lockstep.c` lo mete **en el mismo paquete que las teclas**. Como el paquete ya
existe y tiene sitio de sobra (la cabecera IPX son 42 bytes y el contenido 20),
enviar el checksum **no cuesta un solo paquete extra**.

## 6. Pulsa `D`

```c
		if (keys[KEY_D] && desync_forced == 0){
			player1.position_x = player1.position_x + 1;
			desync_forced = 1;
		}
```

Mueve el tanque **un píxel**, solo en esta máquina. Uno.

La otra no se entera y sigue calculando sin él. A partir de ese frame las dos
partidas son distintas y **a simple vista no se nota todavía**.

Pero el checksum lo caza en la siguiente comprobación.

Eso es lo que hace un checksum: **convertir un fallo invisible en una línea del
log**.

## 7. No arregla nada, y no pasa nada

El checksum no repara la desincronización. Solo avisa.

Y avisar es todo lo que necesitas, porque te dice **el frame exacto** en que se
rompió. Ahí es donde está el fallo, y con eso puedes buscarlo.

Sin checksum, buscarías a ciegas en toda la partida.

## 8. Experimentos

1. **Pulsa `D` en una sola máquina.** Verás el aviso al salir.
2. **Púlsalo en las dos a la vez.** Las dos aplican el mismo +1, así que **no
   hay desincronización**. Eso demuestra que lo que importa es la diferencia,
   no el cambio.
3. **Quita los primos** y suma todo a pelo. Intercambia las posiciones de los
   dos tanques a mano: el checksum no lo nota.
4. **Mete algo local en el checksum** (por ejemplo `local_player_is_1`).
   Desincronización en el frame 1, siempre.

## 9. Lo que hay que llevarse

| | |
|---|---|
| Una desincronización **no se ve**: las dos partidas siguen | |
| **La única forma de enterarse es comprobarlo** | Un resumen del estado |
| Los primos hacen que el orden importe | |
| **Nada local en el checksum** | La cámara nunca entra |
| Viaja gratis con las teclas | El paquete ya existe |
| No arregla; **dice el frame exacto** | Y con eso basta |

---

**Anterior:** [Capítulo 18](../../ch18/doc/README.md) ·
**Siguiente:** [Capítulo 20 — El mundo deja de ser la pantalla](../../ch20/doc/README.md)
