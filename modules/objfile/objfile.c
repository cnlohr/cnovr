#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <stdlib.h>
#include <string.h>
#include <cntools/cnrbtree/cnrbtree.h>

const char * identifier;
cnovr_shader * shaderBasic;
cnovr_texture * texture;
cnovr_pose    modelpose;
cnovr_model * modelobj;
cnovrfocus_capture modelcapture;

struct staticstore
{
	int initialized;
	cnovr_pose    modelpose;
} * store;

int FocusEvent( int event, cnovrfocus_capture * cap, cnovrfocus_properties * prop, int buttoninfo );

void init( const char * identifier )
{
	ovrprintf( "objmodel init %s\n", identifier );
}

void UpdateFunction( void * tag, void * opaquev )
{
	static double start;
	double now = OGGetAbsoluteTime();
	return;
}


void RenderFunction( void * tag, void * opaquev )
{
	int i;
	CNOVRRender( texture );
	CNOVRRender( shaderBasic );
	CNOVRRender( modelobj );
}


static void objfile_scene_setup( void * tag, void * opaquev )
{
	printf( "+++ OBJ Loader\n" );
	int i;

	cnstrstrmap * map = (cnstrstrmap*)GetOtherTCCProperties();

	shaderBasic = CNOVRShaderCreate( RBHAS( map, "shader" )?RBA( map, "shader" ):"assets/basic" );
	modelobj = CNOVRModelCreate( 0, GL_TRIANGLES );
	modelobj->pose = &modelpose;
	pose_make_identity( &modelpose );
	CNOVRModelLoadFromFileAsync( modelobj, RBHAS( map, "model" )?RBA( map, "model" ):"isosphere.obj" );
	modelcapture.tag = 0;
	modelcapture.opaque = modelobj;
	modelcapture.cb = CNOVRFocusDefaultFocusEvent;
	CNOVRModelSetInteractable( modelobj, &modelcapture );


	texture = CNOVRTextureCreate( 0, 0, 0 ); //Set to all 0 to have the load control these details.
	CNOVRTextureLoadFileAsync( texture, RBHAS( map, "texture" )?RBA( map, "texture" ):"test.png" );

	UpdateFunction(0,0);
	CNOVRListAdd( cnovrLUpdate, 0, UpdateFunction );
	CNOVRListAdd( cnovrLRender2, 0, RenderFunction );

	printf( "+++ OBJ Loader setup complete\n" );
}


void start( const char * identifier )
{
	store = CNOVRNamedPtrData( trprintf( "objloader%s", identifier ), 0, sizeof( *store ) + 1024 );
	printf( "=== Initializing %p\n", store );
//	store->initialized = 0;
	if( !store->initialized )
	{
		pose_make_identity( &modelpose );
		store->initialized = 1;
	}

	printf( "OTHERPROPS: %p\n", GetOtherTCCProperties() );
	
	identifier = strdup(identifier);
	CNOVRJobTack( cnovrQPrerender, objfile_scene_setup, 0, 0, 0 );
	printf( "=== Example start %s(%p) + %p %p\n", identifier, identifier );
}

void stop( const char * identifier )
{
	printf( "=== End Example stop\n" );
}


