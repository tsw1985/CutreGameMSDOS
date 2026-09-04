//BITMAP LOADER BY TSW 2012. V2.0 with VESA MODE
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Code for Borland Turbo C++ 3.0
//----------------------------------------------

#include <iostream.h>
#include <stdio.h>
#include <stdlib.h>   //LIBRARIES
#include <conio.h>
#include <string.h>
#include <dos.h>


void carga_total(char *fichero);
void lee_datos(long lugar);
unsigned char *vga = (unsigned char *) MK_FP(0xA000,0); //a FAR pointer (segment and offset) that points
                                                        //at VIDEO MEMORY, 0xA000

FILE *archivo = NULL;
void main(){

	//Called several times on purpose, to check the loading
	//speed from one image to the next.

   cout << "BITMAP LOADER 2.0\n";
   cout << "Press a key to start loading ...\n";
   getch();
   carga_total("c:\\ibiza.bmp");
   getch();
   carga_total("c:\\ibiza.bmp");
}

void carga_total(char *fichero)
{

	char *fil = fichero;
	archivo = fopen(fil,"rb"); //binary
	if(archivo == NULL )
		printf("ERROR GRAVE !!! NO SE HA PODIDO ABRIR EL ARCHIVO!!!\n");


	unsigned char valor;
	unsigned char r,v,a,c;
	unsigned int cuenta_colores;
	cuenta_colores = 0;
    //Pick the VESA resolution
	asm{
	  mov ax,4F02h
      mov bx,107h
      int 10h
	}


	//Seek to exactly where the colour palette starts
	fseek(archivo,54L,SEEK_SET); //--- number of bmp colours
                                 // Read 4 bytes at a time; the 4th is null, only the
                                 // first 3 count (see the bmp documentation)

   do{

  		fread(&valor,1,1,archivo);

		a = (valor/4); //divide by 4 because the maximum value has to be
						//63, since the colours use 6 bits, not 8.
						//00111111 = 63. (0 to 63) 64 colour combinations

		fread(&valor,1,1,archivo);
		v = (valor/4);

		fread(&valor,1,1,archivo);
		r = (valor/4);

		fread(&valor,1,1,archivo);

       outportb(0x3c8,cuenta_colores); //send each colour to the VGA port. To the DAC
  	   outportb(0x3c9,r);  //r
  	   outportb(0x3c9,v);  //v
  	   outportb(0x3c9,a);  //a

   	cuenta_colores = cuenta_colores + 1;
    
	}while(cuenta_colores <= 255);


    int banco = 0;
	long lugar = 1078; //seek to exactly where the picture data starts,
					   //after the colours.
                       //This resolution needs 20 memory banks:
                       //I think it is 1300x1200 divided by 65535 = 20
                       //I do not remember which resolution; look up the VESA
                       //resolutions and the available values will show up

	
   for ( int i = 0; i < 20 ; i++) //so, banks 0 to 20
   {

      asm{            //switch bank ...0,1,2,3... 20
         mov ax,4F05h
         xor bx,bx
         mov dx,[i]   //pick the bank and switch
         int 10h
      }


   	lee_datos(lugar); //fills 65535 bytes IN ONE GO,
   	                  //so it is faster and the load is automatic
      lugar = lugar + 65535; //move through the file 65535 bytes at a
      //time, for the next bank
   }

   fclose(archivo);
}


void lee_datos(long lugar){

  	fseek(archivo,lugar,SEEK_SET); //seek to the given place
	fread(vga,65535,1,archivo);    //write 65535 bytes into video memory
								   //from wherever the file pointer is.
								   //fread lets you do exactly that.
}