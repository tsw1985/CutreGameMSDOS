# Capítulo 5 — El teclado, de verdad

*[English version](README-EN.md)*

**Qué vas a conseguir:** un tanque que se mueve con las flechas, con el mismo
sistema de teclado que usa el juego.

**Qué código real se usa:** `src/bmp.c`, `header/players.h`.

```
make
chap05
```

---

## 1. Por qué `getch()` no sirve

Es lo primero que uno prueba, y falla por tres razones distintas:

**Bloquea.** `getch()` se queda parado hasta que pulsas algo. Un juego no puede
pararse: tiene que seguir dibujando, moviendo balas y sonando aunque no toques
nada.

**No sabe si sigues pulsando.** Te dice "se ha pulsado la A". No te dice "la A
está pulsada ahora mismo". Para que el tanque avance mientras mantienes la
flecha necesitas lo segundo.

**Una tecla cada vez.** Si el jugador 1 va con las flechas y el jugador 2 con
WASD y los dos se mueven a la vez, `getch()` no puede con ello.

## 2. Qué es un scancode

El teclado no manda letras. Manda el **número de la tecla física**.

| Tecla | Scancode |
|---|---|
| Flecha arriba | 0x48 |
| Flecha abajo | 0x50 |
| Flecha izquierda | 0x4B |
| Flecha derecha | 0x4D |
| ESC | 0x01 |
| W | 0x11 |

Esa distinción importa: la tecla que hay a la izquierda del 1 es siempre el
mismo scancode, tenga escrito lo que tenga según el país. Un juego quiere
posiciones, no letras.

## 3. El truco del bit 7

Cada vez que tocas una tecla, el teclado manda **un** scancode. Y manda otro al
soltarla, con el bit 7 encendido:

```
   Pulsas la flecha arriba  ->  0x48         (72)
   La sueltas               ->  0x48 + 128   (200)
```

```c
	if (scancode & 0x80){
		keys[scancode - 128] = 0;   /* soltada */
	}else{
		keys[scancode] = 1;         /* pulsada */
	}
```

**Ahí está todo.** Con un array de 128 casillas, cada tecla enciende la suya al
pulsarse y la apaga al soltarse, **independientemente de las demás**. Leer
varias a la vez sale gratis.

## 4. La interrupción 9

¿Y quién lee el scancode? Una **interrupción**.

Cuando tocas una tecla, el hardware interrumpe al procesador: para lo que
estuviera haciendo, salta a una función, y cuando esa función termina vuelve
exactamente a donde iba. Tu programa ni se entera.

La del teclado es la **INT 9**. DOS tiene la suya puesta (la que hace que
funcione `getch`), y nosotros ponemos la nuestra en su lugar:

```c
static void install_kbd(){
	old_kbd_handler = getvect(IRQ_KEYBOARD);   /* guardar la de DOS */
	setvect(IRQ_KEYBOARD, new_kbd_handler);    /* poner la nuestra */
}
```

Tres detalles que no son opcionales:

**`interrupt far`.** Le dice a Turbo C que esta función es un manejador: tiene
que guardar todos los registros al entrar, restaurarlos al salir, y volver con
`IRET` en vez de `RET`. Sin eso, el programa interrumpido se encuentra los
registros cambiados.

**`volatile` en `keys[]`.** Ese array lo modifica algo que el compilador no
puede ver. Sin `volatile` puede decidir que nadie lo cambia y dejar el valor en
un registro, y tu bucle leería siempre lo mismo.

**`outp(0x20, 0x20)` al final.** Le dice al controlador de interrupciones que
has terminado. Si se te olvida, **no vuelve a llegar ninguna interrupción** y
el teclado queda muerto.

## 5. Devolverlo al salir, o cuelgas la máquina

```c
static void uninstall_kbd(){
	setvect(IRQ_KEYBOARD, old_kbd_handler);
}
```

Esto no es limpieza opcional. Al terminar tu programa, DOS libera su memoria,
pero la INT 9 **sigue apuntando a tu función**, que ya no existe.

La siguiente tecla que toque quien sea salta a memoria que ahora es otra cosa.
La máquina se cuelga.

Por eso el juego sale con ESC y no cerrando la ventana: para que le dé tiempo a
devolver el vector.

## 6. El bucle principal

Este es el esqueleto de cualquier juego, y ya lo tienes entero:

```c
	do {
		/* 1. leer la entrada  */
		/* 2. actualizar       */
		/* 3. dibujar          */
		/* 4. esperar y volcar */
	} while (!keys[KEY_ESC]);
```

Gira sin parar. En cada vuelta **mira** cómo está el teclado, no espera a que
pases nada. Por eso el tanque avanza mientras mantienes la flecha.

## 7. Por qué no se puede ir en diagonal

```c
		if (keys[KEY_UP]){
			...
		}else if (keys[KEY_DOWN]){
```

`if / else if`, no cuatro `if` sueltos. Con dos flechas pulsadas solo entra la
primera.

**Eso es una decisión de diseño, no una limitación.** Un tanque que se mueve en
ocho direcciones necesita ocho juegos de sprites y un sistema de colisiones
distinto. Con cuatro, todo es más simple y el juego se controla mejor.

Si quitas los `else` y compilas, el tanque va en diagonal y verás que el sprite
no acompaña: mira a un lado y se mueve a otro.

## 8. Experimentos

1. **Quita los `else`.** Diagonales, y el sprite descuadrado.
2. **Imprime el scancode** de lo que pulses. En modo texto, antes de entrar en
   gráficos: `while(1){ if(kbhit()) printf("%02X ", getch()); }`. Así sacas los
   scancodes de las teclas que quieras.
3. **Comenta el `outp(0x20, 0x20)`.** El teclado responde una vez y se muere.
4. **Comenta el `uninstall_kbd()`.** Sal del programa y pulsa una tecla en
   DOS. (Hazlo en DOSBox, no en una máquina con cosas sin guardar.)
5. **Sube el paso de 2 a 6.** Más rápido, y se nota que el movimiento va a
   saltos. Ese número es el `PIXEL_TO_MOVE` del juego.

## 9. Lo que hay que llevarse

| | |
|---|---|
| `getch()` bloquea y no da teclas mantenidas | Por eso no sirve |
| El teclado manda **scancodes**, no letras | Posición física, no carácter |
| **El bit 7 distingue pulsar de soltar** | De ahí salen las teclas simultáneas |
| El manejador necesita `interrupt far`, `volatile` y el `outp(0x20,0x20)` | Los tres o nada |
| **Devolver el vector al salir** | Si no, cuelgas la máquina |

---

**Anterior:** [Capítulo 4](../../ch04/doc/README.md) ·
**Siguiente:** [Capítulo 6 — Animación](../../ch06/doc/README.md)
