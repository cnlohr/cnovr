#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovropenvr.h>
#include <cnovrcanvas.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <stdlib.h>
#include <string.h>

const char * identifier;
cnovr_shader * shaderBasic;

#define TIMESLOTS 11
cnovr_model * paddle;
cnovr_pose    paddlepose1[TIMESLOTS];
cnovr_pose    paddlepose2[TIMESLOTS];
cnovr_pose    paddetransform;

cnovr_model * isosphere;
cnovr_pose    isospherepose;
cnovrfocus_capture isocapture;

struct ovrballstore_t
{
	int i;
} * store;

int shutting_down;

cnovr_canvas * canvas;

void init( const char * identifier )
{
	ovrprintf( "Example init %s\n", identifier );
}

void UpdateFunction( void * tag, void * opaquev )
{
	static double start;
	double now = OGGetAbsoluteTime();
	if( start < 1 ) start = now;

	CNOVRCanvasClearFrame( canvas );
	static int frameno;
	frameno++;
	CNOVRCanvasSetLineWidth( canvas, 2 );
	CNOVRCanvasDrawText( canvas, 10, 10, trprintf( "%d", frameno ), 8 );
//	CNOVRCanvasTackSegment( canvas, 10, 10, 100, 10 );
	CNOVRCanvasSwapBuffers( canvas );

	return;
}

void PrerenderFunction( void * tag, void * opaquev )
{
	VRActionHandle_t tip1 = CNOVRFocusGetVRActionHandleFromConrollerAndCtrlA( 1, CTRLA_TIP );
	VRActionHandle_t tip2 = CNOVRFocusGetVRActionHandleFromConrollerAndCtrlA( 2, CTRLA_TIP );

	int i;
	for( i = 0 ; i <TIMESLOTS; i++ )
	{
//	EVRInputError (OPENVR_FNTABLE_CALLTYPE *GetPoseActionDataRelativeToNow)(VRActionHandle_t action, ETrackingUniverseOrigin eOrigin, 
	//float fPredictedSecondsFromNow, struct InputPoseActionData_t * pActionData, uint32_t unActionDataSize, VRInputValueHandle_t ulRestrictToDevice);
	//	printf( "%llx %llx\n", tip1, tip2 );
		InputPoseActionData_t pad1, pad2;
		float now = -.1 + i * .02;
		EVRInputError e1 = cnovrstate->oInput->GetPoseActionDataRelativeToNow( tip1, ETrackingUniverseOrigin_TrackingUniverseStanding, now, &pad1, sizeof( pad1 ), 0 );
		EVRInputError e2 = cnovrstate->oInput->GetPoseActionDataRelativeToNow( tip2, ETrackingUniverseOrigin_TrackingUniverseStanding, now, &pad2, sizeof( pad2 ), 0 );

		cnovr_pose pose1, pose2;
		CNOVRPoseFromHMDMatrix( &pose1, &pad1.pose.mDeviceToAbsoluteTracking );
		CNOVRPoseFromHMDMatrix( &pose2, &pad2.pose.mDeviceToAbsoluteTracking );
		//printf( "%f %f %f\n", PFTHREE( pose2.Pos ) );

		apply_pose_to_pose( &paddlepose1[i], &pose1, &paddetransform );
		apply_pose_to_pose( &paddlepose2[i], &pose2, &paddetransform );
	}

//If you want to lock to "Display" paddle, do this.
//	apply_pose_to_pose( &paddlepose1, CNOVRFocusGetTipPose(1), &paddetransform );
//	apply_pose_to_pose( &paddlepose2, CNOVRFocusGetTipPose(2), &paddetransform );
}

void RenderFunction( void * tag, void * opaquev )
{
	int i;
	CNOVRRender( shaderBasic );
	CNOVRRender( isosphere );

	for( i = 0 ; i <TIMESLOTS; i++ )
	{
		CNOVRModelRenderWithPose( paddle, &paddlepose1[i] );
		CNOVRModelRenderWithPose( paddle, &paddlepose2[i] );
	}
	CNOVRRender( canvas );

}


static void example_scene_setup( void * tag, void * opaquev )
{
	printf( "+++ Example scene setup\n" );
	int i;
	shaderBasic = CNOVRShaderCreate( "assets/basic" );

	canvas = CNOVRCanvasCreate( "ExampleCanvas", 128, 96 );

	isosphere = CNOVRModelCreate( 0, GL_TRIANGLES );
	isosphere->pose = &isospherepose;
	pose_make_identity( &isospherepose );
	isospherepose.Scale = .1;
	CNOVRModelLoadFromFileAsync( isosphere, "isosphere.obj" );

	paddle = CNOVRModelCreate( 0, GL_TRIANGLES );
	paddle->pose = 0;
	CNOVRModelLoadFromFileAsync( paddle, "paddle.obj" );
	pose_make_identity( &paddlepose1 );
	pose_make_identity( &paddlepose2 );
	pose_make_identity( &paddetransform );

	cnovr_euler_angle eu = { 3.14159-.5, 0, 0 };
	quatfromeuler( paddetransform.Rot, eu );


	UpdateFunction(0,0);
	CNOVRListAdd( cnovrLUpdate, 0, UpdateFunction );
	CNOVRListAdd( cnovrLPrerender, 0, PrerenderFunction );
	CNOVRListAdd( cnovrLRender2, 0, RenderFunction );
	//CNOVRListAdd( cnovrLCollide, 0, CollideFunction );
	printf( "+++ Example scene setup complete\n" );
}


void start( const char * identifier )
{
	store = CNOVRNamedPtrData( "ovrballstore", 0, sizeof( *store ) + 1024 );
	printf( "=== Initializing %p\n", store );

	identifier = strdup(identifier);
	CNOVRJobTack( cnovrQPrerender, example_scene_setup, 0, 0, 0 );
	printf( "=== Example start %s(%p) + %p %p\n", identifier, identifier );
}

void stop( const char * identifier )
{
	shutting_down = 1;
	//OGCancelThread( thdmax );
	printf( "=== End Example stop\n" );
}


