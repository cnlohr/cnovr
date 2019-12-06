#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovropenvr.h>
#include <cnovr.h>
#include <string.h>
#include <cnovrutil.h>
#include <chew.h>

const char *  identifier;
cnovr_shader * shaderlines;
cnovr_shader * shaderblack;
cnovr_model *  modelcameralines;
cnovr_model *  modelcamerasolid;
cnovrfocus_capture capture;
	
struct camerastore_t
{
	int initialized;
	cnovr_pose  posecamera;
} * store;

void init( const char * identifier )
{
}

static void RenderFunction( void * tag, void * opaquev )
{
	int i;
//	glEnable( GL_LINE_SMOOTH );
	CNOVRRender( shaderblack );
	CNOVRModelRenderWithPose( modelcamerasolid, &store->posecamera );
	CNOVRRender( shaderlines );
	CNOVRModelRenderWithPose( modelcameralines, &store->posecamera );
}

static void UpdateCamera()
{
	cnovr_pose pin;
	memcpy( &pin,  &store->posecamera, sizeof( cnovr_pose ) );
	pin.Scale = 1;
	cnovr_pose pinvert;
	pose_invert( &pinvert, &pin );
	pinvert.Scale = 1;
	memcpy( &cnovrstate->pPreviewPose, &pinvert, sizeof( cnovr_pose ) );
}

static int CameraFocusEvent( int event, cnovrfocus_capture * cap, cnovrfocus_properties * prop, int buttoninfo )
{
	//void * c = (void*)cap->opaque;
	//cnovr_model * m = c->model;
	//int id = m->iOpaque;
	if( event == CNOVRF_LOSTFOCUS )
	{
		CNOVRNamedPtrSave( "camerastore" );
	}
	UpdateCamera();
	CNOVRGeneralHandleFocusEvent( modelcamerasolid->focuscontrol, modelcamerasolid->pose, prop, event, buttoninfo, CTRLA_PINCHBTN );
	return 0;
}


static void example_scene_setup( void * tag, void * opaquev )
{
	shaderlines = CNOVRShaderCreate( "assets/basic" );
	shaderblack = CNOVRShaderCreate( "assets/black" );
	modelcameralines = CNOVRModelCreate( 0, GL_LINES );
	modelcamerasolid = CNOVRModelCreate( 0, GL_TRIANGLES );
	modelcameralines->pose = &store->posecamera;
	modelcamerasolid->pose = &store->posecamera;
	CNOVRModelLoadFromFileAsync( modelcameralines, "camera.obj:lineify" );
	CNOVRModelLoadFromFileAsync( modelcamerasolid, "camera.obj" );
	CNOVRListAdd( cnovrLRender2, 0, RenderFunction );

	capture.tag = 0;
	capture.opaque = 0;
	capture.cb = CameraFocusEvent;
	CNOVRModelSetInteractable( modelcamerasolid, &capture );

	if( !store->initialized )
	{
		printf( "initializing camera\n" );
		pose_make_identity( &store->posecamera );
		store->initialized = 1;
	}
	UpdateCamera();

	printf( "...loaded %f %f %f  %f %f %f %f\n", PFTHREE( store->posecamera.Pos ), PFFOUR( store->posecamera.Rot ) );
}


void start( const char * identifier )
{
	store = CNOVRNamedPtrData( "camerastore", "camerastore", sizeof( *store ) + 128 );
	printf( "=== Initializing %p\n", store );

	identifier = strdup(identifier);
	CNOVRJobTack( cnovrQPrerender, example_scene_setup, 0, 0, 0 );
	printf( "=== Example start %s(%p) + %p %p\n", identifier, identifier );

}

void stop( const char * identifier )
{
	printf( "=== End Example stop\n" );
}


