/*
	A basic draggable camera as well as an advanced camera for live compositing
*/

#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovropenvr.h>
#include <cnovr.h>
#include <string.h>
#include <cnovrutil.h>
#include <chew.h>
#include <cnovrcanvas.h>

#define CNV4L2_NOSTAT
#include "cntools/cnv4l2/cnv4l2.c"

const char *  identifier;
cnovr_shader * shaderrendermodel;
cnovr_shader * shaderlines;
cnovr_shader * shaderblack;
cnovr_shader * previewcomposite;
cnovr_shader * previewbasic;
cnovr_shader * previewyuyv;
cnovr_model *  modelcameralines;
cnovr_model *  modelcamerasolid;
cnovrfocus_capture capture;

cnovr_canvas * canvascontrol;
cnovr_canvas * canvaspreview;
cnovr_canvas * canvasvideo;
cnovr_rf_buffer * previewtarget[3]; //Background, foreground, and final

cnv4l2 * v4l2interface;

struct camerastore_t
{
	int initialized;
	cnovr_pose  posecamera;
	int use_paired_object;
	char paired_object_serial_number[64];
	cnovr_pose  paired_relative_offset_pose;
} * store;

//Possible identifier options... "advanced,preview"
int advanced_view;
int preview_view;
float fPreviewForegroundSplitDistance = 10;
int previewW;
int previewH;

int videoW = 1920;
int videoH = 1080;

int pboid;
void * mapptr;
uint8_t * lastdat;
int quit = 0;
int framegrabbed;
og_thread_t videodatathread;
og_thread_t camerabusinessthread;

void init( const char * identifier )
{
}

void v4l2framecb( struct cnv4l2_t * match, uint8_t * payload, int payloadlen )
{
	//Stream new data to GPU
	//lastdat = payload;
	if( framegrabbed ) { ovrprintf( "Pending camera frame transfer.  Frame dropped\n" ); return; }
	if( mapptr ) memcpy( mapptr, payload, payloadlen );
	framegrabbed = 1;
}

void * videodatathreadfunction( void * v )
{
	if( !v4l2interface ) return 0;
	while( !quit )
	{
		int r = cnv4l2_read( v4l2interface, 100 );
		if( r < 0 )
		{
			printf( "SERIOUS CAMERA FAULT\n" );
			return 0;
		}
	}
	return 0;
}

void * camerabusinessthreadfunction( void * v )
{
	int did_set_physical_size;
	OGUSleep( 50000 );
	if( !canvascontrol ) return;

	while( !quit )
	{
		OGUSleep( 50000 );
		if( !did_set_physical_size )
		{
			CNOVRCanvasSetPhysicalSize( canvascontrol, canvascontrol->w/(float)canvascontrol->h, 1.0 );
			did_set_physical_size = 1;
		}

		CNOVRCanvasClearFrame( canvascontrol );
		static int frameno;
		frameno++;
		CNOVRCanvasSetLineWidth( canvascontrol, 1 );
		CNOVRCanvasDrawText( canvascontrol, 2, 2, trprintf( "Hello %d", frameno ), 3 );
		canvascontrol->color = 0xffffffff;
		CNOVRCanvasSwapBuffers( canvascontrol );

	}
	return 0;
}

void PrerenderFunction( void * tag, void * opaquev )
{
	static int did_set_aspect_ratio;

	if( v4l2interface )
	{
		uint32_t textureid;
		if( !canvasvideo || !canvasvideo->model->pTextures[0] ||
			!(textureid = canvasvideo->model->pTextures[0]->nTextureId) ) return;

		glBindTexture( GL_TEXTURE_2D, textureid );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );

		if( framegrabbed )
		{
			if( !did_set_aspect_ratio )
			{
				did_set_aspect_ratio = 1;
				CNOVRCanvasSetPhysicalSize( canvasvideo, videoW/(float)videoH, 1.0 );
			}
			if( !pboid )
			{
				glGenBuffers( 1, &pboid );
				glBindBuffer( GL_PIXEL_UNPACK_BUFFER, pboid );
				glBufferData(GL_PIXEL_UNPACK_BUFFER, videoW*videoH*2, NULL, GL_STREAM_DRAW);
				mapptr = glMapBufferRange( GL_PIXEL_UNPACK_BUFFER, 0, videoW*videoH*4/2, GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_BUFFER_BIT);
				glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
			}
			glBindBuffer( GL_PIXEL_UNPACK_BUFFER, pboid ); 
			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER );
			glBindTexture( GL_TEXTURE_2D, textureid );
			glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, videoW/2, videoH, GL_RGBA, GL_UNSIGNED_BYTE, 0);
			glBindTexture( GL_TEXTURE_2D, 0 );
			mapptr = glMapBufferRange( GL_PIXEL_UNPACK_BUFFER, 0, videoW*videoH*4/2, GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_BUFFER_BIT);
			glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 ); 
			framegrabbed = 0;
		}
	}
}

static void AdvancedPreviewRender( void * tag, void * opaquev )
{
	if( cnovrstate->iPreviewHeight == 0 || cnovrstate->iPreviewWidth == 0 ) return;
	if( advanced_view )
	{
		if( cnovrstate->iPreviewHeight != previewH || cnovrstate->iPreviewWidth != previewW || !previewtarget[0] || !previewtarget[1] || !previewtarget[2] )
		{
			previewW = cnovrstate->iPreviewWidth;
			previewH = cnovrstate->iPreviewHeight;
			if( previewtarget[0] ) CNOVRDelete( previewtarget[0] );
			if( previewtarget[1] ) CNOVRDelete( previewtarget[1] );
			if( previewtarget[2] ) CNOVRDelete( previewtarget[2] );
			previewtarget[0] = CNOVRRFBufferCreate( previewW, previewH, cnovrstate->multisample );
			previewtarget[1] = CNOVRRFBufferCreate( previewW, previewH, cnovrstate->multisample );
			previewtarget[2] = CNOVRRFBufferCreate( previewW, previewH, 0 );
			CNOVRCanvasSetPhysicalSize( canvaspreview, previewW/(float)previewH, -1.0 );
			CNOVRCanvasYFlip( canvaspreview, 1 );
		}

		//Find cutting distance.
		cnovr_point3d fvcamdist;
		apply_pose_to_point(fvcamdist, &cnovrstate->pPreviewPose, cnovrstate->pTrackedPoses[0].Pos);
		fPreviewForegroundSplitDistance = -fvcamdist[2];

		int width = cnovrstate->iRTWidth = previewW;
		int height = cnovrstate->iRTHeight = previewH;
		cnovrstate->eyeTarget = 2;
		matrix44identity( cnovrstate->mModel );
		pose_to_matrix44( cnovrstate->mView, &cnovrstate->pPreviewPose );
		int i;
		float distancecut = fPreviewForegroundSplitDistance;
		for( i = 0; i < 2; i++ )
		{
			CNOVRFBufferActivate( previewtarget[i] );
			matrix44perspective( cnovrstate->mPerspective, cnovrstate->fPreviewFOV, width/(float)height, i?cnovrstate->fNear:distancecut, i?distancecut:cnovrstate->fFar );
			glViewport(0, 0, width, height );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
			CNOVRListCall( cnovrLRender0, 0, 0); 
			CNOVRListCall( cnovrLRender1, 0, 0); 
			CNOVRListCall( cnovrLRender2, 0, 0); 
			CNOVRListCall( cnovrLRender3, 0, 0); 
			CNOVRListCall( cnovrLRender4, 0, 0); 
			CNOVRFBufferBlitResolve( previewtarget[i] );
		}
		glDisable( GL_DEPTH_TEST );

		CNOVRFBufferActivate( previewtarget[2] );
	//	glClearColor( 1., 0., 0., 1. );
		glViewport(0, 0, width, height );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		glEnable( GL_TEXTURE_2D );
		glActiveTextureCHEW( GL_TEXTURE0 + 0 );
		glBindTexture( GL_TEXTURE_2D, previewtarget[0]->nResolveTextureId );
		glActiveTextureCHEW( GL_TEXTURE0 + 1 );
		glBindTexture( GL_TEXTURE_2D, previewtarget[1]->nResolveTextureId );
		CNOVRRender( previewcomposite );
		CNOVRRender( cnovrstate->fullscreengeo );
		CNOVRFBufferBlitResolve( previewtarget[2] );

		CNOVRRender( previewbasic );
		glEnable( GL_TEXTURE_2D );
		glActiveTextureCHEW( GL_TEXTURE0 + 0 );
		glBindTexture( GL_TEXTURE_2D, previewtarget[2]->nResolveTextureId );
		CNOVRRender( cnovrstate->fullscreengeo );

		//glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST );
	}
	else
	{
		int width = cnovrstate->iPreviewWidth;
		int height = cnovrstate->iPreviewHeight;
		cnovrstate->eyeTarget = 2;
		matrix44identity( cnovrstate->mModel );
		pose_to_matrix44( cnovrstate->mView, &cnovrstate->pPreviewPose );
		matrix44perspective( cnovrstate->mPerspective, cnovrstate->fPreviewFOV, width/(float)height, cnovrstate->fNear, cnovrstate->fFar );
		glViewport(0, 0, width, height );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		CNOVRListCall( cnovrLRender0, 0, 0); 
		CNOVRListCall( cnovrLRender1, 0, 0); 
		CNOVRListCall( cnovrLRender2, 0, 0); 
		CNOVRListCall( cnovrLRender3, 0, 0); 
		CNOVRListCall( cnovrLRender4, 0, 0); 
	}
}

static void RenderFunction( void * tag, void * opaquev )
{
	int i;
	CNOVRRender( shaderlines );
	CNOVRModelRenderWithPose( modelcameralines, &store->posecamera );
	CNOVRRender( shaderrendermodel );
	if( canvascontrol ) CNOVRRender( canvascontrol );
	if( canvaspreview && previewtarget[2] )
	{
		if( canvaspreview->model )
			canvaspreview->model->pTextures[0]->nTextureId = previewtarget[2]->nResolveTextureId;
		CNOVRRender( canvaspreview->model );
	}
	if( canvasvideo )
	{
		//CNOVRRender( previewyuyv );
		CNOVRRender( canvasvideo );
	}
}

static void UpdateCamera()
{
	cnovr_pose pin;
	cnovr_pose * posecam = &store->posecamera;
	memcpy( &pin, posecam, sizeof( cnovr_pose ) );
	pin.Scale = 1;
	cnovr_pose pinvert;
	pose_invert( &pinvert, &pin );
	pinvert.Scale = 1;
	memcpy( &cnovrstate->pPreviewPose, &pinvert, sizeof( cnovr_pose ) );
	if( canvascontrol )
	{
		const cnovr_pose relative_pose_control = { .Pos = { 0 ,.6 ,0 }, .Rot = { 1, 0, 0, 0 }, .Scale = 1 };
		apply_pose_to_pose( canvascontrol->pose, posecam, &relative_pose_control );
		canvascontrol->pose->Scale *= .5;
	}
	if( canvasvideo )
	{
		const cnovr_pose relative_pose_video = { .Pos = { .5 ,0 ,0 }, .Rot = { 1, 0, 0, 0 }, .Scale = 1 };
		apply_pose_to_pose( canvasvideo->pose, posecam, &relative_pose_video );
		canvasvideo->pose->Scale *= .5;
	}
	if( canvaspreview ) 
	{
		const cnovr_pose relative_pose_preview = { .Pos = { -.5 ,0 ,0 }, .Rot = { 0, 0, 0, 1 }, .Scale = 1 };
		apply_pose_to_pose( canvaspreview->pose, posecam, &relative_pose_preview );
		canvaspreview->pose->Scale *= .5;
	}
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

	CNOVRGeneralHandleFocusEvent( modelcamerasolid->focuscontrol, modelcamerasolid->pose, prop, event, buttoninfo, CTRLA_PINCHBTN );

/*
	CNOVRF_IN = 0,
	CNOVRF_OUT,
	CNOVRF_DOWNFOCUS,
	CNOVRF_DOWNNOFOCUS,
	CNOVRF_UPFOCUS,
	CNOVRF_UPNOFOCUS,
	CNOVRF_MOTION,
	CNOVRF_DRAG,
	CNOVRF_ACQUIREDFOCUS,
	CNOVRF_LOSTFOCUS,
	CNOVRF_MAX_EVENTS,
*/
	if( event == CNOVRF_DRAG )
		UpdateCamera();

	return 0;
}


static void example_scene_setup( void * tag, void * opaquev )
{
	shaderrendermodel = CNOVRShaderCreate( "rendermodel" );
	shaderlines = CNOVRShaderCreate( "assets/basic" );
	shaderblack = CNOVRShaderCreate( "assets/black" );
	previewcomposite = CNOVRShaderCreate( "modules/camera/previewwindow" );
	previewbasic = CNOVRShaderCreate( "previewbasic" );

	modelcameralines = CNOVRModelCreate( 0, GL_LINES );
	modelcamerasolid = CNOVRModelCreate( 0, GL_TRIANGLES );
	modelcameralines->pose = &store->posecamera;
	modelcamerasolid->pose = &store->posecamera;
	CNOVRModelLoadFromFileAsync( modelcameralines, "modules/camera/camera.obj:lineify" );
	CNOVRModelLoadFromFileAsync( modelcamerasolid, "modules/camera/camera.obj" );
	CNOVRListAdd( cnovrLRender2, 0, RenderFunction );
	CNOVRListAdd( cnovrLPrerender, 0, PrerenderFunction );
	CNOVRListAdd( cnovrLPreviewRender, 0, AdvancedPreviewRender );


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

	if( advanced_view )
	{
		canvascontrol = CNOVRCanvasCreate( "VideoSetupControl", 96, 64 );
		canvasvideo = CNOVRCanvasCreate( "VideoSetupVideoView", videoW/2, videoH );
		CNOVRCanvasSwapBuffers( canvasvideo );
		canvasvideo->overrideshd = CNOVRShaderCreate( "modules/camera/previewyuyv" );
	}

	if( preview_view )
	{
		canvaspreview = CNOVRCanvasCreate( "PreviewCameraView", 0, 0 );
	}

	if( advanced_view )
	{
		camerabusinessthread = OGCreateThread( camerabusinessthreadfunction, 0 );
	}
	printf( "Is setup\n" );
}

const char * usecamtex = 0;

void camcnv4l2enumcb( void * opaque, const char * dev, const char * name, const char * bus )
{
	if( strstr( name, "MiraBox" ) != 0 ) usecamtex = strdup( dev );
}


void start( const char * identifier )
{
	store = CNOVRNamedPtrData( "camerastore", "camerastore", sizeof( *store ) + 128 );
	printf( "=== Initializing %p\n", store );

	advanced_view = strstr( identifier, "advanced" )?1:0;
	preview_view = strstr( identifier, "preview" )?1:0;

	if( advanced_view )
	{
		cnv4l2_enumerate( 0, camcnv4l2enumcb );
	}
	if( usecamtex )
	{
		v4l2interface = cnv4l2_open( usecamtex, videoW, videoH, CNV4L2_YUYV, CNV4L2_MMAP, v4l2framecb );
		printf( "Opening cam interface %s got %p\n", usecamtex, v4l2interface );
		if( v4l2interface )
		{
			cnv4l2_set_framerate( v4l2interface, 1, 60 );
			cnv4l2_start_capture( v4l2interface );
			videodatathread = OGCreateThread( videodatathreadfunction, 0 );
		}
	}

	//cnovrstate->fPreviewFOV = 45;

	identifier = strdup(identifier);
	CNOVRJobTack( cnovrQPrerender, example_scene_setup, 0, 0, 0 );
	printf( "=== Example start %s(%p) + %p %p\n", identifier, identifier );

}

void stop( const char * identifier )
{
	quit = 1;
	if( v4l2interface )
	{
		cnv4l2_close( v4l2interface );
	}
	OGJoinThread( videodatathread );
	OGJoinThread( camerabusinessthread );

	printf( "=== End Example stop\n" );
}


