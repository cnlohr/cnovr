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
cnovr_point3d  isospheremotionlinear;
cnovr_aamag  isospheremotionrotation;
cnovrfocus_capture isocapture;

double isospherelife;

cnovr_model * playarea;
cnovr_pose    playareapose;
struct ovrballstore_t
{
	int i;
} * store;

int shutting_down;

cnovr_canvas * canvas;

void ResetIsosphere()
{
	isospherelife = 40;
	pose_make_identity( &isospherepose );
	isospherepose.Pos[1] = 1;
	isospherepose.Pos[2] = 0;
	isospherepose.Scale = .1;

	isospheremotionlinear[0] = 0;
	isospheremotionlinear[1] = 0;
	isospheremotionlinear[2] = 2;

	isospheremotionrotation[0] = 0;
	isospheremotionrotation[1] = 10;
	isospheremotionrotation[2] = 0;
}

int CheckCollideBallWithMesh( cnovr_model * m, int mesh, cnovr_pose * modelpose, cnovr_pose * modelposelast,
	float dtimelast /*Usually fixed, used to compute paddle speed*/,
	float dtimeframe /*Actual delta time*/, cnovr_point3d modeltarget )
{
	cnovr_collide_results res;
	cnovr_point3d start, direction;
	scale3d( start, isospheremotionlinear, dtimeframe );
	add3d( start, start, isospherepose.Pos ); //This is time-corrected for where the object pose would be.

	cnovr_pose invertedxform;
	pose_invert( &invertedxform, modelpose );
	apply_pose_to_point( start, &invertedxform, start);


	sub3d( direction, modeltarget, start );
	//This is a tad bit concerning.  We say we go from where we are now toward the center of the object.
	//Maybe this is a cause of some of our issues?  It should probably be normal to the plane of motion?

	normalize3d( direction, direction );

	res.t = 1.; //Must actually impact.
	m->iCollideMesh = mesh;
	int r = CNOVRModelCollide( m, start, direction, &res );
//	if( r == 0 && res.t >= 0  && res.t < 1. ) { printf( "%f: %f %f %f\n", res.t, PFTHREE( res.collidens ) ); }
	if( r != 0 || res.t > 0 ) return -1;


	//COLLISION

	//Here we are in a neat position. We have a collision!  Time to bounce!
	cnovr_point3d normal; //In world space
	quatrotatevector(normal, modelpose->Rot, res.collidens);
	printf( "Collided depth: %f   %f %f %f  TIME: %f\n", res.t, PFTHREE( res.collidens ), dtimeframe );
	//First, "fix up" location to make sure we don't overpenetrate.
	cnovr_point3d fixup;
	scale3d( fixup, normal, (.001)*1./*arbitrary*/ );
	add3d( isospherepose.Pos, isospherepose.Pos, fixup );

	//Need to compute relative paddle motion at location of impact.
	cnovr_point3d point_of_impact_paddle_space;
	copy3d( point_of_impact_paddle_space, res.collidepos );
	cnovr_point3d paddle_motion_at_impact;  //In world space.
	{
		cnovr_point3d motionlocalA, motionlocalB;
		apply_pose_to_point( motionlocalA, modelpose, point_of_impact_paddle_space );
		apply_pose_to_point( motionlocalB, modelposelast, point_of_impact_paddle_space );
		sub3d( paddle_motion_at_impact, motionlocalA, motionlocalB );
		scale3d( paddle_motion_at_impact, paddle_motion_at_impact, 1./dtimelast );
	}

	cnovr_point3d relative_motion_vector; //In world space
	printf( "RMV IN: %f %f %f   %f %f %f\n", PFTHREE( isospheremotionlinear ), PFTHREE( paddle_motion_at_impact ) );

	//Tricky: What if we are swatting in the direction the ball is already going?
	cnovr_point3d reflection, tmp;
	sub3d( relative_motion_vector, isospheremotionlinear, paddle_motion_at_impact );
	if( dot3d( relative_motion_vector, normal ) < 0 )
	{
		//If it's the opposite way, we reflect.
		printf( "+RMV: %f %f %f / NORMAL: %f %f %f\n", PFTHREE( relative_motion_vector ), PFTHREE( normal ) );
		//If we have paddle motion and normal in world space, what more do we need?
		//REFLECT: r=d-2(d⋅n)n; d = incoming relative motion vector. (relative_motion_vector)
		float dotr = 2*dot3d( relative_motion_vector, normal );
		scale3d( tmp, normal, dotr );
		sub3d( reflection, relative_motion_vector, tmp );
	}
	else
	{
		//Otherwise, we mix.
		printf( "MIXING: %f %f %f  with %f %f %f\n", PFTHREE( relative_motion_vector ), PFTHREE( isospheremotionlinear ) );
		//add3d( reflection, paddle_motion_at_impact, isospheremotionlinear );
		//scale3d( reflection, reflection, 0.5 );
		copy3d( reflection, isospheremotionlinear ); // Turn into a no-op
	}

	//Also fixup
	scale3d( fixup, paddle_motion_at_impact, dtimelast );
	add3d( isospherepose.Pos, isospherepose.Pos, fixup );

	scale3d( isospheremotionlinear, reflection, 1.0 );
	printf( "%f %f %f   %f %f %f   %f %f %f   %f\n", PFTHREE( reflection ), PFTHREE( isospheremotionlinear ), PFTHREE( relative_motion_vector ), res.t );


	//scale3d( fixup, fixup, 10. );
	//add3d( isospheremotionlinear, fixup, isospheremotionlinear );

	return 0;
}



void init( const char * identifier )
{
	ovrprintf( "Example init %s\n", identifier );
}

void UpdateFunction( void * tag, void * opaquev )
{
	static double start;
	static double last;
	double now = OGGetAbsoluteTime();
	if( start < 1 ) { start = now; last = now; return; }
	double runtime = now - start;
	double deltatime = now - last;
	last = now;

	//Handle Isosphere Motion Update
	isospherelife -= deltatime;
	if( isospherelife < 0.0 ) ResetIsosphere();
	cnovr_point3d delta_motion;
	scale3d( delta_motion, isospheremotionlinear, deltatime );
	add3d( isospherepose.Pos, isospherepose.Pos, delta_motion );
	cnovr_aamag aam;
	cnovr_quat qmotion;
	scale3d( aam, isospheremotionrotation, deltatime );
	quatfromaxisanglemag( qmotion, aam );
	quatrotateabout( isospherepose.Rot, isospherepose.Rot, qmotion );


	int i;
	VRActionHandle_t tip1 = CNOVRFocusGetVRActionHandleFromConrollerAndCtrlA( 1, CTRLA_TIP );
	VRActionHandle_t tip2 = CNOVRFocusGetVRActionHandleFromConrollerAndCtrlA( 2, CTRLA_TIP );

	cnovr_pose poselast1;
	cnovr_pose poselast2;
	int did_hit_this_frame = 0;
	for( i = 0 ; i < TIMESLOTS; i++ )
	{
//	EVRInputError (OPENVR_FNTABLE_CALLTYPE *GetPoseActionDataRelativeToNow)(VRActionHandle_t action, ETrackingUniverseOrigin eOrigin, 
	//float fPredictedSecondsFromNow, struct InputPoseActionData_t * pActionData, uint32_t unActionDataSize, VRInputValueHandle_t ulRestrictToDevice);
	//	printf( "%llx %llx\n", tip1, tip2 );
		InputPoseActionData_t pad1, pad2;
		float now = -.02 + i * .0019; //We don't do future predict XXX TODO TWEAK ME
		EVRInputError e1 = cnovrstate->oInput->GetPoseActionDataRelativeToNow( tip1, ETrackingUniverseOrigin_TrackingUniverseStanding, now, &pad1, sizeof( pad1 ), 0 );
		EVRInputError e2 = cnovrstate->oInput->GetPoseActionDataRelativeToNow( tip2, ETrackingUniverseOrigin_TrackingUniverseStanding, now, &pad2, sizeof( pad2 ), 0 );

		cnovr_pose pose1, pose2;
		CNOVRPoseFromHMDMatrix( &pose1, &pad1.pose.mDeviceToAbsoluteTracking );
		CNOVRPoseFromHMDMatrix( &pose2, &pad2.pose.mDeviceToAbsoluteTracking );
		//printf( "%f %f %f\n", PFTHREE( pose2.Pos ) );

		apply_pose_to_pose( &paddlepose1[i], &pose1, &paddetransform );
		apply_pose_to_pose( &paddlepose2[i], &pose2, &paddetransform );

		if( now <= 0 && i > 0 && !did_hit_this_frame )
		{
			cnovr_point3d target = { 0, -.4, 0 };  //Kludge -> Target center of mesh.
			int r1 = CheckCollideBallWithMesh( paddle, 0, &paddlepose1[i], &poselast1, .0049, now, target ); 
			int r2 = CheckCollideBallWithMesh( paddle, 0, &paddlepose2[i], &poselast2, .0049, now, target );
			if( r1 == 0 || r2 == 0 ) did_hit_this_frame = 1;
		}
		memcpy( &poselast1, &paddlepose1[i], sizeof( cnovr_pose ) );
		memcpy( &poselast2, &paddlepose2[i], sizeof( cnovr_pose ) );
	}


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

//If you want to lock to "Display" paddle, do this.
//	apply_pose_to_pose( &paddlepose1, CNOVRFocusGetTipPose(1), &paddetransform );
//	apply_pose_to_pose( &paddlepose2, CNOVRFocusGetTipPose(2), &paddetransform );
}

void RenderFunction( void * tag, void * opaquev )
{
	int i;
	CNOVRRender( shaderBasic );
	CNOVRRender( playarea );
	CNOVRRender( isosphere );

	paddle->iRenderMesh = 1;
	for( i = 0 ; i < TIMESLOTS; i++ )
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

	playarea = CNOVRModelCreate( 0, GL_TRIANGLES );
	playarea->pose = &playareapose;
	pose_make_identity( &playareapose );
	CNOVRModelLoadFromFileAsync( playarea, "playarea.obj" );

	isosphere = CNOVRModelCreate( 0, GL_TRIANGLES );
	isosphere->pose = &isospherepose;
	CNOVRModelLoadFromFileAsync( isosphere, "isosphere.obj" );
	ResetIsosphere();

	paddle = CNOVRModelCreate( 0, GL_LINE_STRIP );
	paddle->pose = 0;
	CNOVRModelLoadFromFileAsync( paddle, "paddle.obj" );
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


