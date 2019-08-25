#include <cnovrutil.h>
#include <os_generic.h>
#include <stdlib.h>
#include <stdio.h>

int g;

void JobTest1( void * opaquev, int opaquei )
{
	g = opaquei;
}

#define FAIL { printf( "Fail at %d\n", __LINE__ ); exit( -5 ); } 

int main()
{
	int i;
	CNOVRJobInit();

	if( 1 ){
		//Test 1: Make sure it doesn't crash.
		printf( "TEST 1: " );
		CNOVRJobTack( cnovrQLow, JobTest1, 0, 1, 1 );
		CNOVRJobTack( cnovrQLow, JobTest1, 0, 2, 0 );
		for( i = 3; i <1000; i++ )	CNOVRJobTack( cnovrQLow, JobTest1, 0, i, 0 );
		CNOVRJobCancel( cnovrQLow, JobTest1, 0, 100, 1 );
		CNOVRJobCancel( cnovrQLow, JobTest1, 0, 200, 1 );
		CNOVRJobCancel( cnovrQLow, JobTest1, 0, 300, 1 );
		CNOVRJobCancel( cnovrQLow, JobTest1, 0, 400, 1 );
		CNOVRJobCancel( cnovrQLow, JobTest1, 0, 500, 1 );
		CNOVRJobCancel( cnovrQLow, JobTest1, 0, 600, 1 );
		OGUSleep(20000); //flush
		if( g != 999 ) FAIL;
		printf( "PASS\n" );
	}
	
	{
		printf( "TEST 2:" );
		//Test 2: Actually check validity.
		CNOVRJobTack( cnovrQPrerender, JobTest1, 0, 1, 1 );
		CNOVRJobTack( cnovrQPrerender, JobTest1, 0, 2, 0 );
		CNOVRJobTack( cnovrQPrerender, JobTest1, 0, 3, 0 );
		CNOVRJobCancel( cnovrQPrerender, JobTest1, 0, 2, 1 );
		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 1 ) FAIL;
		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 3 ) FAIL;

		CNOVRJobTack( cnovrQPrerender, JobTest1, 0, 1, 1 );
		CNOVRJobTack( cnovrQPrerender, JobTest1, 0, 2, 0 );
		CNOVRJobTack( cnovrQPrerender, JobTest1, 0, 3, 0 );
		CNOVRJobCancel( cnovrQPrerender, JobTest1, 0, 1, 1 );
		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 2 ) FAIL;
		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 3 ) FAIL;

		CNOVRJobTack( cnovrQPrerender, JobTest1, 0, 1, 1 );
		CNOVRJobTack( cnovrQPrerender, JobTest1, 0, 2, 0 );
		CNOVRJobTack( cnovrQPrerender, JobTest1, 0, 3, 0 );
		CNOVRJobCancel( cnovrQPrerender, JobTest1, 0, 3, 1 );
		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 1) FAIL;
		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 2 ) FAIL;

		CNOVRJobTack( cnovrQPrerender, JobTest1, 0, 1, 1 );
		CNOVRJobTack( cnovrQPrerender, JobTest1, 0, 2, 0 );
		CNOVRJobTack( cnovrQPrerender, JobTest1, 0, 3, 0 );
		CNOVRJobCancel( cnovrQPrerender, JobTest1, 0, 4, 1 );
		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 1 ) FAIL;
		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 2 ) FAIL;
		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 3 ) FAIL;
		printf( " PASS\n" );
	}
	return 0;
}

