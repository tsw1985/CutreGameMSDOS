/* BMP HAndler */
#include "header\bmp.h"
#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <alloc.h>


FILE *file_background_image_game = NULL;
FILE *file_sprites_game = NULL;



unsigned char *vga = (unsigned char *) MK_FP(0xA000,0); 
unsigned char *buffer_original_background_bmp = NULL;
unsigned char *buffer_bmp_reverted = NULL;
unsigned char *buffer_image_data = NULL;
unsigned char *buffer_palleta_data = NULL;

//SPRITES BUFFER
unsigned char *buffer_sprites_data = NULL;


void bmp_revert_bmp(char *buffer){
	
	unsigned char temp_line[320];  // buffer temporal para una linea
    int y;
    unsigned char *up_line;
    unsigned char *bottom_line;
    
    for(y = 0; y < HEIGHT / 2; y++) {
        up_line = buffer + (y * WIDTH);
        bottom_line = buffer + ((HEIGHT - 1 - y) * WIDTH);
        
        memcpy(temp_line, up_line, WIDTH);
        memcpy(up_line, bottom_line, WIDTH);
        memcpy(bottom_line, temp_line, WIDTH);
    }
	
}

void bmp_write_pallete_data_into_dac(char *pallete_data){
	
	unsigned int buffer_data_index = 0;
	unsigned int color_counter = 0;
	unsigned char r,v,a;
	
	for(color_counter = 0 ; color_counter <= 255 ; color_counter++){
		
		a = pallete_data[buffer_data_index++];  // Blue
		v = pallete_data[buffer_data_index++];  // Green
		r = pallete_data[buffer_data_index++];  // Red

		outportb(0x3c8, color_counter);
  	   	outportb(0x3c9, r);
  	   	outportb(0x3c9, v);
  	   	outportb(0x3c9, a);
	}
}

void bmp_load_pallete_data(char *buffer_data_dest , FILE *_file){
	
	int buffer_data_index = 0;
	unsigned char value;
	unsigned char r,v,a;
	unsigned int color_counter = 0; 
	
	//point to starting pallete_data info in file
	fseek(_file, 54L, SEEK_SET);
	
	do{
		
		// BLUE	
  		fread(&value,1,1,_file);
		a = (value/4);
		buffer_data_dest[buffer_data_index++] = a;  

		// GREEN	
		fread(&value,1,1,_file);
		v = (value/4);
		buffer_data_dest[buffer_data_index++] = v;

		// RED
		fread(&value,1,1,_file);
		r = (value/4);
		buffer_data_dest[buffer_data_index++] = r;

		// EMPTY but we must read
		fread(&value,1,1,_file);

   		color_counter++;
    
	}while(color_counter <= 255);
}


void bmp_load_image_data_from_file(char *buffer_data_dest, FILE *file){
		//set where the image data beggin
		fseek(file, 1078L , SEEK_SET);
		fread(buffer_data_dest,65535,1,file);
}

void bmp_paint_image_data_to_vga(char *buffer_image_data){
	memcpy(vga,buffer_image_data,65535);
}

void bmp_init_buffers(){
	
	//Create buffer bmp
	buffer_original_background_bmp = (char*)malloc(65535 * sizeof(char*));
	if(buffer_original_background_bmp == NULL){
		printf("Error creating buffer_original_background_bmp\n");	
    }
    
    //Create buffer image data
    buffer_image_data = (char*)malloc(IMAGE_DATA_SIZE * sizeof(char*));
	if(buffer_image_data == NULL){
		printf("Error creating buffer_image_data\n");	
    }
    
    //Create buffer pallete data
    buffer_palleta_data = (char*)malloc(PALLETA_DATA_SIZE * sizeof(char*));
	if(buffer_palleta_data == NULL){
		printf("Error creating buffer_palleta_data\n");	
    }
    
    //
    buffer_sprites_data = (char*)malloc(65535 * sizeof(char*));
	if(buffer_sprites_data == NULL){
		printf("Error creating buffer_sprites_data\n");	
    }
	
}

void bmp_load_background_game(char *_file)
{
	
	
	file_background_image_game = fopen(_file,"rb"); //binario
	if(file_background_image_game == NULL ){
		printf("ERROR!!! I can not open the backound image file\n");
	}
	
	// Create all buffers	
	bmp_init_buffers();
	
	//Fill ALL file data into buffer_bmp ( not reverted ) 
    fread(buffer_original_background_bmp,65535,1,file_background_image_game);
    
    //First step is load the PALLETE_DATA of image
	bmp_load_pallete_data(buffer_palleta_data , file_background_image_game);
	
	//Set the pallete data into the VGA DAC
	bmp_write_pallete_data_into_dac(buffer_palleta_data);
	
	// Load the image data
	bmp_load_image_data_from_file(buffer_image_data, file_background_image_game);
	
	//now revert the BMP data because the BMP data in original is reverted
    bmp_revert_bmp(buffer_image_data);
	
	// Paint the image in video memory
	bmp_paint_image_data_to_vga(buffer_image_data);
   
}



void bmp_load_sprites_images(char *_file_sprites_game){
	
	file_sprites_game	= fopen(_file_sprites_game,"rb"); //binario
	if(file_sprites_game == NULL ){
		printf("ERROR!!! I can not open file_sprites_game file\n");
	}
	
	// Load the sprites data . Full sprites image
	bmp_load_image_data_from_file(buffer_sprites_data,_file_sprites_game);
	
	//now revert the BMP data because the BMP data in original is reverted
    bmp_revert_bmp(buffer_sprites_data);
	
}


void bmp_close_files(){
	fclose(file_background_image_game);
}

void bmp_delete_buffers(){
	
	free(buffer_original_background_bmp);
    free(buffer_image_data);
    free(buffer_palleta_data);
    free(buffer_sprites_data);
	
}