//Tool for building a bunch of files into memory.

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

char replacename[PATH_MAX+5];

int main( int argc, char ** argv )
{
	if( argc < 2 )
	{
		fprintf( stderr, "Error: builtin_assets usage:  builtin_assets outfile.c file1 file2 file3\n" );
		exit( -5 );
	}
	FILE * f = fopen( argv[1], "w" );
	int i;
	for( i = 2; i < argc; i++ )
	{
		const char * fname = argv[i];
		FILE * ifa = fopen( fname, "rb" );
		if( !ifa )
		{
			fprintf( stderr, "WARNING: Could not find file \"%s\\n", argv[i] );
			continue; 
		}
		fseek( ifa, 0, SEEK_END );
		int len = ftell( ifa );
		fseek( ifa, 0, SEEK_SET );
		char rename[PATH_MAX];

		replacename[0] = 'I';
		replacename[1] = 'O';
		replacename[2] = 'F';
		replacename[3] = '_';
		char c;
		int j;
		for( j = 0; j < PATH_MAX; j++ )
		{
			c = fname[j];
			if( c == '/' ) c = '_';
			if( c == '.' ) c = '_';
			replacename[j+4] = c;
			if( c == 0 ) break;
		}
		fprintf( f, "__attribute__((used)) const unsigned char %s[] = { 0x%02x, 0x%02x, 0x%02x, 0x%02x, ", replacename, len & 0xff, (len>>8)&0xff, (len>>16)&0xff, (len>>24)&0xff );
		for( j = 0; j < len; j++ )
		{
			if( !(j & 0xf) ) fprintf( f, "\n\t" );
			fprintf( f, "0x%02x, ", getc( ifa ) );
		}
		fprintf( f, "};\n" );
	}
}

