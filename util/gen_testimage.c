#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../lib/stb_image_write.h"
#include <stdio.h>
#include <stdint.h>

#define W 2048
#define H 1024
uint32_t image[H][W];

#define CELL (W/256)

int main()
{
	int x, y, i;
	for( y = 0; y < H; y++ )
		for( x = 0; x < W; x++ )
			image[y][x] = 0xff000000;
		
	// Top gradient.
	for( i = 0; i < 256; i++ )
		for( y = 0; y < 80; y++ )
			for( x = 0; x < CELL; x++ )
				if( y < 64 )
					image[y][x+i*CELL] = 0xff000000 | (i<<0) | (i<<8) | (i<<16);
				else
					image[y][x+i*CELL] = 0xff000000 | (((x%CELL)==0)?0xffffff:0x000000);

	// Green-Red
	for( i = 0; i < 256; i++ )
		for( y = 0; y < 80; y++ )
			for( x = 0; x < CELL; x++ )
				if( y < 64 )
					image[y+128*1][x+i*CELL] = 0xff000000 | (i<<0) | ((255-i)<<8);
				else
					image[y+128*1][x+i*CELL] = 0xff000000 | (((x%CELL)==0)?0xffffff:0x000000);

	// Green-Blue
	for( i = 0; i < 256; i++ )
		for( y = 0; y < 80; y++ )
			for( x = 0; x < CELL; x++ )
				if( y < 64 )
					image[y+128*2][x+i*CELL] = 0xff000000 | (i<<16) | ((255-i)<<8);
				else
					image[y+128*2][x+i*CELL] = 0xff000000 | (((x%CELL)==0)?0xffffff:0x000000);


	// RED
	for( i = 0; i < 256; i++ )
		for( y = 0; y < 80; y++ )
			for( x = 0; x < CELL; x++ )
				if( y < 64 )
					image[y+128*3][x+i*CELL] = 0xff000000 | (i<<0);
				else
					image[y+128*3][x+i*CELL] = 0xff000000 | (((x%CELL)==0)?0xffffff:0x000000);

	// GRN
	for( i = 0; i < 256; i++ )
		for( y = 0; y < 80; y++ )
			for( x = 0; x < CELL; x++ )
				if( y < 64 )
					image[y+128*4][x+i*CELL] = 0xff000000 | (i<<8);
				else
					image[y+128*4][x+i*CELL] = 0xff000000 | (((x%CELL)==0)?0xffffff:0x000000);

	// BLU
	for( i = 0; i < 256; i++ )
		for( y = 0; y < 80; y++ )
			for( x = 0; x < CELL; x++ )
				if( y < 64 )
					image[y+128*5][x+i*CELL] = 0xff000000 | (i<<16);
				else
					image[y+128*5][x+i*CELL] = 0xff000000 | (((x%CELL)==0)?0xffffff:0x000000);


	stbi_write_png( "../pics/gradienttest.png", W, H, 4, image, 4*W);
}
