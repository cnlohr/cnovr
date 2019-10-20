#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define CNRBTREE_IMPLEMENTATION
#include <cnrbtree.h>

CNRBTREETEMPLATE( int, int, RBptrcmp, RBptrcpy, RBnullop );

char ** SplitStrings( const char * line, char * split, char * white, int merge_fields, int * fieldcount );
char * FileToString( const char * fname, int * length );

int main()
{
	int flen;
	char * hipdat = FileToString( "hip2.dat", &flen );
	if( !hipdat )
	{
		printf( "Couldn't get HIP data.\n" );
		exit( -5 );
	}
	int lines;
	char ** filelines = SplitStrings( hipdat, "\n", "", 0, &lines );
	printf( "File read (%d lines)\n", lines );
	int i;
	char * l;
	FILE * flatstars = fopen( "flat_stars.dat", "wb" );
	FILE * flatstarscsv = fopen( "flat_stars.csv", "w" );

	// http://cdsarc.u-strasbg.fr/ftp/I/311/ReadMe says:
	/*

 1   5 0 1  0.0000159148  0.0190068680    4.55    -4.55    -1.19   1.29   0.66   1.33   1.25   0.75  90  0.91  0    0.0    0  9.2043 0.0020 0.017 0  0.482 0.025  0.550   1.19  -0.71   1.00  -0.02   0.02   1.00   0.45  -0.05   0.03   1.09  -0.41   0.09   0.08  -0.60   1.00
 2  75 4 1  0.0000662767 -0.3403189126   20.85   182.88    -1.31   0.95   0.53   1.13   1.22   0.66 121  0.06  0    0.0    0  9.4017 0.0017 0.015 0  0.999 0.002  1.040   1.11  -0.33   1.03   0.19  -0.01   1.08   0.27  -0.04  -0.34   1.00  -0.14   0.38   0.04  -0.10   1.00
 3   5 0 1  0.0000874051  0.6782224871    2.26     4.27    -3.43   0.31   0.21   0.36   0.34   0.27 129  1.56  0    0.0    0  6.6081 0.0006 0.008 0 -0.019 0.004  0.000   1.13  -0.32   1.03  -0.42   0.13   1.00  -1.68   0.20   0.16   1.03   0.41  -1.12  -0.14  -0.94   1.00

Field   Do we care    Star data near 
0			~	   1-  6  I6    ---      HIP     Hipparcos identifier
1				   8- 10  I3    ---      Sn      [0,159] Solution type new reduction (1)
2					  12  I1    ---      So      [0,5] Solution type old reduction (2)
3					  14  I1    ---      Nc      Number of components
4			Y	  16- 28 F13.10 rad      RArad   Right Ascension in ICRS, Ep=1991.25
5			Y	  30- 42 F13.10 rad      DErad   Declination in ICRS, Ep=1991.25
6			Y	  44- 50  F7.2  mas      Plx     Parallax
7				  52- 59  F8.2  mas/yr   pmRA    Proper motion in Right Ascension
8				  61- 68  F8.2  mas/yr   pmDE    Proper motion in Declination
9				  70- 75  F6.2  mas    e_RArad   Formal error on RArad
10				  77- 82  F6.2  mas    e_DErad   Formal error on DErad
11				  84- 89  F6.2  mas    e_Plx     Formal error on Plx
12				  91- 96  F6.2  mas/yr e_pmRA    Formal error on pmRA
13				  98-103  F6.2  mas/yr e_pmDE    Formal error on pmDE
14				 105-107  I3    ---      Ntr     Number of field transits used
15				 109-113  F5.2  ---      F2      Goodness of fit
16				 115-116  I2    %        F1      Percentage rejected data
17				 118-123  F6.1  ---      var     Cosmic dispersion added (stochastic solution)
18				 125-128  I4    ---      ic      Entry in one of the suppl.catalogues
19			Y	 130-136  F7.4  mag      Hpmag   Hipparcos magnitude
20				 138-143  F6.4  mag    e_Hpmag   Error on mean Hpmag
21				 145-149  F5.3  mag      sHp     Scatter of Hpmag
22					 151  I1    ---      VA      [0,2] Reference to variability annex
23			Y	 153-158  F6.3  mag      B-V     Colour index
24				 160-164  F5.3  mag    e_B-V     Formal error on colour index
25			Y	 166-171  F6.3  mag      V-I     V-I colour index
26				 172-276 15F7.2 ---      UW      Upper-triangular weight matrix (G1)
	*/

/* Interesting links:
	* https://stackoverflow.com/questions/21977786/star-b-v-color-index-to-apparent-rgb-color
*/


#define RECSTR( w, x ) const char * w = #x; x

	RECSTR( flatstartype, typedef struct __attribute__((__packed__)) { \
		uint32_t rascention_bams; \
		int32_t  declination_bams; \
		int16_t parallax_10uas; \
		uint16_t magnitude_mag1000; \
		int16_t bvcolor_mag1000; \
		int16_t vicolor_mag1000; \
	} flat_star; );

	cnrbtree_intint * hip_to_line = cnrbtree_intint_create();
	int outstars = 0;
	for( i = 0; l = filelines[i]; i++ )
	{
		int fieldct;
		char ** fields = SplitStrings( l, " ", "", 1, &fieldct );
		if( fieldct == 0 ) continue;
		if( fieldct != 41 ) { fprintf( stderr, "Error in dataset on line %d; Wrong # of fields (Got %d)\n", i+1, fieldct ); }
		int hc = atoi( fields[0] );
		flat_star s;
		memset( &s, 0, sizeof( s  ));
		double tmp;
		RBA( hip_to_line, hc ) = i;
		s.rascention_bams =  ( ( tmp = atof( fields[4] ) )/6.28318530718)*4294967295;
		s.declination_bams = ( ( tmp = atof( fields[5] ) )/3.14159265359)*2147483647;
		s.parallax_10uas = ( tmp = atof( fields[6] ) ) * 100;
		s.magnitude_mag1000 = ( tmp = atof( fields[19] ) ) * 1000;
		s.bvcolor_mag1000 = ( tmp = atof( fields[23] ) ) * 1000;
		s.vicolor_mag1000 = ( tmp = atof( fields[25] ) ) * 1000;
		fprintf( flatstarscsv, "%s,%f,%f,%s,%s,%s,%s\n", fields[0], atof( fields[4] )/6.28318530718*360.0, atof( fields[5] )/6.28318530718*360.0, fields[6], fields[19], fields[23], fields[25] );
		fwrite( &s, sizeof(s), 1, flatstars ); outstars++;
	}
	fclose( flatstars );
	fclose( flatstarscsv );
	free( filelines );
	free( hipdat );
	printf( "Wrote out %d stars\n", outstars );

	//Now we need to parse constellationship.fab
	char * constellationdata = FileToString( "constellationship.fab", &flen );
	if( !constellationdata )
	{
		fprintf( stderr, "Couldn't get constellationship data.\n" );
		exit( -5 );
	}
	char ** constellations = SplitStrings( constellationdata, "\n", "", 0, &lines );
	printf( "File read (%d constellation lines)\n", lines );
	FILE * ffh = fopen( "flat_stars.h", "wb" );

	RECSTR( constellationtype, typedef struct { char cname[3]; uint8_t lines; } constellation; );

	int32_t  * linesegs = malloc( 0 );
	int outlinesegs = 0;
	int curlineseg = 0;
	fprintf( ffh, "#include <stdint.h>\n%s\n%s\n",  flatstartype, constellationtype );
	fprintf( ffh, "constellation constellations[] = {\n" );
	int outconsts = 0;
	for( i = 0; l = constellations[i]; i++ )
	{
		int fieldct;
		char ** fields = SplitStrings( l, " ", "", 1, &fieldct );
		if( fieldct == 0 ) continue;
		if( fieldct < 2 )
		{
			fprintf( stderr, "Warning constellationship line %d bad. [1]\n", i+ 1 );
			continue;
		}
		int expected_line_segs = atoi( fields[1] );
		if( fieldct != expected_line_segs * 2 + 2 )
		{
			fprintf( stderr, "Warning constellationship line %d bad. [2]\n", i+ 1  );
			continue;
		}
		fprintf( ffh, "\t{ {'%c', '%c', '%c'}, %d },\n", fields[0][0], fields[0][1], fields[0][2], expected_line_segs );
		int k;
		outlinesegs += expected_line_segs * 2;
		linesegs = realloc( linesegs, outlinesegs*4 );
		for( k = 0; k < expected_line_segs*2; k++ )
		{
			linesegs[curlineseg] = RBA( hip_to_line, atoi( fields[2+k] ) );
			curlineseg++;
		}
		outconsts++;
	}
	fprintf( ffh, "\n};\nint32_t constellationsegs[] = {\n" );
	free( constellations );
	free( constellationdata );
	for( i = 0; i < curlineseg; i+=2 )
	{
		fprintf( ffh, "\t%6d,%6d,\n", linesegs[i+0], linesegs[i+1] );
	}
	fprintf( ffh, "};\n" );
	fclose( ffh );
	printf( "Wrote out %d constellations with %d line segments\n", outconsts, curlineseg/2 );
}

char * FileToString( const char * fname, int * length )
{
	FILE * f = fopen( fname, "rb" );
	if( !f ) return 0;
	fseek( f, 0, SEEK_END );
	*length = ftell( f );
	fseek( f, 0, SEEK_SET );
	char * ret = malloc( *length + 1 );
	int r = fread( ret, *length, 1, f );
	fclose( f );
	ret[*length] = 0;
	if( r != 1 )
	{
		free( ret );
		return 0;
	}
	return ret;
}

char ** SplitStrings( const char * line, char * split, char * white, int merge_fields, int * elementcount )
{
	if( elementcount ) *elementcount = 0;
	if( !line || strlen( line ) == 0 )
	{
		char ** ret = malloc( sizeof( char * )  );
		*ret = 0;
		return ret;
	}

	int elements = 1;
	char ** ret = malloc( elements * sizeof( char * )  );
	int * lengths = malloc( elements * sizeof( int ) ); 
	int i = 0;
	char c;
	int did_hit_not_white = 0;
	int thislength = 0;
	int thislengthconfirm = 0;
	int needed_bytes = 1;
	const char * lstart = line;
	do
	{
		int is_split = 0;
		int is_white = 0;
		char k;
		c = *(line);
		for( i = 0; (k = split[i]); i++ )
			if( c == k ) is_split = 1;
		for( i = 0; (k = white[i]); i++ )
			if( c == k ) is_white = 1;

		if( c == 0 || ( ( is_split ) && ( !merge_fields || did_hit_not_white ) ) )
		{
			//Mark off new point.
			lengths[elements-1] = (did_hit_not_white)?(thislengthconfirm + 1):0; //XXX BUGGY ... Or is bad it?  I can't tell what's wrong.  the "buggy" note was from a previous coding session.
			ret[elements-1] = (char*)lstart + 0; //XXX BUGGY //I promise I won't change the value.
			needed_bytes += thislengthconfirm + 1;
			elements++;
			ret = realloc( ret, elements * sizeof( char * )  );
			lengths = realloc( lengths, elements * sizeof( int ) );
			lengths[elements-1] = 0;
			lstart = line;
			thislength = 0;
			thislengthconfirm = 0;
			did_hit_not_white = 0;
			line++;
			continue;
		}

		if( !is_white && ( !(merge_fields && is_split) ) )
		{
			if( !did_hit_not_white )
			{
				lstart = line;
				thislength = 0;
				did_hit_not_white = 1;
			}
			thislengthconfirm = thislength;
		}

		if( is_white )
		{
			if( did_hit_not_white ) 
				is_white = 0;
		}

		if( did_hit_not_white )
		{
			thislength++;
		}
		line++;
	} while ( c );

	//Ok, now we have lengths, ret, and elements.
	ret = realloc( ret, ( sizeof( char * ) + 1 ) * elements  + needed_bytes );
	char * retend = ((char*)ret) + ( (sizeof( char * )) * elements);
	int lensum1 = 0;
	for( i = 0; i < elements; i++ )
	{
		int len = lengths[i];
		lensum1 += len + 1;
		memcpy( retend, ret[i], len );
		retend[len] = 0;
		ret[i] = (i == elements-1)?0:retend;
		retend += len + 1;
	}
	if( elementcount && elements ) *elementcount = elements-1;
	free( lengths );
	return ret;
}
