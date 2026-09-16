#ifndef DEMO_OUTRO_H
#define DEMO_OUTRO_H

#include "demos\demolib.h"

//-----------------------------------------------------------
// EL EPILOGO. No es un efecto de la tabla y por eso no tiene la firma de
// los otros veinte: no recibe end_tick porque no termina sola. Termina
// cuando alguien pulsa una tecla.
//
// Meterla en demo_effects[] colgaria la presentacion en la foto que le
// tocara, asi que la llama demo_run() a mano, al final y una sola vez.
//-----------------------------------------------------------
int demo_outro(unsigned char *image,
            unsigned char *screen,
            unsigned char *palette);

#endif
