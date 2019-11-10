#include <cnovrutil.h>
#include <os_generic.h>
#include <stdlib.h>
#include <stdio.h>

int g;

void CNOVRJobInit();
void CNOVRJobStop();

void JobTest1( void * opaquev, void * opaquei )
{
	g = opaquei - (void*)0;
}

#define FAIL { printf( "Fail at %d\n", __LINE__ ); exit( -5 ); } 

int main()
{
	int i;
	CNOVRJobInit();

	if( 1 )
	{
		cnovr_model * m = CNOVRModelCreate( 0, GL_TRIANGLES );
		CNOVRModelSetNumVBOsWithStrides( m, 3, 3, 3, 3 );
		cnovr_point3d v = { 0, 0, 0 };
		CNOVRVBOTackv( m->pGeos[0], 3, v );
		CNOVRVBOTackv( m->pGeos[1], 3, v );
		CNOVRVBOTackv( m->pGeos[2], 3, v );
		v[1] = 1;
		CNOVRVBOTackv( m->pGeos[0], 3, v );
		CNOVRVBOTackv( m->pGeos[1], 3, v );
		CNOVRVBOTackv( m->pGeos[2], 3, v );
		v[1] = 0;
		v[0] = 1;
		CNOVRVBOTackv( m->pGeos[0], 3, v );
		CNOVRVBOTackv( m->pGeos[1], 3, v );
		CNOVRVBOTackv( m->pGeos[2], 3, v );
		CNOVRModelTackIndex( m, 3, 0, 1, 2 );

		cnovr_point3d start = { 0, 0, 2 };
		cnovr_point3d dir = { .21, -0.01, -1 };
		cnovr_point3d dir2 = { .21, -0.02, -1 };
		cnovr_collide_results res;
		res.t = 1e20;
		int r = CNOVRModelCollide( m, start, dir, &res, .1 );
		printf( "%d  %f\n", r, res.t );
		res.t = 1e20;
		r = CNOVRModelCollide( m, start, dir2, &res, .1 );
		printf( "%d  %f\n", r, res.t );
		exit( 0 );
	}

	if( 1 ){
		//Test 1: Make sure it doesn't crash.
		printf( "TEST 1: " );
		CNOVRJobTack( cnovrQLow, JobTest1, 0, (void*)1, 1 );
		CNOVRJobTack( cnovrQLow, JobTest1, 0, (void*)2, 0 );
		for( i = 3; i <1000; i++ )	CNOVRJobTack( cnovrQLow, JobTest1, 0, (char*)0+i, 0 );
		CNOVRJobCancel( cnovrQLow, JobTest1, 0, (void*)100, 1 );
		CNOVRJobCancel( cnovrQLow, JobTest1, 0, (void*)200, 1 );
		CNOVRJobCancel( cnovrQLow, JobTest1, 0, (void*)300, 1 );
		CNOVRJobCancel( cnovrQLow, JobTest1, 0, (void*)400, 1 );
		CNOVRJobCancel( cnovrQLow, JobTest1, 0, (void*)500, 1 );
		CNOVRJobCancel( cnovrQLow, JobTest1, 0, (void*)600, 1 );
		OGUSleep(200000); //flush
		if( g != 999 ) { printf( "G: %d\n", g ); FAIL; }
		printf( "PASS\n" );
	}
	
	{
		printf( "TEST 2:" );
		//Test 2: Actually check validity.
		CNOVRJobTack( cnovrQPrerender, JobTest1, 0, (void*)1, 1 );
		CNOVRJobTack( cnovrQPrerender, JobTest1, 0, (void*)2, 0 );
		CNOVRJobTack( cnovrQPrerender, JobTest1, 0, (void*)3, 0 );
		CNOVRJobCancel( cnovrQPrerender, JobTest1, 0, (void*)2, 1 );
		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 1 ) FAIL;
		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 3 ) FAIL;

		CNOVRJobTack( cnovrQPrerender, JobTest1, 0, (void*)1, 1 );
		CNOVRJobTack( cnovrQPrerender, JobTest1, 0, (void*)2, 0 );
		CNOVRJobTack( cnovrQPrerender, JobTest1, 0, (void*)3, 0 );
		CNOVRJobCancel( cnovrQPrerender, JobTest1, 0, (char*)0 + 1, 1 );
		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 2 ) FAIL;
		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 3 ) FAIL;

		CNOVRJobTack( cnovrQPrerender, JobTest1, 0, (void*)1, 1 );
		CNOVRJobTack( cnovrQPrerender, JobTest1, 0, (void*)2, 0 );
		CNOVRJobTack( cnovrQPrerender, JobTest1, 0, (void*)3, 0 );
		CNOVRJobCancel( cnovrQPrerender, JobTest1, 0, (void*)3, 1 );
		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 1 ) FAIL;
		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 2 ) FAIL;

		CNOVRJobTack( cnovrQPrerender, JobTest1, (void*)0, (void*)1, 1 );
		CNOVRJobTack( cnovrQPrerender, JobTest1, (void*)0, (void*)2, 0 );
		CNOVRJobTack( cnovrQPrerender, JobTest1, (void*)0, (void*)3, 0 );
		CNOVRJobCancel( cnovrQPrerender, JobTest1, (void*)0, (void*)4, 1 );
		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 1 ) FAIL;
		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 2 ) FAIL;
		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 3 ) FAIL;

		CNOVRJobTack( cnovrQPrerender, JobTest1, (void*)0, (void*)1, 1 );
		CNOVRJobTack( cnovrQPrerender, JobTest1, (void*)1, (void*)2, 0 );
		CNOVRJobTack( cnovrQPrerender, JobTest1, (void*)0, (void*)3, 0 );
		CNOVRJobCancelAllTag( (void*)1, 1 );
		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 1 ) FAIL;
		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 3 ) FAIL;
		CNOVRJobTack( cnovrQPrerender, JobTest1, (void*)0, (void*)1, 1 );
		CNOVRJobTack( cnovrQPrerender, JobTest1, (void*)1, (void*)2, 0 );
		CNOVRJobTack( cnovrQPrerender, JobTest1, (void*)0, (void*)3, 0 );
		CNOVRJobCancelAllTag( (void*)0, 1 );
		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 2 ) FAIL;

		CNOVRJobProcessQueueElement( cnovrQPrerender ); if( g != 2 ) FAIL;

		printf( " PASS\n" );
	}
	CNOVRJobStop();
	printf( "DONE\n" );
	return 0;
}

