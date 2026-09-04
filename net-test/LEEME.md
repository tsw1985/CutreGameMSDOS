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

## Paso 2 - dos maquinas

Cada maquina necesita su copia de `bin\` y de `res\`.

**Maquina A** (`dosbox-red-a.conf`): ajusta la ruta del `mount c`.
**Maquina B** (`dosbox-red-b.conf`): ajusta las rutas de los `mount` **y la IP
del `ipxnet connect`**, que es la de la maquina A.

Arranca **siempre la A primero**.

---

## Que mirar

El log de cada instancia queda en `log-a/game.log` y `log-b/game.log`, en K:
separadas a proposito: las dos copias escriben en `k:\game.log`, y con una K:
compartida se machacarian el log la una a la otra.

```
tail -f net-test/log-a/game.log
```

| Linea | Que significa |
|---|---|
| `NET sizes: ecb=42 header=30 packet=60` | Bien. Otro numero = estructuras rellenadas, IPX leeria mal |
| `NET: IPX driver entry at XXXX:YYYY` | DOSBox esta dando el IPX |
| `NET: paired. ... we are player N` | Emparejados, y que tanque toco |
| `NET: sent X, received Y, waited Z frames` | Resumen al salir. **`waited` es el numero clave** |

`waited` cuenta los frames parado esperando a la otra maquina. Cerca de 0, va
sobrado. Cientos o miles, sube `NET_INPUT_DELAY` en `header/net.h`.

Dentro de DOSBox, para comprobar el tunel sin el juego:

```
ipxnet ping
ipxnet status
```

Documentacion completa: [`../src/NETWORK.md`](../src/NETWORK.md)
