/* BMP HAndler */
#include "header\bmp.h"
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
	
	unsigned int buffer_data_index = 0;
	unsigned int cuenta_colores = 0;
	unsigned char r,v,a;
	
	for(cuenta_colores = 0 ; cuenta_colores <= 255 ; cuenta_colores++){
		
		a = pallete_data[buffer_data_index++];  // Blue
		v = pallete_data[buffer_data_index++];  // Green
		r = pallete_data[buffer_data_index++];  // Red

		outportb(0x3c8, cuenta_colores);
  	   	outportb(0x3c9, r);
  	   	outportb(0x3c9, v);
  	   	outportb(0x3c9, a);
	}
}

void load_pallete_data(char *buffer_data_dest , FILE *archivo){
	
	int buffer_data_index = 0;
	unsigned char valor;
	unsigned char r,v,a;
	unsigned int cuenta_colores = 0;  // <- INICIALIZAR A 0
	
	//point to starting pallete_data info in file
	fseek(archivo, 54L, SEEK_SET);
	
	do{
		// BLUE	
  		fread(&valor,1,1,archivo);
		a = (valor/4);
		buffer_data_dest[buffer_data_index++] = a;  // <- usar buffer_data_index

		// GREEN	
		fread(&valor,1,1,archivo);
		v = (valor/4);
		buffer_data_dest[buffer_data_index++] = v;

		// RED
		fread(&valor,1,1,archivo);
		r = (valor/4);
		buffer_data_dest[buffer_data_index++] = r;

		// EMPTY but we must read
		fread(&valor,1,1,archivo);

   		cuenta_colores++;
    
	}while(cuenta_colores <= 255);
}


void load_image_data_from_file(char *buffer_data_dest, FILE *file){

		//set where the image data beggin
		fseek(file, 1078L , SEEK_SET);
		fread(buffer_data_dest,65535,1,file);
	
}

void paint_image_data_to_vga(char *buffer_image_data){
	memcpy(vga,buffer_image_data,65535);
}

void init_buffers(){
	
	//Create buffer bmp
	buffer_bmp = (char*)malloc(65535 * sizeof(char*));
	if(buffer_bmp == NULL){
		printf("Error creating buffer_bmp\n");	
    }
    
    //Create buffer image data
    buffer_image_data = (char*)malloc(IMAGE_DATA_SIZE * sizeof(char*));
	if(buffer_image_data == NULL){
		printf("Error creating buffer_image_data\n");	
    }
    
    buffer_palleta_data = (char*)malloc(PALLETA_DATA_SIZE * sizeof(char*));
	if(buffer_palleta_data == NULL){
		printf("Error creating buffer_palleta_data\n");	
    }
	
}

void load_background_game(char *fichero)
{
	
	unsigned char valor;
	unsigned char r,v,a,c;

	char *fil = fichero;
	
	archivo = fopen(fil,"rb"); //binario
	if(archivo == NULL ){
		printf("ERROR GRAVE !!! NO SE HA PODIDO ABRIR EL ARCHIVO!!!\n");
	}
	
	// Create all buffers	
	init_buffers();
	
	//Fill ALL file data into buffer_bmp ( not reverted ) 
    fread(buffer_bmp,65535,1,archivo);
    
    //First step is load the PALLETE_DATA of image
	load_pallete_data(buffer_palleta_data , archivo);    
	
	//Set the pallete data into the VGA DAC
	write_pallete_data_into_dac(buffer_palleta_data);
	
	// Load the image data
	load_image_data_from_file(buffer_image_data, archivo);
	
	//now revert the BMP data because the BMP data in original is reverted
    revert_bmp(buffer_image_data);
	
	// Paint the image in video memory
	paint_image_data_to_vga(buffer_image_data);
	

   
   free(buffer_bmp);
   free(buffer_image_data);
   free(buffer_palleta_data);
   fclose(archivo);
   
}
