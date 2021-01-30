#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovrcanvas.h>
#include <cnovrcnfa.h>
#include <cnovrutil.h>
#include <cnovr.h>
#include <stdlib.h>
#include <string.h>

double StartTime;

int framecount;
int audio_frame_count;
double doot_at;

double SpinRotationDraw;
double TimeOfNextHit;
double TimeOfLastHit;
float  BeezParam;

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

cnovr_shader * beezshader;
cnovr_model * beez;
cnovrfocus_capture beezcapture;

og_thread_t thddoot;

/*
cnovr_model * isosphere;
cnovr_pose    isospherepose;
cnovrfocus_capture isocapture;
*/
cnovr_canvas * canvas;

int shutting_down;
struct staticstore
{
	int initialized;
	cnovr_pose spinningdiskpose;
	cnovr_pose colorsplotchpose;
	cnovr_pose beezpose;
	int colormode;
	int slide[3];
	int spinmode;
	int spinn[3];
	int time_add_microseconds;
	int time_add_stutter_every;
	int time_add_stutter_amount;
	int beez_every, beez_for;
	
	int do_doot;
} * store;


char ColorSlider0[128];
char ColorSlider1[128];
char ColorSlider2[128];
char Spinnerslider0[128];
char FrameWaitText[128];
char BeezEvery[128];
char BeezFor[128];
char FrameStutterEveryText[128];
char FrameStutterAmountText[128];
char DootTimingText[128];
float ColorParameters[4];

void UpdateMenu()
{
	if( store->colormode > 5 ) store->colormode = 0;
	sprintf( ColorSlider0, "%s: %d", ((const char*[6]){ "RED", "GRN", "BLU", "HUE", "SAT", "VAL" })[store->colormode], store->slide[0] );
	sprintf( ColorSlider1, "Um: %d", store->slide[1] );
	sprintf( ColorSlider2, "Vm: %d", store->slide[2] );
	ColorParameters[0] = (float)store->colormode;
	ColorParameters[1] = (float)store->slide[0]/255.;
	ColorParameters[2] = (float)store->slide[1]/255.;
	ColorParameters[3] = (float)store->slide[2]/255.;

	if( store->spinmode > 1 ) store->spinmode = 1;
	sprintf( Spinnerslider0, "%s speed: %d", ((const char*[2]){ "Time", "Frame" })[store->spinmode], store->spinn[0] );
	
	sprintf( FrameWaitText, "Extra Frame Delay: %4.1fms", store->time_add_microseconds/1000. );
	sprintf( FrameStutterEveryText, "Stutter frame every %d frames", store->time_add_stutter_every );
	sprintf( FrameStutterAmountText, "Frame Stutter Delay: %4.1fms", store->time_add_stutter_amount/1000. );

	sprintf( BeezEvery, "Beez Every %d", store->beez_every );
	sprintf( BeezFor, "Beez For %d", store->beez_for );
}

void DefaultMenu()
{
	store->colormode = 0;
	store->slide[0] = 255;
	store->slide[1] = 255;
	store->slide[2] = 255;

	
	store->spinmode = 0;
	store->spinn[0] = 20;
	store->time_add_microseconds = 0;
	store->time_add_stutter_every = 0;
	store->time_add_stutter_amount = 0;
	
	store->beez_every = 0;
	store->beez_for = 0;
	
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

void adjust_frame_wait( struct cnovr_canvas_t * canvas, const struct cnovr_canvas_canned_gui_element_t * elem, int rx, int ry, cnovrfocus_event event, int devid )
{
	if( rx > 255 ) rx = 255; if( rx < 0 ) rx = 0;
	switch( elem->iopaque )
	{
	case 0:	
		store->time_add_microseconds = rx * 200;
		break;
	case 1:
		store->time_add_stutter_every = rx / 5;
		break;
	case 2:
		store->time_add_stutter_amount = rx * 200;
		break;
	}
	UpdateMenu();
	CNOVRNamedPtrSave( "testworldcodestore" );
}


void adjust_beez( struct cnovr_canvas_t * canvas, const struct cnovr_canvas_canned_gui_element_t * elem, int rx, int ry, cnovrfocus_event event, int devid )
{
	if( rx > 255 ) rx = 255; if( rx < 0 ) rx = 0;
	switch( elem->iopaque )
	{
	case 0:	
		store->beez_every = rx/5;
		break;
	case 1:
		store->beez_for = rx/5;
		break;
	}
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
	if( elem->iopaque < 2 )
	{
		store->spinmode = elem->iopaque;
	}
	else if( elem->iopaque == 2 )
	{
		store->do_doot = !store->do_doot;
	}
	UpdateMenu();
	CNOVRNamedPtrSave( "testworldcodestore" );
}

#define MENUH 16
#define MENUY(y) ((MENUH*(y))+2)
#define MENUHEXX(x) (26*x+2)
#define DEF_WIDTH (160-26)
#define EXTRA_WIDTH (160-4)

#define MENUSTART __COUNTER__
#define MENUNEXT  (__COUNTER__-MENUSTART)

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
	{ .x = 50, .y = MENUY(5), .w = 90,  .h = 14, .cb = 0, .text = DootTimingText },
	{ .x = 2, .y = MENUY(6), .w = 256, .h = 14, .cb = 0, .text = Spinnerslider0, .cb = adjust_spinner, .iopaque = 0, .allowdrag = 1  },
	{ .x = MENUHEXX(0), .y = MENUY(7), .w = 50, .h = 14, .cb = 0, .text = "TIME", .cb = spinner_select_button, .iopaque = 0, },
	{ .x = MENUHEXX(2), .y = MENUY(7), .w = 50, .h = 14, .cb = 0, .text = "FRAMES", .cb = spinner_select_button, .iopaque = 1, },
	{ .x = MENUHEXX(4), .y = MENUY(7), .w = 50, .h = 14, .cb = 0, .text = "DOOT", .cb = spinner_select_button, .iopaque = 2, },

	{ .x = 2, .y = MENUY(8), .w = 90,  .h = 14, .cb = 0, .text = "Frame Wait" },
	{ .x = 2, .y = MENUY(9), .w = 256, .h = 14, .cb = 0, .text = FrameWaitText, .cb = adjust_frame_wait, .iopaque = 0, .allowdrag = 1  },
	{ .x = 2, .y = MENUY(10), .w = 256, .h = 14, .cb = 0, .text = FrameStutterEveryText, .cb = adjust_frame_wait, .iopaque = 1, .allowdrag = 1  },
	{ .x = 2, .y = MENUY(11), .w = 256, .h = 14, .cb = 0, .text = FrameStutterAmountText, .cb = adjust_frame_wait, .iopaque = 2, .allowdrag = 1  },

	{ .x = 2, .y = MENUY(12), .w = 90,  .h = 14, .cb = 0, .text = "Beez" },
	{ .x = 2, .y = MENUY(13), .w = 256, .h = 14, .cb = 0, .text = BeezEvery, .cb = adjust_beez, .iopaque = 0, .allowdrag = 1  },
	{ .x = 2, .y = MENUY(14), .w = 256, .h = 14, .cb = 0, .text = BeezFor, .cb = adjust_beez, .iopaque = 1, .allowdrag = 1  },

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
	CNOVRRender( shader );
	//CNOVRRender( texture );
	//CNOVRRender( shaderBasic );
	//CNOVRRender( isosphere );
	
	CNOVRRender( spinningdiskshader );
	glUniform4f( CNOVRMAPPEDUNIFORMPOS( 19 ), SpinRotationDraw, 0, 0, 0 );
	CNOVRRender( spinningdisk );

	CNOVRRender( beezshader );
	glUniform4f( CNOVRMAPPEDUNIFORMPOS( 19 ), BeezParam, OGGetAbsoluteTime(), 0, 0 );
	CNOVRRender( beez );

	CNOVRRender( colorsplotchshader );
	glUniform4f( CNOVRMAPPEDUNIFORMPOS( 19 ), ColorParameters[0], ColorParameters[1], ColorParameters[2], ColorParameters[3] );
	CNOVRRender( colorsplotch );

	CNOVRRender( canvas );
}

void PrerenderFunction( void * tag, void * opaquev )
{
	framecount++;
	int delay = store->time_add_microseconds;
	if( store->time_add_stutter_every > 0 )
	{
		if( (framecount % store->time_add_stutter_every) == 0 )
		{
			delay += store->time_add_stutter_amount;
		}
	}
	
	if( store->beez_every )
	{
		BeezParam = (framecount%store->beez_every) < store->beez_for;
	}
	else
	{
		BeezParam = 0;
	}
	
//	printf( "%f %f %f %f  %f %f %f\n", store->beezpose.Rot[0], 
//		store->beezpose.Rot[1], store->beezpose.Rot[2], store->beezpose.Rot[3],
//		store->beezpose.Pos[0], store->beezpose.Pos[1], store->beezpose.Pos[2] );

	double LastSpinRotation = SpinRotationDraw;
	double Now = OGGetAbsoluteTime();
	double SpinRotation = (store->spinmode?(framecount/10.):((Now - StartTime)*10)) * store->spinn[0] / 100.;
	SpinRotationDraw = fmod( SpinRotation, 1 );

	double SpinSpeed = (store->spinmode?(.1*cnovrstate->fTargetFPS):(10)) * store->spinn[0] / 100.;
	double DistanceToHit = 1.-SpinRotationDraw;
	double TimeToNextHit = DistanceToHit/SpinSpeed;
	double NewTimeOfNextHit = TimeToNextHit + Now;
	
	if( NewTimeOfNextHit > TimeOfNextHit + 0.1 )
	{
		TimeOfLastHit = TimeOfNextHit;
	}
	TimeOfNextHit = NewTimeOfNextHit;
	
	static int doot_suprress;
	//printf( "%f %f\n", TimeToNextHit, doot_at );
	if( TimeToNextHit < .03 && doot_at <= 0 && store->do_doot )
	{
		doot_at = OGGetAbsoluteTime() + TimeToNextHit;
	}
	
//	printf( "%f %f %f %f\n", SpinSpeed, SpinRotationDraw, OGGetAbsoluteTime(), TimeOfNextHit );
//	if( SpinRotationDraw < LastSpinRotation ) doot_at = 660;

	if( delay > 0 )
		OGUSleep( delay );
}

void AudioPlaybackFunction( void * tag, void * opaquev )
{
	cnovr_audiodataset * data = (cnovr_audiodataset *)opaquev;
	data->cb_no++;

	double Now = OGGetAbsoluteTime();

	int i;
	int16_t * frames = data->frames;

	double DootStart = doot_at;
	double DootEnd = DootStart + 0.02;
	static int DootedFrames;
	if( DootStart > 0 )
	{
		for( i = 0; i < data->numframes; i++ )
		{
			double FrameTime = Now + i/48000.;
			
			if( FrameTime >= DootStart && FrameTime <= DootEnd )
			{
				float s = sin( (DootedFrames++) / 48000. * 660 * 6.28318 );
				frames[i*2+0] = s * 5000;
				frames[i*2+1] = s * 5000;
			}
			if( FrameTime >= DootEnd )
			{
				doot_at = 0;
				DootedFrames = 0;
				break;
			}
		}
	}
}

void * TimingCheckThread(void*v)
{
	//double TimeOfNextHit;
	//double TimeOfLastHit;
	//sprintf( DootTimingText, "Press A to sync" );
	uint64_t hleft  = CNOVRFocusGetVRActionHandleFromConrollerAndCtrlA( 1, 1 );
	uint64_t hright = CNOVRFocusGetVRActionHandleFromConrollerAndCtrlA( 2, 1 );

	#define CLICK_KEEP 16
	double ClickTimes[CLICK_KEEP];
	int ClickHead;

	int wasdown = 0;
	while(!shutting_down)
	{
		int abtn = CNOVRGetDigitalActionData( hleft ) | CNOVRGetDigitalActionData( hright );

		if( abtn && !wasdown )
		{
			double Now = OGGetAbsoluteTime();
			double NextD = Now - TimeOfLastHit;
			double LastD = Now - TimeOfNextHit;
			double Closest;
			if( -NextD > LastD )
			{
				Closest = NextD;
			}
			else
			{
				Closest = LastD;
			}
			//printf( "%f %f %f\n", LastD, NextD, Closest );
			ClickTimes[ClickHead] = Closest;
			ClickHead = (ClickHead + 1) % CLICK_KEEP;
			
			int i;
			double Average = 0.0;
			for( i = 0; i < CLICK_KEEP; i++ )
			{
				Average += ClickTimes[i];
			}
			Average /= CLICK_KEEP;
			double stddev = 0.0;
			for( i = 0; i < CLICK_KEEP; i++ )
			{
				stddev += ( ClickTimes[i] - Average )*( ClickTimes[i] - Average );
				//printf( "\t%f - %f\n", ClickTimes[i], Average );
			}
			stddev /= CLICK_KEEP;
			stddev = sqrt( stddev );
			int NewHit = 0;
			double NewAvg = 0.;
			for( i = 0; i < CLICK_KEEP; i++ )
			{
				double diff = ClickTimes[i] - Average;
				double absdiff = (diff<0)?(-diff):diff;
				if( absdiff < stddev )
				{
					NewHit++;
					NewAvg += ClickTimes[i];
				}
			}
			NewAvg /= NewHit;
			printf( "%f %f %f %d\n", Average, NewAvg, stddev, NewHit );
			sprintf( DootTimingText, "AVG:%4.2f STD:%4.2f HIT:%d\n", NewAvg*1000., stddev*1000., NewHit );
		}
		
		wasdown = abtn;
		
		OGUSleep(1000);
	}
}

static void testworld_scene_setup( void * tag, void * opaquev )
{
	printf( "+++ Test World scene setup\n" );
	int i;
	shader = CNOVRShaderCreate( "assets/example" );
	shaderBasic = CNOVRShaderCreate( "assets/basic" );
	colorsplotchshader = CNOVRShaderCreate( "colorsplotch" );
	spinningdiskshader = CNOVRShaderCreate( "spinningdisk" );
	beezshader = CNOVRShaderCreate( "beez" );
/*
	cnovr_simple_node * root = cnovrstate->pRootNode;
	node = CNOVRNodeCreateSimple( 1 );
	model = CNOVRModelCreate( 0, GL_TRIANGLES );
	CNOVRModelAppendCube( model, (cnovr_point3d){ 1.f, 1.f, 1.f }, 0, 0 );
	CNOVRNodeAddObject( node, shader );
	CNOVRNodeAddObject( node, model );
	CNOVRNodeAddObject( root, node );
*/
	canvas = CNOVRCanvasCreate( "TestWorldCanvas", 300, 320, 0 );
	UpdateMenu();

	srand( 0 );

	spinningdisk = CNOVRModelCreate( 0, GL_TRIANGLES );
	spinningdisk->pose = &store->spinningdiskpose;
	CNOVRModelAppendMesh( spinningdisk, 1, 1, 0, (cnovr_point3d){ 1.f, 1.f, 1.f }, 0, 0 );
	CNOVRModelAppendMesh( spinningdisk, 1, 1, 0, (cnovr_point3d){ -1.f, 1.f, 1.f }, 0, 0 );
	spinningdiskcapture.tag = 0;
	spinningdiskcapture.opaque = spinningdisk;
	spinningdiskcapture.cb = TestWorldFocusEvent;
	CNOVRModelSetInteractable( spinningdisk, &spinningdiskcapture );

	beez = CNOVRModelCreate( 0, GL_TRIANGLES );
	beez->pose = &store->beezpose;
	CNOVRModelAppendMesh( beez, 1, 1, 0, (cnovr_point3d){ 1.f, 1.f, 1.f }, 0, 0 );
	CNOVRModelAppendMesh( beez, 1, 1, 0, (cnovr_point3d){ -1.f, 1.f, 1.f }, 0, 0 );
	beezcapture.tag = 0;
	beezcapture.opaque = beez;
	beezcapture.cb = TestWorldFocusEvent;
	CNOVRModelSetInteractable( beez, &beezcapture );

	colorsplotch = CNOVRModelCreate( 0, GL_TRIANGLES );
	colorsplotch->pose = &store->colorsplotchpose;
	CNOVRModelAppendMesh( colorsplotch, 1, 1, 0, (cnovr_point3d){ 1.f, 1.f, 1.f }, 0, 0 );
	CNOVRModelAppendMesh( colorsplotch, 1, 1, 0, (cnovr_point3d){ -1.f, 1.f, 1.f }, 0, 0 );
	colorsplotchcapture.tag = 0;
	colorsplotchcapture.opaque = colorsplotch;
	colorsplotchcapture.cb = TestWorldFocusEvent;
	CNOVRModelSetInteractable( colorsplotch, &colorsplotchcapture );

#if 0
	isosphere = CNOVRModelCreate( 0, GL_TRIANGLES );
	isosphere->pose = &isospherepose;
	pose_make_identity( &isospherepose );
	CNOVRModelLoadFromFileAsync( isosphere, "isosphere.obj" );
	isocapture.tag = 0;
	isocapture.opaque = isosphere;
	isocapture.cb = CNOVRFocusDefaultFocusEvent;
	CNOVRModelSetInteractable( isosphere, &isocapture );
#endif

	texture = CNOVRTextureCreate( 0, 0, 0 ); //Set to all 0 to have the load control these details.
	CNOVRTextureLoadFileAsync( texture, "test.png" );

	UpdateFunction(0,0);
	CNOVRListAdd( cnovrLUpdate, 0, UpdateFunction );
	CNOVRListAdd( cnovrLRender2, 0, RenderFunction );
	CNOVRListAdd( cnovrLPrerender, 0, PrerenderFunction );
	CNOVRListAdd( cnovrLAudioPlay, 0, AudioPlaybackFunction );
	
	thddoot = OGCreateThread( TimingCheckThread, 0 );
	
	sprintf( DootTimingText, "Press A to sync" );

	
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
		pose_make_identity( &store->beezpose );
		pose_make_identity( &store->colorsplotchpose );
		DefaultMenu();
		store->initialized = 1;
	}
	
	identifier = strdup(identifier);
	CNOVRJobTack( cnovrQPrerender, testworld_scene_setup, 0, 0, 0 );
	printf( "=== Example start %s(%p) + %p %p\n", identifier, identifier );
	
	CNOVRCNFAInit();
}

void stop( const char * identifier )
{
	shutting_down = 1;
	OGJoinThread( thddoot );
	printf( "=== End Example stop\n" );
}


