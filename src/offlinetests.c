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
		cnovr_point3d v = { 1, 1, -1.49 };
		CNOVRVBOTackv( m->pGeos[0], 3, v );
		CNOVRVBOTackv( m->pGeos[1], 3, v );
		CNOVRVBOTackv( m->pGeos[2], 3, v );
		v[0] = 0;
		v[1] = 1;
		v[2] = .132;
		CNOVRVBOTackv( m->pGeos[0], 3, v );
		CNOVRVBOTackv( m->pGeos[1], 3, v );
		CNOVRVBOTackv( m->pGeos[2], 3, v );
		v[0] = 1.31;
		v[1] = 0;
		v[2] = -.17;
		CNOVRVBOTackv( m->pGeos[0], 3, v );
		CNOVRVBOTackv( m->pGeos[1], 3, v );
		CNOVRVBOTackv( m->pGeos[2], 3, v );
		v[0] = 1.31;
		v[1] = 1;
		v[2] = -.094;
		CNOVRVBOTackv( m->pGeos[0], 3, v );
		CNOVRVBOTackv( m->pGeos[1], 3, v );
		CNOVRVBOTackv( m->pGeos[2], 3, v );
		CNOVRModelTackIndex( m, 3, 1, 0, 2 );
		CNOVRModelTackIndex( m, 3, 1, 2, 3 );

		FILE * ftestnorm = fopen( "testnorm.ppm", "wb" );
		fprintf( ftestnorm, "P6\n%d %d\n%d\n", 1000, 1000, 255 );
		FILE * ftestdep = fopen( "testdep.ppm", "wb" );
		fprintf( ftestdep, "P6\n%d %d\n%d\n", 1000, 1000, 255 );

		int x, y;
		for( y = 0; y < 1000; y++ )
		for( x = 0; x < 1000; x++ )
		{
			cnovr_point3d start = { -0.001, -1.001, 2 };
			cnovr_point3d dir = { (x-500)/1000.+.3, (y-500)/1000.+.8, -1 };
			cnovr_collide_results res;
			res.t = 1e20;
			int r = CNOVRModelCollide( m, start, dir, &res, .1, 0 ); //Coarse acquisition
			float rgbof[3];
			scale3d( dir, dir, res.t + 0.01 );
			add3d( start, dir, start );
			res.t = 1e20;
			r = CNOVRModelCollide( m, start, dir, &res, .1, -.2 ); //Coarse acquisition
			scale3d( rgbof, res.geonorm, 0.5 );
			rgbof[0] += 0.5;
			rgbof[1] += 0.5;
			rgbof[2] += 0.5;
			if( res.t > 1000 ) { rgbof[0] = 0; rgbof[1] = 0; rgbof[2] = 0; }
			uint8_t rgbob[3] = { rgbof[0]*255, rgbof[1]*255, rgbof[2]*255 };
			fwrite( rgbob, 1, 3, ftestnorm );
			//printf( "%f %f\n", res.t, res.sndist );
			res.t += .2;
			res.sndist += .2;
			rgbof[1] = rgbof[0] = res.t;
			rgbof[2] = res.sndist;
			if( rgbof[0] < 0 || rgbof[0] > 1. ) rgbof[0] = 0;
			if( rgbof[1] < 0 || rgbof[1] > 1. ) rgbof[1] = 0;
			if( rgbof[2] < 0 || rgbof[2] > 1. ) rgbof[2] = 0;
			if( r < 0 ) rgbof[1] = 1;
			else { printf( "%f %f\n", res.t, res.sndist ); }
			rgbob[0] = rgbof[0] * 255;
			rgbob[1] = rgbof[1] * 255;
			rgbob[2] = rgbof[2] * 255;
			fwrite( rgbob, 1, 3, ftestdep );
			res.t = 1e20;
		}
		fclose( ftestnorm );
		fclose( ftestdep );
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

