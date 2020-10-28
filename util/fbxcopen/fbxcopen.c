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


fbxcopen * FBXCOpenFile( const char * fileName, FBXCErrorCB errorcb, void * opaque )
{
	fbxcopen * ret = 0;

	FILE * f = fopen( fileName, "rb" );
	if( !f )
	{
		char errorstat[512];
		ret = calloc( 1, sizeof( fbxcopen ) );
		ret->opaque = opaque;
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
		ret = FBXCOpenData( data, 1, len, errorcb, opaque );
	}
	else
	{
		char errorstat[512];
		ret = calloc( 1, sizeof( fbxcopen ) );
		ret->opaque = opaque;
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


void FBXCOpenCopyContext( fbxcopen_context * to, fbxcopen_context * from, void * opaque )
{
	memcpy( to, from, sizeof( fbxcopen_context ) );
	to->opaque = opaque;
}

void FBXCOpenMakeContext( fbxcopen * fbxc, fbxcopen_context * ctx, void * opaque )
{
	memset( ctx, 0, sizeof(*ctx) );
	ctx->fbx = fbxc;
	ctx->opaque = opaque;
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
		char loadstatus[128];
		snprintf( loadstatus, sizeof(loadstatus), "FBX EOF Reached when trying to Pop1." );
		ths->error = -1;
		if( fbx->errorcb ) fbx->errorcb( fbx, ths, loadstatus ); 
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
		char loadstatus[128];
		snprintf( loadstatus, sizeof(loadstatus), "FBX EOF Reached when trying to Pop2." );
		ths->error = -2;
		if( fbx->errorcb ) fbx->errorcb( fbx, ths, loadstatus ); 
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
		char loadstatus[128];
		snprintf( loadstatus, sizeof(loadstatus), "FBX EOF Reached when trying to Pop4." );
		ths->error = -4;
		if( fbx->errorcb ) fbx->errorcb( fbx, ths, loadstatus ); 
		return 0;
	}
	uint8_t * rd = fbx->data + ths->place;
	ths->place += 4;
	return rd[0] | (rd[1]<<8) | (rd[2]<<16) | (rd[3]<<24);
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


int FBXCOpenPropLenSource( fbxcopen_context * ctx )
{
	fbxcopen * fbx = ctx->fbx;
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

		ctx->place = start;
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
					printf( "PUFF: %d\n", r );
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

				if( enc ) free( dataout );
			}
			break;
		}
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

static int LocalESEnumCallback( fbxcopen_context * ctx, void * opaque, char * section )
{
	enumSet * es = (enumSet*)opaque;
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

fbxcopen * FBXCOpenData( uint8_t * data, const uint8_t freeWhenDone, const int len, FBXCErrorCB errorcb, void * opaque )
{
	fbxcopen * ret = malloc( sizeof( fbxcopen ) );
	memset( ret, 0, sizeof( fbxcopen ) );

	if( !data || len < 20 )
	{
		char errorstat[512];
		ret->opaque = opaque;
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
		ret->opaque = opaque;
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
	ret->opaque = opaque;
	ret->errorcb = errorcb;
	ret->fileVersion = ret->data[22] | (ret->data[23]<<8);
	return ret;
}

