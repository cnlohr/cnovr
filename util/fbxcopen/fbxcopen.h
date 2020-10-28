#ifndef _FBXCOPEN_H
#define _FBXCOPEN_H

#include <stdint.h>

//OPTIONS:
// -DFBXCUSEPUFF --> Try to use puff.h.


struct fbxcopen_t;
struct fbxcopen_context_t;

typedef void (*FBXCErrorCB)( struct fbxcopen_t * fbxc, struct fbxcopen_context_t * ctx, const char * error );

typedef struct fbxcopen_t
{
	uint16_t fileVersion;

	//Used while opening
	uint8_t * data;
	void * opaque;
	FBXCErrorCB errorcb;
	int len;
	uint8_t freeWhenDone;
} fbxcopen;

typedef struct fbxcopen_context_t
{
	fbxcopen * fbx;
	int place;
	int error; //Nonzero if issue.
	void * opaque;
} fbxcopen_context;

fbxcopen * FBXCOpenFile( const char * fileName, FBXCErrorCB errorcb, void * opaque );

//Note: This does not copy data.  You must have alloc'd it on the heap.  If 'freeWhenDone' This function will free it when done.
fbxcopen * FBXCOpenData( uint8_t * data, const uint8_t freeWhenDone, const int len, FBXCErrorCB errorcb, void * opaque );


void FBXCOpenCopyContext( fbxcopen_context * to, fbxcopen_context * from, void * opaque );
void FBXCOpenMakeContext( fbxcopen * fbxc, fbxcopen_context * ctx, void * opaque );
void FBXCOpenResetContext( fbxcopen_context * ctx, int offset /*If offset = minimumlocation, will set to beginning of file */ );
void FBXCOpenDestroy( fbxcopen * fbxc );




//Utility functions:

//Returns negative on error.
int FBXCOpenPropLenSource( fbxcopen_context * ctx );


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
int FBXCOpenEnumerateWithCb( fbxcopen_context * ctx, int (*callback)( fbxcopen_context * ctx, void * opaque, char * section ), void * opaque ); 



#endif

