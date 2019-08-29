#ifndef _CNOVRUTIL_H
#define _CNOVRUTIL_H

#include <stdint.h>
#include <stdbool.h>

#define CNOVR_MAX_PATH 255

typedef void(cnovr_cb_fn)( void * tag, void * opaquev );


//Not sure if we need this feature.
//void * GetNamedPtr( const char * namedptr, const char * type );
//void * NamedPtr( const char * namedptr, const char * type, int size );

char * FileToString( const char * fname, int * length );
char ** SplitStrings( const char * line, char * split, char * white, bool merge_fields ); //You can just free(...) the return. it's safe.
int StringCompareEndingCase( const char * thing_to_search, const char * check_extension );



//////////////////////////////////////////////////////////////////////////////


//These must be threadsafe.  Also, need a way to wholesale clear out a class of these guys.
void FileTimeAddWatch( const char * fname, cnovr_cb_fn fn, void * tag, void * opaquev );  //Warning: This is also slow... We need to figure out a better way.
void FileTimeRemoveWatch( const char * fname, cnovr_cb_fn fn, void * tag, void * opaquev ); //Warning: this is slow. Avoid its use.
void FileTimeRemoveTagged( void * tag, int wait_on_pending );

double FileTimeCached( const char * fname );

//Internal
void CNOVRInternalStartCacheSystem();
void CNOVRInternalStopCacheSystem();

//////////////////////////////////////////////////////////////////////////////
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
void CNOVRJobInit(); //Internal
void CNOVRJobStop(); //Internal
int CNOVRJobProcessQueueElement( cnovrQueueType q ); //returns 1 if queue still processing.

#endif


