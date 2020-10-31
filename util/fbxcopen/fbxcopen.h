/****************************************************************************
 * FBXCOpen
 *
 * Copyright 2020 <>< Charles Lohr, under the MIT/x11 or NewBSD license.
 *
 * FBXCOpen is a library for opening FBX 3D Model files.
 *
 * It is split into two parts, one is a in-memory copy of the FBX file from
 * the hard drive.  The other is the context, which needs a copy per thread.
 * But multiple threads can read the same file at the same time, as long as
 * each thread has its own context.
 *
 * You load your model into memory, and create a context, then you can
 * perform different operations on it, where you can seek around the tree.
 * in general the tree is seeked recursively, depending on what you are
 * trying to do.
 *
 * An example which manually prints out the contents of a FBX file is:

	//Load file
	fbxcopen * fbx = FBXCOpenFile(
		"testscene.fbx",
		fbxcerror,
		(void*)0x77 ); //Sentinal to make sure opaque is preserved
	if( !fbx ) return 0;

	//A normal enumeration, for dumping a section or the entire file.
	fbxcopen_context myctx;
	FBXCOpenMakeContext( fbx, &myctx, 
		(void*)0x88 ); //Sentinal to make sure opaque is preserved

	FBXCOpenContextPrintNodeInfo( &myctx, 0 );

 *
 * You can manually enumerate through the file, if you'd like as well, but
 * for most uses, you will be using FBXCOpenLoadFromContext.  This will
 * handle enumerating through the file to load an actual model.
 *
 *
 * Extra compiler options:
 *  -DFBXCUSEPUFF - because FBX files require zlib, you can optionally pass
 *     this parameter if your project uses puff (the single-file inflate)
 *     instead of the whole zlib project.  It will expect puff.h to be
 *     accessible. 
 *
 *
 *
 ****************************************************************************/

#ifndef _FBXCOPEN_H
#define _FBXCOPEN_H

#include <stdint.h>

struct fbxcopen_t;
struct fbxcopen_context_t;

typedef void (*FBXCErrorCB)( struct fbxcopen_t * fbxc, struct fbxcopen_context_t * ctx, const char * error );

typedef struct fbxcopen_t
{
	uint16_t fileVersion;

	//Used while opening
	uint8_t * data;
	void * opaqueO;
	FBXCErrorCB errorcb;
	int len;
	uint8_t freeWhenDone;
} fbxcopen;

typedef struct fbxcopen_context_t
{
	fbxcopen * fbx;
	int place;
	int listObjectStart; //To make sure we don't overflow the property list.
	int error; //Nonzero if issue.
	void * opaqueC;
} fbxcopen_context;

fbxcopen * FBXCOpenFile( const char * fileName, FBXCErrorCB errorcb, void * opaqueO );

//Note: This does not copy data.  You must have alloc'd it on the heap.  If 'freeWhenDone' This function will free it when done.
fbxcopen * FBXCOpenData( uint8_t * data, const uint8_t freeWhenDone, const int len, FBXCErrorCB errorcb, void * opaqueO );


void FBXCOpenCopyContext( fbxcopen_context * to, fbxcopen_context * from, void * opaqueC );
void FBXCOpenMakeContext( fbxcopen * fbxc, fbxcopen_context * ctx, void * opaqueC );
void FBXCOpenResetContext( fbxcopen_context * ctx, int offset /*If offset = minimumlocation, will set to beginning of file */ );
void FBXCOpenDestroy( fbxcopen * fbxc );

//Properties functions.

void FBXCOpenAdvanceObjectToProperties( fbxcopen_context * ctx );
void FBXCOpenPropertyAdvance( fbxcopen_context * ctx ); //Advance to next property indiscriminatorily.

//Note these functions are intended for maximum robustness.  If the type is convertable at all, it will convert.

uint32_t FBXCOpenPropertyPopInt( fbxcopen_context * ctx );
uint64_t FBXCOpenPropertyPopLong( fbxcopen_context * ctx );
float FBXCOpenPropertyPopFloat( fbxcopen_context * ctx );
char * FBXCOpenPropertyPopSV( fbxcopen_context * ctx, char * strplace, int strlen, int * outlen ); //Copies string into specified buffer, returns length of data read inoutlen.  This will null-terminate, but, binary blob data is OK.  If buffer exceeds size, returns 0 or NULL.  If strplace == NULL, data will be allocated on heap.

int * 


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



#define FBXCO_MODELVERT   1
#define FBXCO_MODELINDEX  2
#define FBXCO_MODELEDGES  4
#define FBXCO_MODELNORM   8
#define FBXCO_MODELUV     16
#define FBXCO_OBJECT      32
#define FBXCO_ANIMATION   64
#define FBXCO_CONNECTION  128
#define FBXCO_ALL         0xff

//XXX TODO: Add additional load types here, like for cameras, etc.

typedef struct modelload_data_t
{
	char * objectpath;
	uint32_t id;
	char dataformat; //'f', 'd', 'l', 'i', 'b', etc...
	void * data;
} modelload_data;

typedef struct modelload_uv_t
{
	char * objectpath;
	uint32_t id;
	char dataformatCoord; //'f', 'd', 'l', 'i', 'b', etc...
	char dataformatIndex; //'f', 'd', 'l', 'i', 'b', etc...
	void * dataCoord;
	void * dataIndex;
} modelload_uv;

typedef struct modelload_object_t
{
	char * objectpath;
	uint32_t id;
	double translate[3];
	double rotate[3]; //Euler angles.
	double scale[3];
} modelload_object;

typedef struct modelload_connection_t
{
	uint32_t idA;
	uint32_t idB;
} modelload_connection;

//Return nonzero to
typedef int (*FBXCOpenLoadFn)( fbxcopen_context * ctx, int cbtype, void * data, void * opaqueL );

void FBXCOpenLoadFromContext( fbxcopen * fbx, FBXCOpenLoadFn fn, void * opaqueL );





//Utility functions:

//Returns negative on error. Otherwise, return the source length of the prop ctx is pointing to. 
int FBXCOpenPropLenSource( fbxcopen_context * ctx );

//Enter a given object, i.e. move the context to the first sub-object.
void FBXCOpenCtxEnterObject( fbxcopen_context * ctx );


//Exploration functions
//Given we are at the beginning of a Node Record, we want to move into its nested list.
//Returns zero if successful.
int FBXCOpenContextGoToNestedList( fbxcopen_context * ctx );
void FBXCOpenContextPrintProperties( fbxcopen_context * ctx, int numProps, int depth );

//You can actually call this on the root node and it will print an expository view of the file.
void FBXCOpenContextPrintNodeInfo( fbxcopen_context * ctx, int depth );


//Tricky, you can just free() your ptr that is written by enumeratePtrs.
//It's actually allocated as a single chunk of data. It also includes offsets.
//You will need to free(...) both enumeratePtrs.
//
//NOTE: This is a particularly inefficient mode of exploration, please use EnumerateWithCb.
//NOTE: enumerateOffsets is the beginning of the node itself.
int FBXCOpenEnumerate( fbxcopen_context * ctx, char *** enumeratePtrs, int ** enumerateOffsets );


//Callback returning nonzero-value will stop progression.
int FBXCOpenEnumerateWithCb( fbxcopen_context * ctx, int (*callback)( fbxcopen_context * ctx, void * opaqueE, char * section ), void * opaque ); 



#endif

