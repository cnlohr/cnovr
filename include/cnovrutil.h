#ifndef _CNOVRUTIL_H
#define _CNOVRUTIL_H

#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <cnovrparts.h>

#define CNOVR_MAX_PATH 255

typedef void(cnovr_cb_fn)( void * tag, void * opaquev );

//////////////////////////////////////////////////////////////////////////////

//Threadsafe asprintf, DO NOT DELETE RETURN POINTER!  It will automatically be deleted on thread closure.
//Both of these functions share the same heap data.
int tasprintf( char ** out, const char * format, ... );
int tvasprintf( char ** out, const char * format, va_list ap );
char * trprintf( const char * format, ... ); //Thread-save return value.
char * jsmnstrsn( char * outbuffer, int outlen, const char * data, int start, int end ); //Extracts a substring of data into outbuffer and returns outbuffer.
int    jsmnintparse( const char * data, int start, int end );

//Not sure if we need this feature.
void * CNOVRNamedPtrGet( const char * namedptr, const char * type );
void CNOVRNamedPtrSave( const char * namedptr );
void CNOVRNamedPtrRevert( const char * namedptr );
void * CNOVRNamedPtrData( const char * namedptr, const char * type, int size );
#define NAMEDPTRTYPED( name, type ) ((type*)CNOVRNamedPtrData( name, #type, sizeof( type )))

//////////////////////////////////////////////////////////////////////////////

char ** CNOVRFolderListing( const char * path, int * elements ); //You can free this up with a single free(...)
char * CNOVRFileToString( const char * fname, int * length );
char ** CNOVRSplitStrings( const char * line, char * split, char * white, int merge_fields, int * elementcount ); //You can just free(...) the return. it's safe.
int CNOVRStringCompareEndingCase( const char * thing_to_search, const char * check_extension );
char * CNOVRGetBaseFileName( const char * path ); //returned in trprintf, so you can't re-use the buffer.

//////////////////////////////////////////////////////////////////////////////

char * CNOVRFileSearch( const char * fname ); //Returns a thread-local reference.
char * CNOVRFileSearchAbsolute( const char * fname ); //Returns a thread-local reference.
void CNOVRFileSearchAddPath( const char * path ); //This function dups your string.
void CNOVRFileSearchRemovePath( const char * path );

//////////////////////////////////////////////////////////////////////////////


//These must be threadsafe.  Also, need a way to wholesale clear out a class of these guys.
void CNOVRFileTimeAddWatch( const char * fname, cnovr_cb_fn fn, void * tag, void * opaquev );  //XXX: This is also slow... We need to figure out a better way.
void CNOVRFileTimeRemoveWatch( const char * fname, cnovr_cb_fn fn, void * tag, void * opaquev ); //XXX: this is slow. Avoid its use.
void CNOVRFileTimeRemoveTagged( void * tag, int wait_on_pending );

double CNOVRFileTimeCached( const char * fname );

//////////////////////////////////////////////////////////////////////////////
//For one-shot tasks.

typedef enum
{
	cnovrQLow,			//Things like making sure we're up to date - not sure?
	cnovrQAsync,		//??? No access to GL thread.
	//cnovrQUpdate,		//???
	cnovrQPrerender,	//Happens in the render thread
	cnovrQMAX,
} cnovrQueueType;

//Async, but, has delays between each completion. Multiple identical items can queue.  Cancellation must match all parameters.
void CNOVRJobTack( cnovrQueueType q, cnovr_cb_fn fn, void * tag, void * opaquev, bool insert_even_if_pending );
void CNOVRJobCancel( cnovrQueueType q, cnovr_cb_fn fn, void * tag, void * opaquev, bool wait_on_pending );
void CNOVRJobCancelAllTag( void * tag, int wait_on_pending );

//Usually internal
void DEBUGDumpQueue( cnovrQueueType qt );
int CNOVRJobProcessQueueElement( cnovrQueueType q ); //returns 1 if queue still processing.

//////////////////////////////////////////////////////////////////////////////
//XXX TODO: Add some sort of "in the future" queue.

//////////////////////////////////////////////////////////////////////////////

//For real-time immediate responses, continuously calling, will not remove upon job completion.
typedef enum
{
	//XXX Also need an asychronous "Update" function.
	cnovrLUpdate,
	cnovrLPrerender,
	cnovrLCollide,
	cnovrLRender0,
	cnovrLRender1,
	cnovrLRender2,
	cnovrLRender3,
	cnovrLRender4,
	cnovrLPostRender,
	cnovrLPreviewRender,
	cnovrLMAX,
} cnovrRunList;

//Returns # of called events.
int CNOVRListCall( cnovrRunList l, void * data, int delete_on_call ); 
void CNOVRListAdd( cnovrRunList l, void * base_object, cnovr_cb_fn * fn );
void CNOVRListDeleteTag( void * base_object );
void CNOVRListDeleteTCCTag( void * tcctag );

//////////////////////////////////////////////////////////////////////////////

#define FREE_LATER_LAG 3

//Not tied to a thread.
void CNOVRFreeLater( void * tofree );

//Not intended for script use. Use more for internal use.
void * CNOVRThreadMalloc( int size );
void * CNOVRThreadRealloc( void * initial, int size );
void CNOVRThreadFree( void * tofree );


#endif


