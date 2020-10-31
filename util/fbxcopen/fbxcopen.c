#include "fbxcopen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined( FBXCUSEPUFF )

#include <puff.h>
#define localpuff puff

#else

#include <zlib.h>
int localpuff(unsigned char *dest,      /* pointer to destination pointer */
         unsigned long *destlen,        /* amount of output space */
         /*const*/ unsigned char *source,   /* pointer to source data pointer */
         unsigned long *sourcelen)      /* amount of input available */
{
	int ret;
	unsigned have;
	z_stream strm;

	/* allocate inflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;

	strm.avail_in = *sourcelen;
	strm.next_in = source - 2; //TRICKY: zlib wants these extra 2 bytes?
	strm.avail_out = *destlen;
	strm.next_out = dest;

	ret = inflateInit(&strm);
	if (ret != Z_OK)
		return ret;

	ret = inflate(&strm, /*Z_NO_FLUSH*/0);

	switch (ret) {
	case Z_STREAM_ERROR:
	 	(void)inflateEnd(&strm);
		return ret;
	case Z_NEED_DICT:
		ret = Z_DATA_ERROR;     /* and fall through */
	case Z_DATA_ERROR:
	case Z_MEM_ERROR:
		(void)inflateEnd(&strm);
		return ret;
	case Z_STREAM_END:
	case Z_OK:
	 	(void)inflateEnd(&strm);

		*sourcelen = strm.avail_in;
		*destlen = strm.avail_out;

		return Z_OK;
	default:
		return ret; //unknown error;
	}
}

#endif

#define loadStatus_RESP


fbxcopen * FBXCOpenFile( const char * fileName, FBXCErrorCB errorcb, void * opaqueO )
{
	fbxcopen * ret = 0;

	FILE * f = fopen( fileName, "rb" );
	if( !f )
	{
		char errorstat[512];
		ret = calloc( 1, sizeof( fbxcopen ) );
		ret->opaqueO = opaqueO;
		snprintf( errorstat, sizeof(errorstat)-1, "Error opening: Could not open filename %s", fileName );
		if( errorcb )
		{
			errorcb( ret, 0, errorstat );
		}
		else
		{
			printf( "%s\n", errorstat );
		}
		free( ret );
		return 0;
	}
	fseek( f, 0, SEEK_END );
	int len = ftell( f );
	fseek( f, 0, SEEK_SET );
	uint8_t * data = malloc( len );
	int r = fread( data, len, 1, f );
	fclose( f );
	if( r == 1 && len > 0 )
	{
		ret = FBXCOpenData( data, 1, len, errorcb, opaqueO );
	}
	else
	{
		char errorstat[512];
		ret = calloc( 1, sizeof( fbxcopen ) );
		ret->opaqueO = opaqueO;
		snprintf( errorstat, sizeof(errorstat)-1, "File unreadable or zero bytes %s", fileName );
		if( errorcb )
		{
			errorcb( ret, 0, errorstat );
		}
		else
		{
			printf( "%s\n", errorstat );
		}
		free( ret );
		return 0;
	}
	return ret;
}


void FBXCOpenCopyContext( fbxcopen_context * to, fbxcopen_context * from, void * opaqueC )
{
	memcpy( to, from, sizeof( fbxcopen_context ) );
	to->opaqueC = opaqueC;
}

void FBXCOpenMakeContext( fbxcopen * fbxc, fbxcopen_context * ctx, void * opaqueC )
{
	memset( ctx, 0, sizeof(*ctx) );
	ctx->fbx = fbxc;
	ctx->opaqueC = opaqueC;
	FBXCOpenResetContext( ctx, -1 );
}

void FBXCOpenResetContext( fbxcopen_context * ctx, int offset )
{
	//actual start of first node (MAGIC NUMBER!)
	ctx->place = (offset<27)?27:offset;
	ctx->error = 0;
}

void FBXCOpenDestroy( fbxcopen * fbxc )
{
	if( !fbxc ) return;
	if( fbxc->data && fbxc->freeWhenDone ) free( fbxc->data );
	free( fbxc );
}





static void DumpBytes( fbxcopen_context * ths, int numbytes )
{
	fbxcopen * fbx = ths->fbx;
	ths->place += numbytes;
	if( ths->place > fbx->len ) ths->place = fbx->len;
}

static uint8_t Pop1( fbxcopen_context * ths )
{
	fbxcopen * fbx = ths->fbx;
	if( ths->error ) return 0;
	if( ths->place + 1 > ths->fbx->len )
	{
		ths->error = -101;
		if( fbx->errorcb ) fbx->errorcb( fbx, ths, "FBX EOF Reached when trying to Pop1." ); 
		return 0;
	}
	uint8_t * rd = fbx->data + ths->place;
	ths->place += 1;
	return rd[0];
}

static uint16_t Pop2( fbxcopen_context * ths )
{
	fbxcopen * fbx = ths->fbx;

	if( ths->error ) return 0;
	if( ths->place + 2 > fbx->len )
	{
		ths->error = -102;
		if( fbx->errorcb ) fbx->errorcb( fbx, ths, "FBX EOF Reached when trying to Pop2." ); 
		return 0;
	}
	uint8_t * rd = fbx->data + ths->place;
	ths->place += 2;
	return rd[0] | (rd[1]<<8);
}

static uint32_t Pop4( fbxcopen_context * ths )
{
	fbxcopen * fbx = ths->fbx;

	if( ths->error ) return 0;
	if( ths->place + 4 > fbx->len )
	{
		ths->error = -104;
		if( fbx->errorcb ) fbx->errorcb( fbx, ths, "FBX EOF Reached when trying to Pop4." ); 
		return 0;
	}
	uint8_t * rd = fbx->data + ths->place;
	ths->place += 4;
	return rd[0] | (rd[1]<<8) | (rd[2]<<16) | (rd[3]<<24);
}

static uint64_t Pop8( fbxcopen_context * ths )
{
	fbxcopen * fbx = ths->fbx;

	if( ths->error ) return 0;
	if( ths->place + 8 > fbx->len )
	{
		ths->error = -108;
		if( fbx->errorcb ) fbx->errorcb( fbx, ths, "FBX EOF Reached when trying to Pop8." ); 
		return 0;
	}
	uint64_t * rd = fbx->data + ths->place;
	ths->place += 8;
	return *rd;
}


static double PopDouble( fbxcopen_context * ths )
{
	fbxcopen * fbx = ths->fbx;

	if( ths->error ) return 0;
	if( ths->place + 8 > fbx->len )
	{
		ths->error = -188;
		if( fbx->errorcb ) fbx->errorcb( fbx, ths, "FBX EOF Reached when trying to PopDouble." ); 
		return 0;
	}
	double * rd = fbx->data + ths->place;
	ths->place += 8;
	return *rd;
}

static double PopFloat( fbxcopen_context * ths )
{
	fbxcopen * fbx = ths->fbx;

	if( ths->error ) return 0;
	if( ths->place + 4 > fbx->len )
	{
		ths->error = -184;
		if( fbx->errorcb ) fbx->errorcb( fbx, ths, "FBX EOF Reached when trying to PopFloat." ); 
		return 0;
	}
	float * rd = fbx->data + ths->place;
	ths->place += 4;
	return *rd;
}


int FBXCOpenContextGoToNestedList( fbxcopen_context * ctx )
{
	uint32_t endOffset = Pop4( ctx );
	int numProps = Pop4( ctx );
	int propertyListLen = Pop4( ctx );
	int nameLen = Pop1( ctx );
	ctx->place += nameLen;
	ctx->place += propertyListLen;

	if( ctx->place >= endOffset )
		return 1;
	else
		return 0;
}


//////////////////////////////////////////////////////////////////////////////
// PROPERTIES
//////////////////////////////////////////////////////////////////////////////


void FBXCOpenAdvanceObjectToProperties( fbxcopen_context * ctx )
{
	uint32_t endOffset = Pop4( ctx );
	int numProps = Pop4( ctx );
	int propertyListLen = Pop4( ctx );
	int nameLen = Pop1( ctx );

	ctx->place += nameLen;
	ctx->listObjectStart = ctx->place + ctx->propertyListLen;
}

void FBXCOpenPropertyAdvance(fbxcopen_context * ctx)
{
	if( ctx->error ) return;
	if( ctx->place >= ctx->listobjectstart )
	{
		ctx->error = -324;
		if( fbx->errorcb ) fbx->errorcb( fbx, ctx, "Error: Advancing property off end of property list." ); 
		return;
	}
	ctx->place += FBXCOpenPropLenSource(ctx);
}

uint32_t FBXCOpenPropertyPopInt( fbxcopen_context * ctx )
{
	int start = ctx->place;
	if( ctx->error ) return 0;
	int len = FBXCOpenPropLenSource( ctx );
	char ct = Pop1( ctx );
	int ret = 0;
	int captured;

	captured = 1;
	switch( ct )
	{
	case 'Y': ret = Pop2(); break;
	case 'C': ret = Pop1(); break;
	case 'F': ret = (int)PopFloat(); break;
	case 'I': ret = Pop4(); break;
	case 'D': case 'L': ret = (int)Pop8(); break;
	default: captured = 0; break;
	}

	if( !captured )
	{
		uint32_t len = Pop4( ctx );
		switch( ct )
		{
		case 'S':
		{
			char cts[32];
			int tocpy = (len>sizeof(cts)-2)?(sizeof(cts)-2):len;
			strcpy( cts, ctx->place + ctx->fbx->data, tocpy );
			cts[tocpy] = 0;
			ret = atoi( cts );
			captured = 1;
			break;
		}
		case 'R': //raw binary data is unconvertable. 
		default:
			capture = 0;
		}

		if( ret == -1 )
		{
			int typesize = 0;
			switch( ct )
			{
				case 'f': typesize = 4; break;
				case 'd': typesize = 8; break;
				case 'l': typesize = 8; break;
				case 'i': typesize = 4; break;
				case 'b': typesize = 1; break;
			}

			//ArrayLength = 'len' from above.
			int encoding = Pop4( ctx ); //Encoding
			int compressedLen = Pop4( ctx );
			if( encoding )
				ret = compressedLen; //'len' becomes # of elements.
			else if( typesize )
				ret = len * typesize;
		}
	}


	ctx->place += len;
	return ret;
}

uint64_t FBXCOpenPropertyPopLong( fbxcopen_context * ctx )
{
}


float FBXCOpenPropertyPopFloat( fbxcopen_context * ctx )
{
}

char * FBXCOpenPropertyPopSV( fbxcopen_context * ctx, char * strplace, int strlen )
{
	//Copies string into specified buffer, 
}


int FBXCOpenPropLenSource( fbxcopen_context * ctx )
{
	fbxcopen * fbx = ctx->fbx;
	int start = ctx->place;
	int ret = -1;

	if( ctx->error ) return -2;
	char ct = Pop1( ctx );

	switch( ct )
	{
	case 'Y': ret = 2; break;
	case 'C': ret = 1; break;
	case 'F': case 'I': ret = 4; break;
	case 'D': case 'L': ret = 8; break;
	}

	if( ret == -1 )
	{
		uint32_t len = Pop4( ctx );
		switch( ct )
		{
		case 'S':
		case 'R':
			ret = len;
		}

		if( ret == -1 )
		{
			int typesize = 0;
			switch( ct )
			{
				case 'f': typesize = 4; break;
				case 'd': typesize = 8; break;
				case 'l': typesize = 8; break;
				case 'i': typesize = 4; break;
				case 'b': typesize = 1; break;
			}

			//ArrayLength = 'len' from above.
			int encoding = Pop4( ctx ); //Encoding
			int compressedLen = Pop4( ctx );
			if( encoding )
				ret = compressedLen; //'len' becomes # of elements.
			else if( typesize )
				ret = len * typesize;
		}
	}

	if( ret == -1 )
	{
		char loadstatus[128];
		snprintf( loadstatus, sizeof(loadstatus), "Unknown property type '%c' (%d)", ct, ct );
		ctx->error = -4;
		if( fbx->errorcb ) fbx->errorcb( fbx, ctx, loadstatus ); 
	}

	ctx->place = start;
	return ret;
}

void FBXCOpenContextPrintProperties( fbxcopen_context * ctx, int numProps, int depth )
{
	int prop;
	fbxcopen * fbx = ctx->fbx;
	for( prop = 0; prop < numProps; prop++ )
	{
		int start = ctx->place;
		int len = FBXCOpenPropLenSource( ctx );
		int loc = ctx->place;

		char cc = Pop1( ctx );
		switch(cc)
		{
		case 'S':
		{
			int len = Pop4( ctx );
			char * cs = alloca( len + 1 );
			memcpy( cs, ctx->place + fbx->data, len );
			cs[len] = 0;
			printf( "%*d:S<%s\n", depth+6, len, cs );
			break;
		}
		case 'f': case 'd': case 'l': case 'i':
		{
			int ilen = Pop4( ctx ); //# of elements
			int enc = Pop4( ctx );
			long unsigned int ecl = Pop4( ctx );
			printf( "%*d:%c / %d %d %ld\n", depth+6, len, cc, ilen, enc, ecl );
		
			int typesize = 0;
			switch( cc )
			{
				case 'f': typesize = 4; break;
				case 'd': typesize = 8; break;
				case 'l': typesize = 8; break;
				case 'i': typesize = 4; break;
				case 'b': typesize = 1; break;
			}

			if( typesize )
			{
				char * dataout = fbx->data + ctx->place;

				if( enc )
				{
					dataout = malloc( typesize * ilen );
					unsigned long destlen = typesize * ilen;
					int r = localpuff( dataout,      /* pointer to destination pointer */
		     			&destlen,        /* amount of output space */
		     			fbx->data + ctx->place + 2,   /* pointer to source data pointer */
		     			&ecl );      /* amount of input available */
					if( r )
					{
						ctx->error = -95;
						ctx->fbx->errorcb( ctx->fbx, ctx, "Error in gzipped data input" );
						return;
					}
				}

				int i = 0;
				for( i = 0; i < ilen ; i++ )
				{
					switch( cc )
					{
					case 'f': printf( "%f ", ((float*)dataout)[i] ); break;
					case 'd': printf( "%f ", ((double*)dataout)[i] ); break;
					case 'l': printf( "%lu ", ((int64_t*)dataout)[i] ); break;
					case 'i': printf( "%d ", ((int*)dataout)[i] ); break;
					case 'b': printf( "%d ", ((char*)dataout)[i] ); break;
					}
				}
				printf( "\n" );

				if( enc ) free( dataout );
			}
			break;
		}
		case 'Y':
			printf( "%*d:%c [%d]\n", depth+6, len, cc, (fbx->data+ctx->place)[0] | ((fbx->data+ctx->place)[1]<<8) );
			break;
		case 'C':
			printf( "%*d:%c [%d]\n", depth+6, len, cc, (fbx->data+ctx->place)[0] );
			break;
		case 'I':
			printf( "%*d:%c [%d]\n", depth+6, len, cc, (fbx->data+ctx->place)[0] | ((fbx->data+ctx->place)[1]<<8)| ((fbx->data+ctx->place)[2]<<16)| ((fbx->data+ctx->place)[3]<<24) );
			break;
		case 'F':
			printf( "%*d:%c [%f]\n", depth+6, len, cc, *((float*)(fbx->data+ctx->place)) );
			break;
		case 'D':
			printf( "%*d:%c [%f]\n", depth+6, len, cc, *((double*)(fbx->data+ctx->place)) );
			break;
		case 'L':
			printf( "%*d:%c [%lu]\n", depth+6, len, cc, *((uint64_t*)(fbx->data+ctx->place)) );
			break;
		default:
			printf( "%*d:%c\n", depth+6, len, cc );
		}
		ctx->place = loc + len;
	}	
}

void FBXCOpenContextPrintNodeInfo( fbxcopen_context * ctx, int depth )
{
	uint32_t backup;
	fbxcopen * fbx = ctx->fbx;
	char cm[257];

	do
	{
		backup = ctx->place;
		uint32_t endOffset = Pop4( ctx );
		int numProps = Pop4( ctx );
		int propertyListLen = Pop4( ctx );
		int nameLen = Pop1( ctx );

		if( nameLen == 0 ) break;

		memcpy( cm, ctx->fbx->data + ctx->place, nameLen );
		cm[nameLen] = 0;

		ctx->place += nameLen;

		printf( "%*s%5d Name: %s: props %d / propertyListLen: %d {nt:%d}\n",
			depth, "", backup,
			cm, numProps, propertyListLen, ctx->place < endOffset );

		int preprintplace = ctx->place;
		FBXCOpenContextPrintProperties( ctx, numProps, depth+1 );

		if( ctx->error ) return;
		ctx->place = preprintplace + propertyListLen;

		if( ctx->place < endOffset )
		{
			//We go deeper.
			FBXCOpenContextPrintNodeInfo( ctx, depth+2 );
		}


	} while( ctx->place < fbx->len && ctx->place != 0 && ctx->error == 0 );

	//ctx->place = backup;
}





typedef struct enumSet_t
{
	int num;
	int totallen;
	int * offsets;
	char ** ptrSet;
} enumSet;

static int LocalESEnumCallback( fbxcopen_context * ctx, void * opaqueE, char * section )
{
	enumSet * es = (enumSet*)opaqueE;
	es->num++;
	es->ptrSet = realloc( es->ptrSet, es->num * sizeof(char*) );
	es->offsets = realloc( es->offsets, es->num * sizeof(int*) );
	int sl = strlen( section );
	es->totallen += sl+1;
	char * s = es->ptrSet[es->num-1] = malloc( sl + 1 );
	memcpy( s, section, sl );
	s[sl] = 0;

	es->offsets[es->num-1] = ctx->place;
	return 0;
}

int FBXCOpenEnumerate( fbxcopen_context * ctx, char *** enumeratePtrs, int ** enumerateOffsets )
{
	if( !enumeratePtrs ) return -1;

	enumSet es;
	memset( &es, 0, sizeof(es) );
	FBXCOpenEnumerateWithCb( ctx, LocalESEnumCallback, &es );
	int i;
	char   ** ptrpr = *enumeratePtrs = malloc( sizeof(char*) * es.num + es.totallen + sizeof(int*) * es.num );
	int   * offsets = (int*) (((char*)ptrpr) + sizeof(char*) * es.num + es.totallen );
	char * ptrplace = ((char*)(ptrpr)) + sizeof(char*) * es.num;
	for( i = 0; i < es.num; i++ )
	{
		ptrpr[i] = ptrplace;
		char * srcname = es.ptrSet[i];
		int sl = strlen( srcname );
		memcpy( ptrplace, srcname, sl );
		ptrplace[sl] = 0; 
		ptrplace += sl+1;
		offsets[i] = es.offsets[i];
		free( srcname );
	}
	free( es.ptrSet );
	free( es.offsets );
	*enumerateOffsets = offsets;
	return es.num;
}

int FBXCOpenEnumerateWithCb( fbxcopen_context * ctx, int (*callback)( fbxcopen_context * ctx, void * opaque, char * section ), void * opaque )
{
	fbxcopen * fbx = ctx->fbx;

	char secname[257];
	int i = 0;

	//Iterate over al sectons.
	do {
		int backupIndex = ctx->place;
		int endLoc = Pop4( ctx );
		DumpBytes( ctx, 8 ); 
		int nameLen = Pop1( ctx );

		if( nameLen == 0 ) break;

		//Create C string from not-c-string.
		char * namestart = fbx->data + ctx->place;
		memcpy( secname, namestart, nameLen );
		secname[nameLen] = 0;

		ctx->place = backupIndex;

		//Callback nonzero - stop iterating.
		if( ctx->error ) break;

		if( callback( ctx, opaque, secname ) )
			break;

		//Skip pointer forward
		ctx->place = endLoc;
	} while( ctx->place < fbx->len && ctx->place != 0 && ctx->error == 0 );
}

fbxcopen * FBXCOpenData( uint8_t * data, const uint8_t freeWhenDone, const int len, FBXCErrorCB errorcb, void * opaqueO )
{
	fbxcopen * ret = malloc( sizeof( fbxcopen ) );
	memset( ret, 0, sizeof( fbxcopen ) );

	if( !data || len < 20 )
	{
		char errorstat[512];
		ret->opaqueO = opaqueO;
		snprintf( errorstat, sizeof(errorstat)-1, "FBX File too small or invalid (non-null:%d):(%d).", !!data, len );
		if( errorcb )
		{
			errorcb( ret, 0, errorstat );
		}
		else
		{
			printf( "%s\n", errorstat );
		}
		free( ret );

		if( data && freeWhenDone ) free( data );
		return 0;
	}

	if( memcmp( data, "Kaydara FBX Binary  "/*/x00*/, 20 ) != 0 )
	{
		char errorstat[512];
		ret->opaqueO = opaqueO;
		snprintf( errorstat, sizeof(errorstat)-1, "FBX File magic incorrect.  Expecting Kaydra FBX Binary" );
		if( errorcb )
		{
			errorcb( ret, 0, errorstat );
		}
		else
		{
			printf( "%s\n", errorstat );
		}
		free( ret );

		if( data && freeWhenDone ) free( data );
		return 0;
	}

	ret->data = data;
	ret->len = len;
	ret->opaqueO = opaqueO;
	ret->errorcb = errorcb;
	ret->fileVersion = ret->data[22] | (ret->data[23]<<8);
	return ret;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Actual loading code.


#define FBXCO_MODELVERT   1
#define FBXCO_MODELINDEX  2
#define FBXCO_MODELEDGES  4
#define FBXCO_MODELNORM   8
#define FBXCO_MODELUV     16
#define FBXCO_OBJECT      32
#define FBXCO_ANIMATION   64
#define FBXCO_CONNECTION  128
#define FBXCO_ALL         0xff


//Return nonzero to
typedef int (*FBXCOpenLoadFn)( fbxcopen_context * ctx, int datatype, void * data, void * opaqueL );

static int FBXCObjectGeometryLoadsCallback( fbxcopen_context * ctx, void * opaque, char * section )
{
	FBXCOpenLoadFn fn = opaque;
	printf( "GEOLOADS: %s\n", section );
	return 0;
}

static int FBXCObjectLoadsCallback( fbxcopen_context * ctx, void * opaque, char * section )
{
	FBXCOpenLoadFn fn = opaque;
	if( strcmp( section, "Geometry" ) == 0 )
	{
		//Pull off string property.
		int backup = ctx->place;
		//FBXCOpenContextPrintProperties( ctx, numProps, 0 );
		ctx->place = backup;
		printf( "\n" );
		//FBXCOpenContextGoToNestedList( ctx );
		//FBXCOpenEnumerateWithCb( ctx, FBXCObjectGeometryLoadsCallback, fn );
	}
	else if( strcmp( section, "Model" ) == 0 )
	{
	}
	//Other: sections we don't care about: Material and NodeAttribute
	return 0;
}


void FBXCOpenLoadFromContext( fbxcopen * fbx, FBXCOpenLoadFn fn, void * opaqueL )
{
	fbxcopen_context ctxmem;
	fbxcopen_context * ctx = &ctxmem;
	FBXCOpenMakeContext( fbx, &ctxmem, opaqueL );

	int i = 0;

	//Iterate over al sectons.
	do {
		int backupIndex = ctx->place;
		int endLoc = Pop4( ctx );
		int numProps = Pop4( ctx );
		int propertyListLen = Pop4( ctx );
		int nameLen = Pop1( ctx );

		if( nameLen == 0 ) break;

		//Create C string from not-c-string.
		char * namestart = fbx->data + ctx->place;

		ctx->place = backupIndex;

		if( strncmp( namestart, "Objects", nameLen ) == 0 )
		{
			printf( "=============================\n" );
			FBXCOpenContextGoToNestedList( ctx );
			FBXCOpenEnumerateWithCb( ctx, FBXCObjectLoadsCallback, fn );
			printf( "=============================\n" );
		}
		else if( strncmp( namestart, "Connections", nameLen ) == 0 )
		{
			FBXCOpenContextGoToNestedList( ctx );
//			FBXCOpenContextPrintNodeInfo( ctx, 0 );
			FBXCOpenEnumerateWithCb( ctx, FBXCObjectLoadsCallback, fn );
		}

		//Skip pointer forward
		ctx->place = endLoc;
	} while( ctx->place < fbx->len && ctx->place != 0 && ctx->error == 0 );

}



