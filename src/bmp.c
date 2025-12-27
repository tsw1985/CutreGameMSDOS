/* BMP HAndler */
#include "header/bmp.h"
#include <stdio.h>
#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <alloc.h>


FILE *archivo = NULL;
unsigned char *vga = (unsigned char *) MK_FP(0xA000,0); 
unsigned char *buffer_bmp = NULL;

void hello_bmp(){
	printf("helloooo BMPPPPo noooo\n");
}




void load_back_ground_game(char *fichero)
{
	
	
	
	
	unsigned char valor;
	unsigned char r,v,a,c;
	unsigned int cuenta_colores = 0;
	int i,banco = 0;
	long lugar = 1078; //me situo justo donde empiezan los datos del dibujo
	char *fil = fichero;
	
	archivo = fopen(fil,"rb"); //binario
	if(archivo == NULL )
		printf("ERROR GRAVE !!! NO SE HA PODIDO ABRIR EL ARCHIVO!!!\n");

	
	//Create buffer bmp
	buffer_bmp = (char*)malloc(64000 * sizeof(char*));
	if(buffer_bmp == NULL){
		printf("Error creating buffer_bmp\n");	
    }else{
	    printf("OK creating buffer_bmp\n");	
	}
	
	


	//Me situo justo donde empieza la paleta de colores
	fseek(archivo,54L,SEEK_SET); //--- numero colores bmp
                                 // Voy leyendo de 4 en 4 bytes, el nº 4 nulo, solo valen
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


    
					   //los colores.
                       //En este caso la resolucion pide 20 bancos de memoria 
                       //creo que es 1300x1200 dividido entre 65535 = 20
                       //no me acuerdo que resolucion , busca resoluciones VESA, te apareceran
                       //los valores disponibles

	
   for ( i = 0; i < 20 ; i++) //pues de 0 a 20 bancos
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
   free(buffer_bmp);
   
}


void lee_datos(long lugar){
	

  	fseek(archivo,lugar,SEEK_SET); //me situo en el sitio indicado
	fread(vga,65535,1,archivo);    //escribo en la memoria de video 65535 bytes
								   //desde donde diga el puntero del archivo.
								   //la funcion fread permite eso.
							   
}