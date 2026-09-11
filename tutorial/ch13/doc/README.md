# Capítulo 13 — Mezclar: varios sonidos a la vez

**Qué vas a conseguir:** dos motores y un disparo sonando simultáneamente. Y oír
la distorsión cuando te pasas.

```
make
chap13
```

`1` disparo · `2` explosión · `3`/`4` motores · **`5` volúmenes al máximo** ·
`0` parar todo

---

## 1. La tarjeta solo tiene un canal

Esta es la sorpresa. Una Sound Blaster de 8 bits reproduce **un flujo de
muestras**. Uno. No tiene ocho canales ni nada parecido.

Entonces, ¿cómo suenan dos motores y un disparo a la vez?

**Los sumas tú, a mano, muestra a muestra, antes de dárselos a la tarjeta.** Eso
es mezclar, y lo hace `sound_update()` cada vez que rellena media buffer.

```
   motor 1:   .-'-.  .-'-.  .-'-.
   motor 2:  ~~^~~^~~^~~^~~^~~
   disparo:        |||||
                 +
   ------------------------------
   lo que sale:  la suma de los tres
```

## 2. El recorte, y a qué suena

Cada muestra cabe en un byte. Si sumas tres sonidos fuertes, la suma **se sale
del rango**.

Cuando eso pasa, el mezclador tiene que **recortar**: dejarlo en el máximo. Y el
recorte suena a **distorsión sucia**, como un altavoz roto.

**Pulsa `5` en el programa**, enciende los dos motores y dispara. Eso es el
recorte.

## 3. Por eso los volúmenes del juego son bajos

```c
	set_sound_volume(fire,    16);
	set_sound_volume(engine1, 12);
	set_sound_volume(engine2, 12);
	set_sound_volume(died,    34);
```

Estos son los números reales de `src/main.c`, y no son arbitrarios:

- Los **motores** van bajos (12) porque suenan **todo el rato** y son dos. Si
  fueran altos, no oirías nada más.
- El **disparo** (16) tiene que oírse *por encima* de los motores.
- La **explosión** (34) es un evento raro e importante: puede permitirse ser
  fuerte.

Mezclar bien es más de decidir qué es importante que de programar.

## 4. Sonar una vez contra sonar en bucle

| | |
|---|---|
| `play_sound(id)` | Suena una vez y se acaba sola |
| `loop_sound(id)` | Suena y vuelve a empezar, **para siempre** |

Si llamas a `play_sound()` otra vez antes de que acabe, **suenan las dos copias
solapadas**. Para un disparo eso es lo correcto.

Un bucle, en cambio, **no se acaba solo**. Alguien tiene que pararlo:

```c
	stop_looping_sound(engine1);
```

En el juego, el motor arranca cuando pulsas una flecha y para cuando la sueltas.
Y hay un caso que se olvida siempre: **cuando el tanque muere**. Si nadie para
el motor ahí, el tanque sigue rugiendo mientras arde.

## 5. Las voces

Por dentro `sound.c` tiene un número limitado de "voces": huecos donde puede
estar sonando algo. Si se llenan todas y pides otro sonido, el nuevo no suena.

Para este juego (dos motores, dos disparos, una explosión, una canción) sobran.
Pero si haces un juego con muchos efectos simultáneos, es lo primero que hay que
mirar.

## 6. Experimentos

1. **Pulsa `5` y dispara con los dos motores.** Escucha el recorte.
2. **Pon todos los volúmenes a 4.** Limpio pero inaudible. El equilibrio está
   entre esos dos extremos.
3. **Enciende un motor y sal con ESC** sin pararlo. `sound_end()` lo corta, pero
   prueba a quitar el `sound_end()` y verás lo del capítulo 11.
4. **Dispara diez veces muy rápido.** En algún momento dejan de solaparse: se
   acabaron las voces.

## 7. Lo que hay que llevarse

| | |
|---|---|
| **La tarjeta tiene un canal; la mezcla la haces tú** | Sumando muestra a muestra |
| Si la suma se sale, hay que **recortar**, y suena mal | |
| Los volúmenes bajos no son timidez: evitan el recorte | |
| `play_sound` se acaba; `loop_sound` no | Un bucle necesita quien lo pare |

---

**Anterior:** [Capítulo 12](../../ch12/doc/README.md) ·
**Siguiente:** [Capítulo 14 — Música](../../ch14/doc/README.md)
