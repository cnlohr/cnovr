#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovrcanvas.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <stdlib.h>
#include <string.h>

const char * identifier;
cnovr_shader * shader;
cnovr_shader * shaderBasic;
cnovr_texture * texture;

cnovr_shader * colorsplotch;
cnovr_model * spinningdisk;
cnovrfocus_capture spinningdiskcapture;

cnovr_model * isosphere;
cnovr_pose    isospherepose;
cnovrfocus_capture isocapture;

cnovr_canvas * canvas;

int shutting_down;
struct staticstore
{
	int initialized;
	cnovr_pose spinningdiskpose;
	
	int blueLevel;
} * store;


char ColorSliderB[128];
float ColorParamters[4];

void UpdateMenu()
{
	sprintf ( ColorSliderB, "B: %d\n", store->blueLevel );
	ColorParamters[0] = (float)store->blueLevel/255.;	
}

void DefaultMenu()
{
		store->blueLevel = 0;
}

void test_button( struct cnovr_canvas_t * canvas, const struct cnovr_canvas_canned_gui_element_t * elem, int rx, int ry, cnovrfocus_event event, int devid )
{
	store->blueLevel = rx;
	UpdateMenu();
	CNOVRNamedPtrSave( "testworldcodestore" );
}

#define MENUH 16
#define MENUY(y) ((MENUH*(y))+2)
#define MENUHEXX(x) (26*x+2)
#define DEF_WIDTH (160-26)
#define EXTRA_WIDTH (160-4)

const const struct cnovr_canvas_canned_gui_element_t menu_canvas[] = {
	{ .x = 2, .y = MENUY(0), .w = 90, .h = 14, .cb = 0, .text = "Test Text" },
	{ .x = 2, .y = MENUY(1), .w = 256, .h = 14, .cb = 0, .text = ColorSliderB, .cb = test_button, .vopaque = 0, .allowdrag = 1  },
	{ .cb = 0, .w = 0, .h = 0 }
};

int TestWorldFocusEvent( int event, cnovrfocus_capture * cap, cnovrfocus_properties * prop, int buttoninfo )
{
	//printf( "EVENT: %d %d %d\n", event, cap->opaque, buttoninfo );
	cnovr_model * m = (cnovr_model*)cap->opaque;
	int id = m->iOpaque;
	switch( event )
	{
		case CNOVRF_DOWNNOFOCUS:
			//if( buttoninfo == 0 ) { store->zapped[id] = .999; return 0; } //Catpured event
			break;
		case CNOVRF_LOSTFOCUS:
			printf( "SAVING\n" );
			CNOVRNamedPtrSave( "testworldcodestore" );
			break;
		case CNOVRF_ACQUIREDFOCUS:
			//store->zapped[id] = 1;
			break;
	}

	CNOVRGeneralHandleFocusEvent( m->focuscontrol, m->pose, prop, event, buttoninfo, CTRLA_PINCHBTN );

	return 0;
}



int FocusEvent( int event, cnovrfocus_capture * cap, cnovrfocus_properties * prop, int buttoninfo );

void init( const char * identifier )
{
	ovrprintf( "Test World init %s\n", identifier );
}

void UpdateFunction( void * tag, void * opaquev )
{

	static double start;
	double now = OGGetAbsoluteTime();
	if( start < 1 ) start = now;

	int i;
	double truedt = now - start;

	CNOVRCanvasApplyCannedGUI( canvas, menu_canvas );
	CNOVRCanvasSwapBuffers( canvas );

	return;
}






void RenderFunction( void * tag, void * opaquev )
{
	CNOVRRender( shader );
	CNOVRRender( texture );
	CNOVRRender( shaderBasic );
	CNOVRRender( isosphere );
	
	CNOVRRender( colorsplotch );
	glUniform4f( CNOVRMAPPEDUNIFORMPOS( 19 ), ColorParamters[0], ColorParamters[1], ColorParamters[2], ColorParamters[3] );
	CNOVRRender( spinningdisk );
	CNOVRRender( canvas );
}


static void testworld_scene_setup( void * tag, void * opaquev )
{
	printf( "+++ Test World scene setup\n" );
	int i;
	shader = CNOVRShaderCreate( "assets/example" );
	shaderBasic = CNOVRShaderCreate( "assets/basic" );
	colorsplotch = CNOVRShaderCreate( "colorsplotch" );
/*
	cnovr_simple_node * root = cnovrstate->pRootNode;
	node = CNOVRNodeCreateSimple( 1 );
	model = CNOVRModelCreate( 0, GL_TRIANGLES );
	CNOVRModelAppendCube( model, (cnovr_point3d){ 1.f, 1.f, 1.f }, 0, 0 );
	CNOVRNodeAddObject( node, shader );
	CNOVRNodeAddObject( node, model );
	CNOVRNodeAddObject( root, node );
*/
	canvas = CNOVRCanvasCreate( "TestWorldCanvas", 320, 240, 0 );
	UpdateMenu();

	srand( 0 );

	spinningdisk = CNOVRModelCreate( 0, GL_TRIANGLES );
	spinningdisk->pose = &store->spinningdiskpose;
	CNOVRModelAppendMesh( spinningdisk, 1, 1, 0, (cnovr_point3d){ 1.f, 1.f, 1.f }, 0, 0 );
	spinningdiskcapture.tag = 0;
	spinningdiskcapture.opaque = spinningdisk;
	spinningdiskcapture.cb = TestWorldFocusEvent;
	CNOVRModelSetInteractable( spinningdisk, &spinningdiskcapture );

	isosphere = CNOVRModelCreate( 0, GL_TRIANGLES );
	isosphere->pose = &isospherepose;
	pose_make_identity( &isospherepose );
	CNOVRModelLoadFromFileAsync( isosphere, "isosphere.obj" );
	isocapture.tag = 0;
	isocapture.opaque = isosphere;
	isocapture.cb = CNOVRFocusDefaultFocusEvent;
	CNOVRModelSetInteractable( isosphere, &isocapture );


	texture = CNOVRTextureCreate( 0, 0, 0 ); //Set to all 0 to have the load control these details.
	CNOVRTextureLoadFileAsync( texture, "test.png" );

	UpdateFunction(0,0);
	CNOVRListAdd( cnovrLUpdate, 0, UpdateFunction );
	CNOVRListAdd( cnovrLRender2, 0, RenderFunction );
	//CNOVRListAdd( cnovrLCollide, 0, CollideFunction );
	printf( "+++ Test World scene setup complete\n" );
}


void start( const char * identifier )
{

	store = CNOVRNamedPtrData( "testworldcodestore", 0, sizeof( *store ) + 1024 );
	printf( "=== Initializing %p\n", store );
//	store->initialized = 0;
	if( !store->initialized )
	{
		int i;
		pose_make_identity( &store->spinningdiskpose );
		DefaultMenu();
		store->initialized = 1;
	}
	
	identifier = strdup(identifier);
	CNOVRJobTack( cnovrQPrerender, testworld_scene_setup, 0, 0, 0 );
	printf( "=== Example start %s(%p) + %p %p\n", identifier, identifier );
}

void stop( const char * identifier )
{
	shutting_down = 1;
	//OGCancelThread( thdmax );
	printf( "=== End Example stop\n" );
}


