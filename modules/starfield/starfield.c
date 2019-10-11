#include <stdio.h>
#include <stdlib.h>
#include <cnovrparts.h>
#include <cnovrutil.h>
#include <cnovrmath.h>
#include <string.h>
#include <chew.h>
#include "flat_stars.h"

cnovr_model * model;
cnovr_shader * shader;
cnovr_model * modelConst;
//cnovr_shader * shaderConst;

float * starfield_data1;	//ascention, declination, parallax [1]
float * starfield_data2;   //magnitude, bvcolor, vicolor [1]
flat_star * database;
int numstars = 0;
//typedef struct __attribute__((__packed__)) {
//uint32_t rascention_bams;
//int32_t declination_bams;
//int16_t parallax_10uas;
//uint16_t magnitude_mag1000;
//int16_t bvcolor_mag1000;
//int16_t vicolor_mag1000;
//} flat_star;


static void RenderFunction( void * tag, void * opaquev )
{
	if( !model || !shader ) return;
	CNOVRRender( shader );
	cnovr_pose pident;
	pose_make_identity( &pident );
	float scales = 100000;
	float fvu[4] = { 1., scales, 0., 0. };

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
	glDisable( GL_DEPTH_TEST);
	glPointSize( 4 );
	glUniform4fv( 19, 1, fvu );
	CNOVRModelRenderWithPose( model, &pident );
	glLineWidth(.5);
	fvu[0] = 0;
	glUniform4fv( 19, 1, fvu );
	CNOVRModelRenderWithPose( modelConst, &pident );

	glEnable( GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  
}

//typedef struct { char cname[3]; uint8_t lines; } constellation;
//constellation constellations[] = {


static void starfield_setup( void * tag, void * opaquev )
{
	shader = CNOVRShaderCreate( "starfield/starfield" );
	//shaderConst = CNOVRShaderCreate( "starfield/constellations" );
	CNOVRListAdd( cnovrLRender, 0, RenderFunction );

	int constellationlines = sizeof( constellationsegs ) / sizeof( constellationsegs[0] ) / 2;

	modelConst = CNOVRModelCreate( 0, GL_LINES );
	CNOVRModelSetNumVBOs( modelConst, 2 );
	modelConst->pGeos[0] = CNOVRCreateVBO( 4, 0, constellationlines*2, 0 );
	modelConst->pGeos[1] = CNOVRCreateVBO( 4, 0, constellationlines*2, 0 );
	int i;
	for( i = 0; i < constellationlines*2; i++ )
	{
		int starid = constellationsegs[i];
		memcpy( modelConst->pGeos[0]->pVertices + i*4, starfield_data1 + starid * 4, sizeof( float ) * 4 );
		memcpy( modelConst->pGeos[1]->pVertices + i*4, starfield_data2 + starid * 4, sizeof( float ) * 4 );
		//make the .w component allow the vertex shader to know if it's the beginning or end of the line.
		(modelConst->pGeos[0]->pVertices + i*4)[3] = i&1; 
		CNOVRModelTackIndex( modelConst, 1, i );		
		//printf( "%d %d\n", i, starid );
	}
	CNOVRVBOTaint( modelConst->pGeos[0] );
	CNOVRVBOTaint( modelConst->pGeos[1] );
	CNOVRModelTaintIndices( modelConst );

	model = CNOVRModelCreate( numstars, GL_POINTS );
	for( i = 0; i < numstars; i++ )
	{
		model->pIndices[i] = i;
	}
	CNOVRModelSetNumVBOs( model, 2 );
	model->pGeos[0] = CNOVRCreateVBO( 4, 0, numstars, 0 );
	model->pGeos[1] = CNOVRCreateVBO( 4, 0, numstars, 1 );
	memcpy( model->pGeos[0]->pVertices, starfield_data1, numstars * sizeof(float) * 4 );
	memcpy( model->pGeos[1]->pVertices, starfield_data2, numstars * sizeof(float) * 4 );
	CNOVRVBOTaint( model->pGeos[0] );
	CNOVRVBOTaint( model->pGeos[1] );
	CNOVRModelTaintIndices( model );
}

void CNOVRGenericSetInteractable( cnovr_model * m, void * tag, cnovr_model_focus_controller ** fcptr, cnovr_cb_fn collisioncheck, cnovrfocus_capture * focusevent )
{

}

void init()
{
}

void start( const char * identifier )
{
	printf( "Loading stars\n" );
	int len = 0;
	database = (flat_star*)FileToString( "modules/starfield/tablize/flat_stars.dat", &len );
	numstars = len/sizeof(flat_star);
	printf( "Loaded %d bytes, %d stars\n", len, numstars );

	if( !database )
	{
		printf( "ERROR: Cannot load star database.\n" );
		return; //Don't proceed.
	}

	starfield_data1 = malloc( sizeof( float ) * 4 * numstars );
	starfield_data2 = malloc( sizeof( float ) * 4 * numstars );

	int i;
	for( i = 0; i < numstars; i++ )
	{
		flat_star * s = database + i;
		float * f1 = starfield_data1 + i * 4;
		float * f2 = starfield_data2 + i * 4;
		f1[0] = (((double)s->rascention_bams)/(1L<<32))*CNOVRPI*2; //to get radians
		f1[1] = (((double)s->declination_bams)/(1L<<31))*CNOVRPI;	//to get radians
		f1[2] = ((double)s->parallax_10uas)/100.0;	//to get mas
		f1[3] = 1;
		f2[0] = ((double)s->magnitude_mag1000)/1000.0;
		f2[1] = ((double)s->bvcolor_mag1000)/1000.0;
		f2[2] = ((double)s->vicolor_mag1000)/1000.0;
		f2[3] = 1;
	}

	CNOVRJobTack( cnovrQPrerender, starfield_setup, 0, 0, 0 );
	//starfield_setup( 0, 0 );

	//CNOVRGenericSetInteractable( 0, cnovr_model_focus_controller ** fcptr, cnovr_cb_fn collisioncheck, cnovrfocus_capture * focusevent )
}

void stop( const char * identifier )
{
}

