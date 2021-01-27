#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovrcanvas.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <stdlib.h>
#include <string.h>

double StartTime;

const char * identifier;
cnovr_shader * shader;
cnovr_shader * shaderBasic;
cnovr_texture * texture;

cnovr_shader * colorsplotchshader;
cnovr_model * colorsplotch;
cnovrfocus_capture colorsplotchcapture;

cnovr_shader * spinningdiskshader;
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
	cnovr_pose colorsplotchpose;
	int colormode;
	int slide[3];
	int spinmode;
	int spinn[3];
} * store;


char ColorSlider0[128];
char ColorSlider1[128];
char ColorSlider2[128];
char Spinnerslider0[128];
float ColorParameters[4];
float SpinnerParameters[4]; //The first 2 are reserved.

void UpdateMenu()
{
	if( store->colormode > 5 ) store->colormode = 0;
	sprintf ( ColorSlider0, "%s: %d\n", ((const char*[6]){ "RED", "GRN", "BLU", "HUE", "SAT", "VAL" })[store->colormode], store->slide[0] );
	sprintf ( ColorSlider1, "Um: %d\n", store->slide[1] );
	sprintf ( ColorSlider2, "Vm: %d\n", store->slide[2] );
	if( store->spinmode > 1 ) store->spinmode = 1;
	sprintf ( Spinnerslider0, "%s speed: %d\n", ((const char*[2]){ "Time", "Frame" })[store->spinmode], store->spinn[0] );
	SpinnerParameters[2] = store->spinmode;
	SpinnerParameters[3] = store->spinn[0]/255.;
	
	ColorParameters[0] = (float)store->colormode;
	ColorParameters[1] = (float)store->slide[0]/255.;
	ColorParameters[2] = (float)store->slide[1]/255.;
	ColorParameters[3] = (float)store->slide[2]/255.;
}

void DefaultMenu()
{
	store->colormode = 0;
	store->slide[0] = 255;
	store->slide[1] = 255;
	store->slide[2] = 255;
	
	store->spinmode = 0;
	store->spinn[0] = 20;
	UpdateMenu();
}

void adjust_slider( struct cnovr_canvas_t * canvas, const struct cnovr_canvas_canned_gui_element_t * elem, int rx, int ry, cnovrfocus_event event, int devid )
{
	if( rx > 255 ) rx = 255; if( rx < 0 ) rx = 0;
	store->slide[elem->iopaque] = rx;
	UpdateMenu();
	CNOVRNamedPtrSave( "testworldcodestore" );
}

void adjust_spinner( struct cnovr_canvas_t * canvas, const struct cnovr_canvas_canned_gui_element_t * elem, int rx, int ry, cnovrfocus_event event, int devid )
{
	if( rx > 255 ) rx = 255; if( rx < 0 ) rx = 0;
	store->spinn[elem->iopaque] = rx;
	UpdateMenu();
	CNOVRNamedPtrSave( "testworldcodestore" );
}

void color_select_button( struct cnovr_canvas_t * canvas, const struct cnovr_canvas_canned_gui_element_t * elem, int rx, int ry, cnovrfocus_event event, int devid )
{
	store->colormode = elem->iopaque;
	UpdateMenu();
	CNOVRNamedPtrSave( "testworldcodestore" );
}
void spinner_select_button( struct cnovr_canvas_t * canvas, const struct cnovr_canvas_canned_gui_element_t * elem, int rx, int ry, cnovrfocus_event event, int devid )
{
	store->spinmode = elem->iopaque;
	UpdateMenu();
	CNOVRNamedPtrSave( "testworldcodestore" );
}

#define MENUH 16
#define MENUY(y) ((MENUH*(y))+2)
#define MENUHEXX(x) (26*x+2)
#define DEF_WIDTH (160-26)
#define EXTRA_WIDTH (160-4)

struct cnovr_canvas_canned_gui_element_t menu_canvas[] = {
	{ .x = 2, .y = MENUY(0), .w = 90,  .h = 14, .cb = 0, .text = "Color Splotch" },
	{ .x = 2, .y = MENUY(1), .w = 256, .h = 14, .cb = 0, .text = ColorSlider0, .cb = adjust_slider, .iopaque = 0, .allowdrag = 1  },
	{ .x = 2, .y = MENUY(2), .w = 256, .h = 14, .cb = 0, .text = ColorSlider1, .cb = adjust_slider, .iopaque = 1, .allowdrag = 1  },
	{ .x = 2, .y = MENUY(3), .w = 256, .h = 14, .cb = 0, .text = ColorSlider2, .cb = adjust_slider, .iopaque = 2, .allowdrag = 1  },
	{ .x = MENUHEXX(0), .y = MENUY(4), .w = 24, .h = 14, .cb = 0, .text = "RED", .cb = color_select_button, .iopaque = 0, },
	{ .x = MENUHEXX(1), .y = MENUY(4), .w = 24, .h = 14, .cb = 0, .text = "GRN", .cb = color_select_button, .iopaque = 1, },
	{ .x = MENUHEXX(2), .y = MENUY(4), .w = 24, .h = 14, .cb = 0, .text = "BLU", .cb = color_select_button, .iopaque = 2, },
	{ .x = MENUHEXX(3), .y = MENUY(4), .w = 24, .h = 14, .cb = 0, .text = "HUE", .cb = color_select_button, .iopaque = 3, },
	{ .x = MENUHEXX(4), .y = MENUY(4), .w = 24, .h = 14, .cb = 0, .text = "SAT", .cb = color_select_button, .iopaque = 4, },
	{ .x = MENUHEXX(5), .y = MENUY(4), .w = 24, .h = 14, .cb = 0, .text = "VAL", .cb = color_select_button, .iopaque = 5, },
	{ .x = 2, .y = MENUY(5), .w = 90,  .h = 14, .cb = 0, .text = "Spinner" },
	{ .x = 2, .y = MENUY(6), .w = 256, .h = 14, .cb = 0, .text = Spinnerslider0, .cb = adjust_spinner, .iopaque = 0, .allowdrag = 1  },
	{ .x = MENUHEXX(0), .y = MENUY(7), .w = 50, .h = 14, .cb = 0, .text = "TIME", .cb = spinner_select_button, .iopaque = 0, },
	{ .x = MENUHEXX(2), .y = MENUY(7), .w = 50, .h = 14, .cb = 0, .text = "FRAMES", .cb = spinner_select_button, .iopaque = 1, },

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
	double now = OGGetAbsoluteTime();

	int i;
	double truedt = now - StartTime;

	CNOVRCanvasApplyCannedGUI( canvas, menu_canvas );
	CNOVRCanvasSwapBuffers( canvas );

	return;
}






void RenderFunction( void * tag, void * opaquev )
{
	static int framecount;
	framecount++;
	CNOVRRender( shader );
	CNOVRRender( texture );
	CNOVRRender( shaderBasic );
	CNOVRRender( isosphere );
	
	CNOVRRender( spinningdiskshader );
	glUniform4f( CNOVRMAPPEDUNIFORMPOS( 19 ), OGGetAbsoluteTime() - StartTime, framecount, SpinnerParameters[2], SpinnerParameters[3] );
	CNOVRRender( spinningdisk );

	CNOVRRender( colorsplotchshader );
	glUniform4f( CNOVRMAPPEDUNIFORMPOS( 19 ), ColorParameters[0], ColorParameters[1], ColorParameters[2], ColorParameters[3] );
	CNOVRRender( colorsplotch );



	CNOVRRender( canvas );
}


static void testworld_scene_setup( void * tag, void * opaquev )
{
	printf( "+++ Test World scene setup\n" );
	int i;
	shader = CNOVRShaderCreate( "assets/example" );
	shaderBasic = CNOVRShaderCreate( "assets/basic" );
	colorsplotchshader = CNOVRShaderCreate( "colorsplotch" );
	spinningdiskshader = CNOVRShaderCreate( "spinningdisk" );
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

	colorsplotch = CNOVRModelCreate( 0, GL_TRIANGLES );
	colorsplotch->pose = &store->colorsplotchpose;
	CNOVRModelAppendMesh( colorsplotch, 1, 1, 0, (cnovr_point3d){ 1.f, 1.f, 1.f }, 0, 0 );
	colorsplotchcapture.tag = 0;
	colorsplotchcapture.opaque = colorsplotch;
	colorsplotchcapture.cb = TestWorldFocusEvent;
	CNOVRModelSetInteractable( colorsplotch, &colorsplotchcapture );

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
	StartTime = OGGetAbsoluteTime();

	store = CNOVRNamedPtrData( "testworldcodestore", 0, sizeof( *store ) + 1024 );
	printf( "=== Initializing %p\n", store );
	//store->initialized = 0;
	if( !store->initialized )
	{
		int i;
		pose_make_identity( &store->spinningdiskpose );
		pose_make_identity( &store->colorsplotchpose );
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


