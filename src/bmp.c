/* BMP HAndler */
#include "header/bmp.h"
#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <alloc.h>


FILE *archivo = NULL;
unsigned char *vga = (unsigned char *) MK_FP(0xA000,0); 
unsigned char *buffer_bmp = NULL;
unsigned char *buffer_bmp_reverted = NULL;


unsigned char *buffer_image_data = NULL;
unsigned char *buffer_palleta_data = NULL;




void hello_bmp(){
	printf("helloooo BMPPPPo noooo\n");
}


void revert_bmp(char *buffer){
	
	unsigned char temp_linea[320];  // buffer temporal para una linea
    int y;
    unsigned char *linea_superior;
    unsigned char *linea_inferior;
    
    for(y = 0; y < HEIGHT / 2; y++) {
	    
        linea_superior = buffer + (y * WIDTH);
        linea_inferior = buffer + ((HEIGHT - 1 - y) * WIDTH);
        
        // Guardar linea superior en temp
        memcpy(temp_linea, linea_superior, WIDTH);
        
        // Copiar linea inferior a superior
        memcpy(linea_superior, linea_inferior, WIDTH);
        
        // Copiar temp (ex-superior) a inferior
        memcpy(linea_inferior, temp_linea, WIDTH);
    }
	
}

void write_pallete_data_into_dac(char *pallete_data){
	
	unsigned int pallete_data_index = 0;
	unsigned int buffer_data_index = 0;
	unsigned int cuenta_colores = 0;
	unsigned char r,v,a,c;
	
	for(pallete_data_index = 0 ; pallete_data_index <= 255 ; pallete_data_index++){
		
		// BLUE	
		a = pallete_data[pallete_data_index];
		pallete_data_index++;

		// GREEN	
		v = pallete_data[pallete_data_index];
		pallete_data_index++;

		// RED
		r = pallete_data[pallete_data_index];
		pallete_data_index++;
	

		// EMPTY but we must read. Pallete data is 4 bytes by 4 bytes	
		pallete_data_index++;

       outportb(0x3c8,cuenta_colores); //envio cada color al puerto de la VGA. Al DAC
  	   outportb(0x3c9,r);  //r
  	   outportb(0x3c9,v);  //v
  	   outportb(0x3c9,a);  //a
  	   
	}
    
	
}


void load_pallete_data(char *buffer_data_dest , FILE *archivo){
	
	int buffer_data_index = 0;
	unsigned char valor;
	unsigned char r,v,a,c;
	unsigned int cuenta_colores;
	
	//point to starting pallete_data info in file
	fseek(archivo, 54L, SEEK_SET);
	
	do{

		// BLUE	
  		fread(&valor,1,1,archivo);
		a = (valor/4);
		buffer_data_dest[buffer_data_index] = a;
		buffer_data_index++;

		// GREEN	
		fread(&valor,1,1,archivo);
		v = (valor/4);
		buffer_data_dest[buffer_data_index] = v;
		buffer_data_index++;

		// RED
		fread(&valor,1,1,archivo);
		r = (valor/4);
		buffer_data_dest[buffer_data_index] = r;
		buffer_data_index++;

		// EMPTY but we must read. Pallete data is 4 bytes by 4 bytes	
		fread(&valor,1,1,archivo);
		buffer_data_index++;

   		cuenta_colores = cuenta_colores + 1;
    
	}while(cuenta_colores <= 255);
	
}


void load_image_data_from_file(char *buffer_data_dest, FILE *file){

		//set where the image data beggin
		fseek(file, 1078L , SEEK_SET);
		fread(buffer_data_dest,65535,1,file);
	
}

void paint_image_data_to_vga(char *buffer_image_data){
	//fread(vga,65535,1,buffer_image_data);    //escribo en la memoria de video 65535 bytes	
	memcpy(vga,buffer_image_data,65535);
}

void init_buffers(){
	
	//Create buffer bmp
	buffer_bmp = (char*)malloc(65535 * sizeof(char*));
	if(buffer_bmp == NULL){
		printf("Error creating buffer_bmp\n");	
    }
    
    //Create buffer image data
    buffer_image_data = (char*)malloc(64456 * sizeof(char*));
	if(buffer_image_data == NULL){
		printf("Error creating buffer_image_data\n");	
    }
    
    buffer_palleta_data = (char*)malloc(309 * sizeof(char*));
	if(buffer_palleta_data == NULL){
		printf("Error creating buffer_palleta_data\n");	
    }
	
}

void load_background_game(char *fichero)
{
	
	unsigned char valor;
	unsigned char r,v,a,c;
	unsigned int cuenta_colores = 0;
	int i = 0;
	int palleta_index = 0;
	long lugar = 1078; //me situo justo donde empiezan los datos del dibujo
	char *fil = fichero;
	
	archivo = fopen(fil,"rb"); //binario
	if(archivo == NULL ){
		printf("ERROR GRAVE !!! NO SE HA PODIDO ABRIR EL ARCHIVO!!!\n");
	}
	
	// Create all buffers	
	init_buffers();
	
	//Fill ALL file data into buffer_bmp ( not reverted ) 
    fread(buffer_bmp,65535,1,archivo);
    
    //now revert the BMP data because the BMP data in original is reverted
    //revert_bmp(buffer_bmp);
    
    
    //First step is load the PALLETE_DATA of image
	load_pallete_data(buffer_palleta_data , archivo);    
	
	//Set the pallete data into the VGA DAC
	write_pallete_data_into_dac(buffer_palleta_data);
	
	// Load the image data
	load_image_data_from_file(buffer_image_data, archivo);
	
	// Paint the image in video memory
	paint_image_data_to_vga(buffer_image_data);
	

   
   free(buffer_bmp);
   free(buffer_image_data);
   free(buffer_palleta_data);
   fclose(archivo);
   
}


void lee_datos(long lugar){
	
	int image_data_index = 0;	
  	//fseek(archivo,lugar,SEEK_SET); //me situo en el sitio indicado
  	//fread(vga,65535,1,archivo);    //escribo en la memoria de video 65535 bytes
  	
  	for(image_data_index = lugar; image_data_index <= 64457 ; image_data_index++){
	  	  	
	}
  	
  	
	fread(vga,65535,1,buffer_bmp);    //escribo en la memoria de video 65535 bytes
	
								   //desde donde diga el puntero del archivo.
								   //la funcion fread permite eso.
							   
}