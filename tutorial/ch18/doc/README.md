# Capítulo 18 — Lockstep: el juego en red

*[English version](README-EN.md)*

**El capítulo más importante del bloque.** Aquí está la idea que hace que un
juego funcione en red, y no es la que uno espera.

```
./launch_game_both.sh
```
y en las dos: `cd tutorial\ch18` y `chap18`. Los dos se conducen con las
flechas.

---

## 1. Lo que no funciona: mandar posiciones

Lo primero que se le ocurre a cualquiera:

> "Mando dónde está mi tanque, y el otro lo dibuja ahí."

Parece obvio y da un juego malo:

- El tanque del otro se ve **a saltos**, porque los paquetes no llegan a un
  ritmo perfecto
- Si un paquete se pierde, su tanque **se teletransporta**
- Para dos tanques y sus balas hay que mandar bastantes bytes por frame
- Y las balas van a su ritmo, así que también hay que sincronizarlas

Se puede arreglar con interpolación y predicción, y así es como funcionan los
juegos modernos. Es mucho trabajo.

## 2. Lo que se hace: mandar teclas

Se manda **qué teclas has pulsado**. Un byte. El del capítulo 8.

Y **las dos máquinas simulan la partida entera**, los dos tanques, las dos
balas.

Si las dos hacen exactamente las mismas cuentas con las mismas entradas, llegan
al mismo resultado. **No hace falta mandar posiciones porque las dos las
calculan.**

A eso se le llama **lockstep**.

| | Posiciones | Lockstep |
|---|---|---|
| Bytes por frame | Muchos | **Uno** |
| Si se pierde un paquete | Teletransporte | Se espera (y el cap. 19 lo detecta) |
| Requisito | Ninguno | **Determinismo** |

## 3. Determinismo: la condición de todo

> Mismas entradas ⟶ mismo resultado. **Siempre.**

Eso significa, en la práctica:

- Nada de números aleatorios sin semilla compartida
- Nada que dependa del reloj
- **Nada que dependa de la velocidad de la máquina**

Y aquí hay una buena noticia: **este juego ya era determinista sin
pretenderlo.** El tanque se mueve 2 píxeles **por frame**, no por milisegundo.
Una máquina lenta va más despacio en tiempo real, pero recorre exactamente los
mismos píxeles.

Si el juego usara `delta time` (movimiento proporcional al tiempo transcurrido),
nada de esto funcionaría, porque las dos máquinas nunca medirían exactamente lo
mismo.

## 4. El retardo de entrada

```c
	net_set_local_input(local_input);
```

Esa llamada **no guarda las teclas para el frame de ahora**. Las guarda para el
frame **actual + NET_INPUT_DELAY**, que son 5 frames. Unos 70 ms.

¿Por qué introducir un retraso a propósito?

```
   frame 95:  mando mis teclas "para el frame 100"
   frame 96:  ...viajando...
   frame 97:  ...viajando...
   frame 98:  llega
   frame 99:
   frame 100: las uso. Llevaban dos frames esperando.
```

**Sin retardo**, cada frame tendría que esperar a un paquete que **acaba de
salir**, y cualquier hipo de la red se vería como un tirón.

Con 5 frames de margen, el paquete casi siempre está esperando cuando lo
necesitas.

⚠️ **Y aquí hay una trampa que parece una mejora:** aplicar *tus* teclas al
instante y las del otro con retardo. Se siente mejor... y **desincroniza en el
primer segundo**, porque las dos máquinas estarían haciendo cuentas distintas.
Las dos tienen que ir igual de tarde.

## 5. El precio: esperar

```c
		while (net_has_remote_input() == 0){
			net_poll();
			...
		}
```

Si el otro se retrasa, **tú te paras**. Las dos máquinas van siempre por el
mismo frame, nunca una por delante.

Eso es lo que da nombre a *lockstep*: marchar al paso, atados.

Con el retardo de 5 frames, esta espera casi siempre dura cero. Cuando dura, se
nota como un tirón en **las dos** máquinas a la vez.

Fíjate en que el juego real llama a `sound_update()` **dentro de esta espera**.
Un hipo de la red no puede convertirse en un hipo del sonido.

## 6. Y aquí se cobra el capítulo 8

```c
		if (local_player_is_1 == 1){
			player1_input = net_get_local_input();
			player2_input = net_get_remote_input();
		}else{
			player1_input = net_get_remote_input();
			player2_input = net_get_local_input();
		}

		process_player_input(&player1, player1_input);
		process_player_input(&player2, player2_input);
```

**A partir de esa línea, el resto del programa es literalmente el del capítulo
8.** Compáralos:

```
diff tutorial/ch08/chap08.c tutorial/ch18/chap18.c
```

`process_player_input()`, `tut_try_move()`, las colisiones, la animación, el
dibujado: **ni una línea cambiada**.

Eso es lo que se sembró en el capítulo 8 al separar *"qué teclas"* de *"de dónde
vienen las teclas"*. Los bits ahora llegan por un cable, y a nada de lo que hay
debajo le importa.

**Si haces un juego y quieres poder meterle red algún día, haz esa separación
desde el principio.** Es lo único que hay que prever.

## 7. El frame en red, los cinco pasos

```
  1. leer mis teclas
  2. entregarlas (van al frame actual + 5) y enviarlas
  3. ESPERAR a que lleguen las suyas para ESTE frame
  4. repartir: las mías a mi tanque, las suyas al suyo
  5. simular y dibujar, igual que siempre
  6. net_advance_frame()
```

El orden es sagrado. Y el paso 6 solo ocurre cuando las dos máquinas han
terminado el frame con el mismo estado.

## 8. Experimentos

1. **`diff` con el capítulo 8.** Es la mejor forma de ver qué añade la red.
2. **Baja `NET_INPUT_DELAY` a 1** en `header/lockstep.h`. Más reactivo, y con
   cualquier hipo de la red se ven tirones.
3. **Súbelo a 20.** Fluidísimo, y el control se siente pastoso.
4. **Aplica tu input al instante** (salta el sistema de retardo para el jugador
   local). Desincroniza enseguida: eso lo ve el capítulo 19.

## 9. Lo que hay que llevarse

| | |
|---|---|
| **No mandes posiciones: manda teclas** | Un byte por frame |
| Las dos máquinas simulan la partida entera | Y llegan al mismo resultado |
| **Determinismo es la condición** | Por frame, no por milisegundo |
| El retardo de entrada da margen a la red | Y tiene que aplicarse a los DOS |
| El precio es esperar | Las dos van al paso |
| **La separación del capítulo 8 es lo que lo hace posible** | Ni una línea cambiada debajo |

---

**Anterior:** [Capítulo 17](../../ch17/doc/README.md) ·
**Siguiente:** [Capítulo 19 — Desincronización](../../ch19/doc/README.md)
