# Probar el juego en red

Ficheros de configuracion de DOSBox para probar `game.exe /net`.

**Antes de nada:** el `bin\GAME.EXE` que se use tiene que ser el **recien
compilado**, el que ya lleva la red. Copialo desde la maquina donde compiles.

---

## Paso 1 - un solo PC, dos DOSBox

Es la prueba que separa "mi codigo esta bien" de "mi red esta bien". Si esto
funciona, el codigo es correcto y cualquier problema posterior es de red.

```
dosbox -conf net-test/dosbox-local-a.conf     <- primero esta
dosbox -conf net-test/dosbox-local-b.conf     <- y luego esta
```

No hace falta configurar nada.

---

## Paso 2 - dos maquinas: usa los scripts

Los `.conf` de esta carpeta llevan rutas absolutas escritas dentro, y eso es
justo lo que se rompe al pasarlos a la segunda maquina. Para dos maquinas usa
los scripts de la raiz del proyecto: **averiguan solos donde esta el juego** a
partir de donde estan ellos mismos, y generan el `.conf` en cada arranque. No
hay ninguna ruta que ajustar en ningun sitio.

En la maquina que hace de servidor (arranca esta **primero**):

```
./launch_game_server.sh
```

Te dice por pantalla la IP que hay que darle al otro jugador. En la otra:

```
./launch_game_client.sh <esa-ip>
```

Cada maquina necesita su copia de `bin\` y de `res\`, y el proyecto puede
estar en carpetas distintas en cada una: da igual.

Opciones utiles en los dos scripts:

| | |
|---|---|
| `-p 6000` | otro puerto, el mismo en las dos |
| `-y 'max'` | otros ciclos de CPU, los mismos en las dos |
| `-c fichero.conf` | usar un `.conf` propio en vez de generarlo |
| `-h` | ayuda |

Antes de arrancar comprueban que DOSBox esta instalado, que estan `bin/` y
`res/`, que el puerto no es privilegiado ni esta ocupado, y el cliente hace
ping al servidor. Cada fallo dice que pasa y como arreglarlo.

> "Servidor" no significa que no juegue. Las dos maquinas juegan exactamente
> igual; lo unico que hace el servidor es levantar el tunel para que la otra
> pueda engancharse. Tampoco tiene nada que ver con quien acaba siendo el
> jugador 1.

---

## Donde estan los logs

Cada instancia escribe su log en `k:\game.log`, y cada conf monta una K:
**distinta** a proposito: con una compartida, las dos copias se machacarian el
log la una a la otra justo donde mas falta hace.

| Como lo lanzaste | Servidor / A | Cliente / B |
|---|---|---|
| Los scripts | `net-test/log-server/GAME.LOG` | `net-test/log-client/GAME.LOG` |
| `dosbox-local-*.conf` | `net-test/log-a/GAME.LOG` | `net-test/log-b/GAME.LOG` |

Con dos maquinas, cada log se queda **en su propia maquina**.

```
tail -f net-test/log-server/GAME.LOG | grep --line-buffered "NET\|Tank hit"
```

| Linea | Que significa |
|---|---|
| `NET sizes: ecb=42 header=30 packet=60` | Bien. Otro numero significa que el compilador ha rellenado las estructuras e IPX leeria todos los campos del sitio equivocado |
| `NET: IPX driver entry at XXXX:YYYY` | El driver IPX esta ahi |
| `NET: node 000000000000` | **Mala senal.** Un nodo todo ceros significa que ningun tunel nos ha asignado direccion: esta instancia no esta unida a ninguno |
| `NET: paired. ... we are player N` | Emparejados, y que tanque toco |
| `NET DESYNC at frame N` | Las dos maquinas han calculado estados distintos |
| `NET: sent X, received Y, waited Z frames` | Resumen al salir |

Sal con **ESC**, no cerrando la ventana, o la linea de resumen no se escribe.

### Como leer el `waited`

`waited` cuenta los frames que el juego paso parado esperando a la otra
maquina. Tiene **dos causas distintas**, y distinguirlas importa porque la
solucion no es la misma:

- **Unos pocos, a rachas** — jitter de la red. Para eso esta
  `NET_INPUT_DELAY` en `header/net.h`. Subelo.
- **Una fraccion grande y sostenida** de los frames (por ejemplo la mitad) —
  las dos maquinas **no van a la misma velocidad**, y la rapida se esta
  frenando al ritmo de la lenta. Subir `NET_INPUT_DELAY` **no arregla esto** y
  solo mete latencia: un buffer fijo absorbe variacion, no una diferencia
  permanente de velocidad. Baja `cycles` en **las dos** maquinas a un valor que
  la lenta pueda sostener (`-y 'fixed 20000'`).

Para saber cual es la lenta, compara el `waited` de los dos logs: la lenta lo
tendra cerca de cero.

Dentro de DOSBox, para comprobar el tunel sin el juego:

```
ipxnet ping
ipxnet status
```

---

## El puerto: por que no es el 213

DOSBox usa por defecto el **puerto UDP 213** para el tunel IPX. En **Linux**
eso no funciona: cualquier puerto por debajo de 1024 es privilegiado y un
usuario normal **no puede abrirlo**. `ipxnet startserver` falla, y lo unico
que ves es que el cliente da `Timeout connecting to server`.

Por eso estos confs usan el **5213**, que es alto y no necesita permisos:

```
ipxnet startserver 5213
ipxnet connect <ip> 5213
```

El puerto tiene que ser **el mismo en las dos maquinas**. En Windows el 213
si funciona, pero no hay razon para usarlo: deja el 5213 en las dos y te
olvidas. Si abres el cortafuegos, abre **UDP 5213**, no el 213.

---

Documentacion completa: [`NETWORK.md`](NETWORK.md)
