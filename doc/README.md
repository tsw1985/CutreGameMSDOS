# Documentación / Documentation

Documentación técnica del proyecto, en dos idiomas. Los dos árboles tienen los
mismos ficheros con los mismos nombres.

*Technical documentation for the project, in two languages. Both trees carry
the same files under the same names.*

| | Español | English |
|---|---|---|
| **TUTORIAL de sonido / Sound TUTORIAL** | [`ES/TUTORIAL-SONIDO.md`](ES/TUTORIAL-SONIDO.md) | [`EN/SOUND-TUTORIAL.md`](EN/SOUND-TUTORIAL.md) |
| **TUTORIAL de red / Network TUTORIAL** | [`ES/TUTORIAL-RED.md`](ES/TUTORIAL-RED.md) | [`EN/NETWORK-TUTORIAL.md`](EN/NETWORK-TUTORIAL.md) |
| **MANUAL de camara / Camera MANUAL** | [`ES/MANUAL-CAMARA.md`](ES/MANUAL-CAMARA.md) | [`EN/CAMERA-MANUAL.md`](EN/CAMERA-MANUAL.md) |
| **MANUAL de red / Network MANUAL** | [`ES/MANUAL-RED.md`](ES/MANUAL-RED.md) | [`EN/NETWORK-MANUAL.md`](EN/NETWORK-MANUAL.md) |
| **Sonido / Sound** | [`ES/SOUND.md`](ES/SOUND.md) | [`EN/SOUND.md`](EN/SOUND.md) |
| **Red / Network** | [`ES/NETWORK.md`](ES/NETWORK.md) | [`EN/NETWORK.md`](EN/NETWORK.md) |
| **Probar la red / Testing the network** | [`ES/NETWORK-TESTING.md`](ES/NETWORK-TESTING.md) | [`EN/NETWORK-TESTING.md`](EN/NETWORK-TESTING.md) |
| **El reproductor WAV original / The original WAV player** | [`ES/SBWAV8-FLOW.md`](ES/SBWAV8-FLOW.md) | [`EN/SBWAV8-FLOW.md`](EN/SBWAV8-FLOW.md) |

---

### Español

- **TUTORIAL-SONIDO** — **empieza por aqui si quieres USAR el sonido en otro
  proyecto.** Los cuatro pasos, `play_sound`, `loop_sound`, el volumen, como
  tienen que ser los WAV, y un programa completo de ejemplo. Nada de tanques.
- **TUTORIAL-RED** — **empieza por aqui si quieres USAR la red en otro
  proyecto.** Los cinco pasos, `net_send` y `net_receive`, servidor y cliente,
  un chat completo de ejemplo y como enviar un fichero. Nada de tanques.
- **MANUAL-CAMARA** — **empieza por aquí si quieres entender la cámara y el
  mapa grande.** De cero: los dos sistemas de coordenadas, la ventana, el
  recorte de sprites, la zona muerta, la máscara de colisión de un bit por
  píxel, por qué la cámara no puede entrar en el checksum de red, y la
  gestión de memoria de DOS con la historia completa del bug de `died.wav`.
- **MANUAL-RED** — **empieza por aquí si quieres aprender.** Un manual de
  cero a jugar en red: por qué mandar posiciones no vale, qué es el
  determinismo, qué es lockstep, qué es IPX, el código en orden de ejecución,
  cómo viaja una tecla hasta las dos pantallas, y experimentos para trastear.
- **NETWORK** — la referencia, para consultar.
- **SOUND** — `src/sound.c`: el mezclador por software, el DMA en auto-init,
  el doble buffer, y el porqué de cada decisión.
- **NETWORK** — `src/net.c` y `src/lockstep.c`: qué es IPX, por qué lockstep, el retardo de
  entrada, la detección de desincronización, el flujo completo y la referencia
  de todas las funciones.
- **NETWORK-TESTING** — cómo montar dos máquinas para probarlo, y qué mirar en
  el log cuando algo no va.
- **SBWAV8-FLOW** — el reproductor de WAV original `src/sb/sbwav8.c` del que
  salió el módulo de sonido.

### English

- **SOUND-TUTORIAL** — **start here if you want to USE the sound in another
  project.** The four steps, `play_sound`, `loop_sound`, volume, what the WAV
  files have to be, and a complete example program. No tanks anywhere.
- **NETWORK-TUTORIAL** — **start here if you want to USE the network in
  another project.** The five steps, `net_send` and `net_receive`, server and
  client, a complete chat example and how to send a file. No tanks anywhere.
- **CAMERA-MANUAL** — **start here if you want to understand the camera and
  the big map.** From nothing: the two coordinate systems, the window, sprite
  clipping, the dead zone, the one-bit-per-pixel collision mask, why the camera
  must never enter the network checksum, and DOS memory management with the
  full story of the `died.wav` bug.
- **NETWORK-MANUAL** — **start here if you want to learn.** A manual from
  nothing to networked play: why sending positions does not work, what
  determinism is, what lockstep is, what IPX is, the code in execution order,
  how a keypress travels to both screens, and experiments to play with.
- **NETWORK** — the reference, for looking things up.
- **SOUND** — `src/sound.c`: the software mixer, auto-init DMA, the double
  buffer, and the reasoning behind every decision.
- **NETWORK** — `src/net.c` and `src/lockstep.c`: what IPX is, why lockstep, the input delay, desync
  detection, the complete flow and a reference for every function.
- **NETWORK-TESTING** — how to set up two machines to try it, and what to look
  for in the log when something goes wrong.
- **SBWAV8-FLOW** — the original `src/sb/sbwav8.c` WAV player the sound module
  came from.
