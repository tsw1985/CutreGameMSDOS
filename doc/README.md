# Documentación / Documentation

Documentación técnica del proyecto, en dos idiomas. Los dos árboles tienen los
mismos ficheros con los mismos nombres.

*Technical documentation for the project, in two languages. Both trees carry
the same files under the same names.*

| | Español | English |
|---|---|---|
| **MANUAL de red / Network MANUAL** | [`ES/MANUAL-RED.md`](ES/MANUAL-RED.md) | [`EN/NETWORK-MANUAL.md`](EN/NETWORK-MANUAL.md) |
| **Sonido / Sound** | [`ES/SOUND.md`](ES/SOUND.md) | [`EN/SOUND.md`](EN/SOUND.md) |
| **Red / Network** | [`ES/NETWORK.md`](ES/NETWORK.md) | [`EN/NETWORK.md`](EN/NETWORK.md) |
| **Probar la red / Testing the network** | [`ES/NETWORK-TESTING.md`](ES/NETWORK-TESTING.md) | [`EN/NETWORK-TESTING.md`](EN/NETWORK-TESTING.md) |
| **El reproductor WAV original / The original WAV player** | [`ES/SBWAV8-FLOW.md`](ES/SBWAV8-FLOW.md) | [`EN/SBWAV8-FLOW.md`](EN/SBWAV8-FLOW.md) |

---

### Español

- **MANUAL-RED** — **empieza por aquí si quieres aprender.** Un manual de
  cero a jugar en red: por qué mandar posiciones no vale, qué es el
  determinismo, qué es lockstep, qué es IPX, el código en orden de ejecución,
  cómo viaja una tecla hasta las dos pantallas, y experimentos para trastear.
- **NETWORK** — la referencia, para consultar.
- **SOUND** — `src/sound.c`: el mezclador por software, el DMA en auto-init,
  el doble buffer, y el porqué de cada decisión.
- **NETWORK** — `src/net.c`: qué es IPX, por qué lockstep, el retardo de
  entrada, la detección de desincronización, el flujo completo y la referencia
  de todas las funciones.
- **NETWORK-TESTING** — cómo montar dos máquinas para probarlo, y qué mirar en
  el log cuando algo no va.
- **SBWAV8-FLOW** — el reproductor de WAV original `src/sb/sbwav8.c` del que
  salió el módulo de sonido.

### English

- **NETWORK-MANUAL** — **start here if you want to learn.** A manual from
  nothing to networked play: why sending positions does not work, what
  determinism is, what lockstep is, what IPX is, the code in execution order,
  how a keypress travels to both screens, and experiments to play with.
- **NETWORK** — the reference, for looking things up.
- **SOUND** — `src/sound.c`: the software mixer, auto-init DMA, the double
  buffer, and the reasoning behind every decision.
- **NETWORK** — `src/net.c`: what IPX is, why lockstep, the input delay, desync
  detection, the complete flow and a reference for every function.
- **NETWORK-TESTING** — how to set up two machines to try it, and what to look
  for in the log when something goes wrong.
- **SBWAV8-FLOW** — the original `src/sb/sbwav8.c` WAV player the sound module
  came from.
