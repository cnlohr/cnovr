#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <stdlib.h>
#include <string.h>
#include <cntools/cnrbtree/cnrbtree.h>

const char * midentifier;
cnovr_shader * shaderBasic;
cnovr_texture * texture;
cnovr_model * modelobj;
cnovrfocus_capture modelcapture;
char * ptrname;
int backfaces = 0;

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
	if( backfaces ) glDisable(GL_CULL_FACE);  
	CNOVRRender( texture );
	CNOVRRender( shaderBasic );
	CNOVRRender( modelobj );
	if( backfaces ) glEnable(GL_CULL_FACE);  
}



int SaveFocusEvent( int event, cnovrfocus_capture * cap, cnovrfocus_properties * prop, int buttoninfo )
{
	cnovr_model * m = (cnovr_model*)cap->opaque;
	int id = m->iOpaque;
	switch( event )
	{
		case CNOVRF_DOWNNOFOCUS:
			//Catpured event
			break;
		case CNOVRF_LOSTFOCUS:
			printf( "=== Saving %p (%s) INITIALIZED:%d\n", store, ptrname, store->initialized );
			printf( "OTHERPROPS: %f\n", store->modelpose.Scale );
			CNOVRNamedPtrSave( ptrname );
			break;
		case CNOVRF_ACQUIREDFOCUS:
			break;
	}

	CNOVRGeneralHandleFocusEvent( m->focuscontrol, m->pose, prop, event, buttoninfo, CTRLA_PINCHBTN );

	if( event == CNOVRF_DRAG )
	{
		//Do some sort of last-second fixup here? Snapping?
	}

	return 0;
}

static void objfile_scene_setup( void * tag, void * opaquev )
{
	printf( "+++ OBJ Loader\n" );
	int i;

	cnstrstrmap * map = (cnstrstrmap*)GetOtherTCCProperties();
	shaderBasic = CNOVRShaderCreateWithPrefix( (map && RBHAS( map, "shader" ))?RBA( map, "shader" ):"assets/basic",
		(map && RBHAS( map, "shaderprefix" ))?RBA( map, "shaderprefix" ):"" );
	modelobj = CNOVRModelCreate( 0, GL_TRIANGLES );
	modelobj->pose = &store->modelpose;
 
	CNOVRModelLoadFromFileAsync( modelobj, (map && RBHAS( map, "model" ))?RBA( map, "model" ):"isosphere.obj" );
	modelcapture.tag = 0;
	modelcapture.opaque = modelobj;
	modelcapture.cb = SaveFocusEvent;
	CNOVRModelSetInteractable( modelobj, &modelcapture );


	modelobj->iRenderMesh = (map && RBHAS( map, "rendermesh" ))?atoi(RBA( map, "rendermesh" )):-1;
	modelobj->iCollideMesh = (map && RBHAS( map, "collidemesh" ))?atoi(RBA( map, "collidemesh" )):-1;
	backfaces = (map && RBHAS( map, "backfaces" ))?atoi(RBA( map, "backfaces" )):0;
	printf( "BACKFACES: %d\n", backfaces );
	texture = CNOVRTextureCreate( 0, 0, 0 ); //Set to all 0 to have the load control these details.
	CNOVRTextureLoadFileAsync( texture, (map && RBHAS( map, "texture" ))?RBA( map, "texture" ):"test.png" );

	UpdateFunction(0,0);
	CNOVRListAdd( cnovrLUpdate, 0, UpdateFunction );
	CNOVRListAdd( cnovrLRender2, 0, RenderFunction );

	printf( "+++ OBJ Loader setup complete\n" );
}


void start( const char * identifier )
{
	midentifier = strdup(identifier);
	ptrname = strdup( trprintf( "objloader%s", identifier ) );
	store = CNOVRNamedPtrData( ptrname, 0, sizeof( *store ) * 2 );
	printf( "=== Initializing %p (%s) INITIALIZED:%d\n", store, ptrname, store->initialized );
//	store->initialized = 0;
	if( !store->initialized )
	{
		pose_make_identity( &store->modelpose );
		store->initialized = 1;
	}

	printf( "OTHERPROPS: %p %f\n", GetOtherTCCProperties(), store->modelpose.Scale );
	
	CNOVRJobTack( cnovrQPrerender, objfile_scene_setup, 0, 0, 0 );
	printf( "=== Example start %s(%p) + %p %p\n", midentifier, identifier );
}

void stop( const char * identifier )
{
	printf( "=== End Example stop\n" );
}


