//BITMAP LOADER BY TSW 2012. V2.0 with VESA MODE
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Code for Borland Turbo C++ 3.0
//----------------------------------------------

#include <iostream.h>
#include <stdio.h>
#include <stdlib.h>   //LIBRERIAS
#include <conio.h>
#include <string.h>
#include <dos.h>


void carga_total(char *fichero);
void lee_datos(long lugar);
unsigned char *vga = (unsigned char *) MK_FP(0xA000,0); //defino puntero FAR ( con segmento y desplazamiento ) 
                                                        //que apunta a la MEMORIA DE VIDEO 0xA000

FILE *archivo = NULL;
void main(){

	//La llamada la hago varias veces para comprobar la velocidad
	//de carga entre imagen e imagen.

   cout << "BITMAP LOADER 2.0\n";
   cout << "Pulse una tecla para empezar la carga ...\n";
   getch();
   carga_total("c:\\ibiza.bmp");
   getch();
   carga_total("c:\\ibiza.bmp");
}

void carga_total(char *fichero)
{

	char *fil = fichero;
	archivo = fopen(fil,"rb"); //binario
	if(archivo == NULL )
		printf("ERROR GRAVE !!! NO SE HA PODIDO ABRIR EL ARCHIVO!!!\n");


	unsigned char valor;
	unsigned char r,v,a,c;
	unsigned int cuenta_colores;
	cuenta_colores = 0;
    //Elijo tipo de resolucion en modo VESA
	asm{
	  mov ax,4F02h
      mov bx,107h
      int 10h
	}


	//Me situo justo donde empieza la paleta de colores
	fseek(archivo,54L,SEEK_SET); //--- numero colores bmp
                                 // Voy leyendo de 4 en 4 bytes, el nÂº 4 nulo, solo valen
                                 // los 3 primeros ( mira la documentacion del bmp )

   do{

  		fread(&valor,1,1,archivo);

		a = (valor/4); //divido entre 4 porque el valor maximo tiene que
						//ser 63 porque los colores usan 6 bits, no 8.
						//00111111 = 63. (de 0 a 63 ) 64 combinaciones de color

		fread(&valor,1,1,archivo);
		v = (valor/4);

		fread(&valor,1,1,archivo);
		r = (valor/4);

		fread(&valor,1,1,archivo);

       outportb(0x3c8,cuenta_colores); //envio cada color al puerto de la VGA. Al DAC
  	   outportb(0x3c9,r);  //r
  	   outportb(0x3c9,v);  //v
  	   outportb(0x3c9,a);  //a

   	cuenta_colores = cuenta_colores + 1;
    
	}while(cuenta_colores <= 255);


    int banco = 0;
	long lugar = 1078; //me situo justo donde empiezan los datos del dibujo
					   //los colores.
                       //En este caso la resolucion pide 20 bancos de memoria 
                       //creo que es 1300x1200 dividido entre 65535 = 20
                       //no me acuerdo que resolucion , busca resoluciones VESA, te apareceran
                       //los valores disponibles

	
   for ( int i = 0; i < 20 ; i++) //pues de 0 a 20 bancos
   {

      asm{            //cambio de banco ...0,1,2,3... 20
         mov ax,4F05h
         xor bx,bx
         mov dx,[i]   //elijo el banco y cambio
         int 10h
      }


   	lee_datos(lugar); //funcion que llena de UN GOLPE 65535 bytes
   	                  //por lo tanto gana velocidad y la carga es automatica
      lugar = lugar + 65535; //en el archivo me voy moviendo de 65535 en
      //65535 , para el siguiente banco
   }

   fclose(archivo);
}


void lee_datos(long lugar){

  	fseek(archivo,lugar,SEEK_SET); //me situo en el sitio indicado
	fread(vga,65535,1,archivo);    //escribo en la memoria de video 65535 bytes
								   //desde donde diga el puntero del archivo.
								   //la funcion fread permite eso.
}