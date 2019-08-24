#include <cnovrutil.h>
#include <os_generic.h>
#include <stdlib.h>
#include <stdio.h>

int g;

void JobTest1( void * opaquev, int opaquei )
{
	printf( "OI: %d\n", opaquei );
	g = opaquei;
}

#define FAIL { printf( "Fail at %d\n", __LINE__ ); exit( -5 ); } 

int main()
{
	int i;
	CNOVRJobInit();

	CNOVRJobTack( cnovrQLow, JobTest1, 0, 1, 1 );
	CNOVRJobTack( cnovrQLow, JobTest1, 0, 2, 0 );
for( i = 0; i <1000; i++ )	CNOVRJobTack( cnovrQLow, JobTest1, 0, rand(), 0 );
	printf( "Canceling\n" );
//DEBUGDumpQueue( cnovrQLow );
	CNOVRJobCancel( cnovrQLow, JobTest1, 0, 2, 1 );
	printf( "...\n" );
	CNOVRJobProcessQueueElement( cnovrQLow ); if( g != 1 ) FAIL;
	CNOVRJobProcessQueueElement( cnovrQLow ); if( g != 1 ) FAIL;
	return 0;
}

#if 0

typedef void(cnovr_cb_fn)( void * opaquev, int opaquei );

typedef enum
{
	cnovrQLow,			//Things like making sure we're up to date - not sure?
	cnovrQAsync,		//??? No access to GL thread.
	//cnovrQUpdate,		//???
	cnovrQPrerender,	//Happens in the render thread
	cnovrQMAX,
} cnovrQueueType;

//Async, but, has delays between each completion. Multiple identical items can queue.  Cancellation must match all parameters.
void CNOVRJobTack( cnovrQueueType q, cnovr_cb_fn fn, void * opaquev, int opaquei, int insert_even_if_pending );
void CNOVRJobCancel( cnovrQueueType q, cnovr_cb_fn fn, void * opaquev, int opaquei, int wait_on_pending );

//Usually internal
void CNOVRJobInit(); //Internal
int CNOVRJobProcessQueueElement( cnovrQueueType q ); //returns 1 if queue still processing.

#endif
