#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <stdlib.h>
#include <string.h>

cnovr_shader * shaderBasic;
cnovr_model * isosphere;
cnovr_pose    isospherepose;
cnovrfocus_capture isocapture;


void init( const char * identifier )
{
	ovrprintf( "Example init %s\n", identifier );
}

void UpdateFunction( void * tag, void * opaquev )
{
	static double start;
	double now = OGGetAbsoluteTime();
	if( start < 1 ) start = now;

	// Do nothing.

	return;
}


void RenderFunction( void * tag, void * opaquev )
{
	int i;
	CNOVRRender( shaderBasic );
	CNOVRRender( isosphere );
}


static void example_scene_setup( void * tag, void * opaquev )
{
	printf( "+++ Example scene setup\n" );
	int i;
	shaderBasic = CNOVRShaderCreate( "assets/basic" );
	isosphere = CNOVRModelCreate( 0, GL_TRIANGLES );
	isosphere->pose = &isospherepose;
	pose_make_identity( &isospherepose );
	isospherepose.Pos[2]++;
	isospherepose.Scale = .1;
	CNOVRModelLoadFromFileAsync( isosphere, "isosphere.obj" );
	isocapture.tag = 0;
	isocapture.opaque = isosphere;
	isocapture.cb = CNOVRFocusDefaultFocusEvent;
	CNOVRModelSetInteractable( isosphere, &isocapture );

	UpdateFunction(0,0);
	CNOVRListAdd( cnovrLUpdate, 0, UpdateFunction );
	CNOVRListAdd( cnovrLRender2, 0, RenderFunction );
	//CNOVRListAdd( cnovrLCollide, 0, CollideFunction );
	printf( "+++ Example scene setup complete\n" );
}


void start( const char * identifier )
{
	CNOVRJobTack( cnovrQPrerender, example_scene_setup, 0, 0, 0 );
	printf( "=== Example start %s(%p) + %p %p\n", identifier, identifier );
}

void stop( const char * identifier )
{
	printf( "=== End Example stop\n" );
}


