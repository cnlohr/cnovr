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
cnovr_shader * shaderBlack;
og_thread_t thdmax;

#define TIMESLOTS 10
#define EXTRASLOTS 20
#define PARTICLES 256

cnovr_model * paddle;
cnovr_pose    paddlepose1[TIMESLOTS+EXTRASLOTS];
cnovr_pose    paddlepose2[TIMESLOTS+EXTRASLOTS];
cnovr_pose    paddetransform;

cnovr_model * isosphere;
cnovr_pose    isospherepose;
cnovr_point3d  isospheremotionlinear;
cnovr_aamag  isospheremotionrotation;
float isospherehitcooldown;
cnovrfocus_capture isocapture;

int sentinal1;
double isospherelife;
int sentinal2;

cnovr_model * playarea;
cnovr_model * playareacollide;
cnovr_pose    playareapose;
cnovr_pose   playareaposeepisilondown; //For pushing the triangles down a bit to unmask the lines.
cnovr_pose    boomroot; //Must be origin

cnovr_point3d roomoffset = { 0, 0, -1 };

cnovr_model * explosion_model;
cnovr_shader * explosion_shader;
float * explosion_points;
float * explosion_data;
float * explosion_color;
float * explosion_extradata;

void EmitParticle( float * pos, float size, float * dir, float * color, float lifetime )
{
	static int exphead;

	memcpy( explosion_points + exphead*3, pos, sizeof(float)*3 );
	memcpy( explosion_data + (exphead*4+1), dir, sizeof(float)*3 );
	*(explosion_data + (exphead*4+0)) = size;
	memcpy( explosion_color + exphead*4, color, sizeof(float)*4 );
	float * extra = (explosion_extradata + exphead*4);
	extra[0] = lifetime; //Current lifetime
	extra[1] = size;
	extra[2] = lifetime;
	extra[3] = 0;
	exphead++;
	if( exphead == PARTICLES ) exphead = 0;
}

void Boom( float * pos, int npart, float expand, float lifetime )
{
	int i;
	for( i = 0; i < npart; i++ )
	{
		cnovr_point3d dir;
		dir[0] = (rand()%102)-50.5;
		dir[1] = (rand()%102)-50.5;
		dir[2] = (rand()%102)-50.5;
		normalize3d( dir, dir );
		//^^^^ Ugh buggy compiler
		scale3d( dir, dir, expand );
		float color[4];
		color[0] = (rand()%100)+1;
		color[1] = (rand()%100)+1;
		color[2] = (rand()%100)+1;
		color[3] = 1;
		normalize3d( color, color );
		EmitParticle( pos, 20+(rand()%20), dir, color, lifetime );
	}
}


int cpupoints;
int playerpoints;
#define CPUEND -26
#define ACCELEND -12
#define PLAYEREND    3

struct ovrballstore_t
{
	int i;
} * store;

int shutting_down;

cnovr_canvas * canvas;

void ResetIsosphere()
{
	printf( "Reset Isosphere\n" );
	isospherelife = 40.0f;
	isospherehitcooldown = 40.0f;
	pose_make_identity( &isospherepose );
	isospherepose.Pos[1] = 1;
	add3d( isospherepose.Pos, isospherepose.Pos, roomoffset );
	isospherepose.Scale = .1;

	isospheremotionlinear[0] = 0;
	isospheremotionlinear[1] = 0;
	isospheremotionlinear[2] = 0;

	isospheremotionrotation[0] = 0;
	isospheremotionrotation[1] = 0;
	isospheremotionrotation[2] = 0;
}

int CheckCollideBallWithMesh( cnovr_model * m, int mesh, cnovr_pose * modelpose,
	cnovr_pose * modelposelast,
	float dtimelast /*Usually fixed, used to compute paddle speed*/,
	float dtimeframe /*Actual delta time*/, float * modeltarget, float damper, int customcollide )
{
	cnovr_collide_results res;
	cnovr_point3d start, direction;
	scale3d( start, isospheremotionlinear, dtimeframe );
	add3d( start, start, isospherepose.Pos ); //This is time-corrected for where the object pose would be.

	cnovr_pose invertedxform;
	pose_invert( &invertedxform, modelpose );
	apply_pose_to_point( start, &invertedxform, start);

	int invt = 0;

	if( modeltarget )
		sub3d( direction, modeltarget, start );
	else
	{
	//	normalize3d( direction, isospheremotionlinear );
	//	scale3d( direction, direction, -1 );
		cnovr_point3d localtarget = { 0, 1, 0 };
		sub3d( direction, localtarget, start );
		invt = 1;
	}
	//This is a tad bit concerning.  We say we go from where we are now toward the center of the object.
	//Maybe this is a cause of some of our issues?  It should probably be normal to the plane of motion?

	normalize3d( direction, direction );
	res.t = 1.; //Must actually impact.
	m->iCollideMesh = mesh;
	int r = CNOVRModelCollide( m, start, direction, &res, 0.1 );

//	printf( "%d %f\n", r, res.t );

	if( invt ) res.t = -res.t;
//	if( r == 0 && res.t >= 0  && res.t < 1. ) { printf( "%f: %f %f %f\n", res.t, PFTHREE( res.collidens ) ); }
	if( r < 0 || res.t > 0  ) return -1; //1dm is too big.
	if( res.t < -.5 ) return -1;
//	if( r>=0 ) printf( "RHITMARK %d %f\n", r, res.t );

	//COLLISION

	//Here we are in a neat position. We have a collision!  Time to bounce!
	cnovr_point3d normal; //In world space
	quatrotatevector(normal, modelpose->Rot, res.collidens);
	printf( "Collided depth: %f   %f %f %f  TIME: %f\n", res.t, PFTHREE( res.collidens ), dtimeframe );
	//First, "fix up" location to make sure we don't overpenetrate.
	cnovr_point3d fixup;
	scale3d( fixup, normal, -res.t );
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

	Boom( isospherepose.Pos,  3, .2, .5 );

	//Tricky: What if we are swatting in the direction the ball is already going?
	cnovr_point3d reflection, tmp;
	scale3d( paddle_motion_at_impact, paddle_motion_at_impact, 1 ); //Prentend racket mass high -> Would make this higher
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

	scale3d( isospheremotionlinear, reflection, damper );

	//Also fixup
	scale3d( fixup, reflection, dtimelast*5 );
	add3d( isospherepose.Pos, isospherepose.Pos, fixup );

	printf( "%f %f %f   %f %f %f   %f %f %f   %f\n", PFTHREE( reflection ), PFTHREE( isospheremotionlinear ), PFTHREE( relative_motion_vector ), res.t );


	//scale3d( fixup, fixup, 10. );
	//add3d( isospheremotionlinear, fixup, isospheremotionlinear );

	return 0;
}



void init( const char * identifier )
{
	ovrprintf( "Example init %s\n", identifier );
}

void * PhysicsThread( void * v )
{
//	cnovr_pose playareapose;
//	pose_make_identity( &playareapose );
	int racketslot = 0;
	VRActionHandle_t tip1 = CNOVRFocusGetVRActionHandleFromConrollerAndCtrlA( 1, CTRLA_TIP );
	VRActionHandle_t tip2 = CNOVRFocusGetVRActionHandleFromConrollerAndCtrlA( 2, CTRLA_TIP );

	cnovr_pose poselast1;
	cnovr_pose poselast2;

	int hitslot = 0;

	while( !shutting_down )
	{
		static double start;
		static double last;
		OGUSleep( 1000 );
		double now = OGGetAbsoluteTime();
		if( start < 1 ) { start = now; last = now; continue; }
		double runtime = now - start;
		float deltatime = (float)(now - last);
		last = now;

		isospherehitcooldown += deltatime;

		//Handle Isosphere Motion Update
		isospherelife -= deltatime;
		if( isospherelife < 0.0f ) { ResetIsosphere(); printf( "NEWLIFE %f\n", isospherelife ); }
		cnovr_point3d delta_motion;
		scale3d( delta_motion, isospheremotionlinear, deltatime );
		add3d( isospherepose.Pos, isospherepose.Pos, delta_motion );
		cnovr_aamag aam;
		cnovr_quat qmotion;
		scale3d( aam, isospheremotionrotation, deltatime );
		quatfromaxisanglemag( qmotion, aam );
		quatrotateabout( isospherepose.Rot, isospherepose.Rot, qmotion );

		int did_hit_this_frame = 0;

		if( !cnovrstate->oInput ) continue;
		InputPoseActionData_t pad1, pad2;
		float tnow = -0.001;
		EVRInputError e1 = cnovrstate->oInput->GetPoseActionDataRelativeToNow( tip1, ETrackingUniverseOrigin_TrackingUniverseStanding, tnow, &pad1, sizeof( pad1 ), 0 );
		EVRInputError e2 = cnovrstate->oInput->GetPoseActionDataRelativeToNow( tip2, ETrackingUniverseOrigin_TrackingUniverseStanding, tnow, &pad2, sizeof( pad2 ), 0 );

		cnovr_pose pose1, pose2;
		CNOVRPoseFromHMDMatrix( &pose1, &pad1.pose.mDeviceToAbsoluteTracking );
		CNOVRPoseFromHMDMatrix( &pose2, &pad2.pose.mDeviceToAbsoluteTracking );
		//printf( "%f %f %f\n", PFTHREE( pose2.Pos ) );

		apply_pose_to_pose( &paddlepose1[racketslot], &pose1, &paddetransform );
		apply_pose_to_pose( &paddlepose2[racketslot], &pose2, &paddetransform );

		if( isospherehitcooldown > .05f )
		{
			cnovr_point3d target = { 0, -.4f, 0 };  //Kludge -> Target center of mesh.
			int r1 = CheckCollideBallWithMesh( paddle, 1, &paddlepose1[racketslot], &poselast1, 
				deltatime, tnow, target, 1.5f, 0 ); 
			int r2 = CheckCollideBallWithMesh( paddle, 1, &paddlepose2[racketslot], &poselast2, 
				deltatime, tnow, target, 1.5f, 0 );
			int r3 = CheckCollideBallWithMesh( playareacollide, 0, &playareapose, &playareapose,
				deltatime, tnow, 0, 0.9f, 1 );
			if( r1 == 0 || r2 == 0 || r3 == 0 ) {
				if( r1 == 0 || r2 == 0 )
				{
					memcpy( &paddlepose1[hitslot+TIMESLOTS],  &paddlepose1[racketslot], sizeof( cnovr_pose ) );
					memcpy( &paddlepose2[hitslot+TIMESLOTS],  &paddlepose2[racketslot], sizeof( cnovr_pose ) );
					isospherehitcooldown = 0;
					printf( "Writing into slot %d\n", hitslot );
					hitslot++;
					if( hitslot == EXTRASLOTS ) hitslot = 0;
				}
			}
		}

		memcpy( &poselast1, &paddlepose1[racketslot], sizeof( cnovr_pose ) );
		memcpy( &poselast2, &paddlepose2[racketslot], sizeof( cnovr_pose ) );

		//Check end stops
		if( isospherepose.Pos[2] < CPUEND ) { playerpoints++; Boom( isospherepose.Pos, 100, 2.0, 4.0 ); ResetIsosphere(); printf( "cpuend\n" ); }
		if( isospherepose.Pos[2] > PLAYEREND ) { cpupoints++; ResetIsosphere(); printf( "playerend\n" ); }

		//Accel out the CPU end.
		if( isospherepose.Pos[2] < ACCELEND ) { scale3d( isospheremotionlinear, isospheremotionlinear, 1.005 ); }
		racketslot++;
		if( racketslot == TIMESLOTS ) racketslot = 0;
	}

	return 0;
}

float fpstime = 1.;

void UpdateFunction( void * tag, void * opaquev )
{
	static double start;
	static double last;
	static double lastframetime;
	double now = OGGetAbsoluteTime();
	if( start < 1 ) { start = now; last = now; return; }
	double runtime = now - start;
	float deltatime = (float)(now - last);
	last = now;

	CNOVRCanvasClearFrame( canvas );
	static int frameno;
	frameno++;
	CNOVRCanvasSetLineWidth( canvas, 1 );
	if( (frameno % 10) == 0 )
	{
		fpstime = (float)(now-lastframetime);
		lastframetime = now;
	}
	CNOVRCanvasDrawText( canvas, 2, 2, trprintf( "%3.fFPS\n CPU YOU\n%4d%4d", 10./fpstime, cpupoints, playerpoints ), 3 );
//	CNOVRCanvasTackSegment( canvas, 10, 10, 100, 10 );
	CNOVRCanvasSwapBuffers( canvas );

	int i;
	for( i = 0; i < PARTICLES; i++ )
	{
		float * pt = explosion_points + i*3;
		float * dat = explosion_data + i*4;
		float * col = explosion_color + i*4;
		float * ext = explosion_extradata + i*4;
 
		float life = ext[0];
		if( life > 0 )
		{
			life -= deltatime;
		}
		ext[0] = life;
		cnovr_point3d motion;
		scale3d( motion, dat+1, deltatime );
		add3d( pt, motion, pt );
		dat[0] = (life/ext[2])*ext[1];
		//printf( "%f/%f=%f   %f %f %f    %f    %f %f %f %f\n", life, ext[2], dat[0], PFTHREE( pt ), dat[0], PFFOUR(col) );
	}
	CNOVRVBOTaint( explosion_model->pGeos[0] );
	CNOVRVBOTaint( explosion_model->pGeos[1] );
	CNOVRVBOTaint( explosion_model->pGeos[2] );
	CNOVRVBOTaint( explosion_model->pGeos[3] );

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
	CNOVRRender( isosphere );

	//CNOVRRender( shaderBlack );
	//playareacollide->iRenderMesh = 0;
	//CNOVRRender( playareacollide );

	glDepthFunc( GL_LEQUAL );
	CNOVRRender( shaderBasic );
	playarea->iRenderMesh = 0;
	CNOVRRender( playarea );
	playarea->iRenderMesh = 2;
	CNOVRRender( playarea );



	paddle->iRenderMesh = 1;
	for( i = 0 ; i < TIMESLOTS /*+ EXTRASLOTS Not displaying these*/; i++ )
	{
		CNOVRModelRenderWithPose( paddle, &paddlepose1[i] );
		CNOVRModelRenderWithPose( paddle, &paddlepose2[i] );
	}

	CNOVRRender( canvas );

#define GL_VERTEX_PROGRAM_POINT_SIZE 0x8642
	glHint(GL_POINT_SMOOTH_HINT, GL_FASTEST);
	glEnable( GL_VERTEX_PROGRAM_POINT_SIZE );
//	glDisable(GL_POINT_SMOOTH);
	CNOVRRender( explosion_shader );
	CNOVRRender( explosion_model );
}


static void example_scene_setup( void * tag, void * opaquev )
{
	printf( "+++ Example scene setup\n" );
	int i;
	shaderBasic = CNOVRShaderCreate( "assets/basic" );
	shaderBlack = CNOVRShaderCreate( "assets/black" );

	canvas = CNOVRCanvasCreate( "ExampleCanvas", 96, 64 );

	playarea = CNOVRModelCreate( 0, GL_LINES );
	playarea->pose = &playareapose;
	pose_make_identity( &playareapose );
	add3d( playareapose.Pos, playareapose.Pos, roomoffset );
	CNOVRModelLoadFromFileAsync( playarea, "playarea.obj:lineify" );

	pose_make_identity( &playareaposeepisilondown );
	playareaposeepisilondown.Pos[1] -= .005;
	playareaposeepisilondown.Pos[2] = playareapose.Pos[2];
	playareacollide = CNOVRModelCreate( 0, GL_QUADS );
	playareacollide->pose = &playareaposeepisilondown;
	CNOVRModelLoadFromFileAsync( playareacollide, "playarea.obj" );

	isosphere = CNOVRModelCreate( 0, GL_LINES
 );
	isosphere->pose = &isospherepose;
	CNOVRModelLoadFromFileAsync( isosphere, "isosphere.obj:lineify" );
	ResetIsosphere();

	paddle = CNOVRModelCreate( 0, GL_LINE_STRIP );
	paddle->pose = 0;
	CNOVRModelLoadFromFileAsync( paddle, "paddle.obj" );
	pose_make_identity( &paddetransform );

	cnovr_euler_angle eu = { 3.14159-.5, 0, 0 };
	quatfromeuler( paddetransform.Rot, eu );

	explosion_shader = CNOVRShaderCreate( "ovrball/explosion" );
	explosion_model = CNOVRModelCreate( 0, GL_POINTS );
	CNOVRModelSetNumVBOsWithStrides( explosion_model, 4, 3, 4, 4, 4 );
	for( i = 0; i < PARTICLES; i++ )
	{
		float nil[4] = { 0 };
		CNOVRVBOTackv( explosion_model->pGeos[0], 3, nil );
		CNOVRVBOTackv( explosion_model->pGeos[1], 4, nil );
		CNOVRVBOTackv( explosion_model->pGeos[2], 4, nil );
		CNOVRVBOTackv( explosion_model->pGeos[3], 4, nil );
		CNOVRModelTackIndex( explosion_model, 1, i );
	}
	explosion_points = explosion_model->pGeos[0]->pVertices;
	explosion_data = explosion_model->pGeos[1]->pVertices;
	explosion_color = explosion_model->pGeos[2]->pVertices;
	explosion_extradata = explosion_model->pGeos[3]->pVertices;
	CNOVRVBOTaint( explosion_model->pGeos[0] );
	CNOVRVBOTaint( explosion_model->pGeos[1] );
	CNOVRVBOTaint( explosion_model->pGeos[2] );
	CNOVRVBOTaint( explosion_model->pGeos[3] );
	CNOVRModelTaintIndices( explosion_model );
	pose_make_identity( &boomroot );
	explosion_model->pose = &boomroot;


	UpdateFunction(0,0);
	CNOVRListAdd( cnovrLUpdate, 0, UpdateFunction );
	CNOVRListAdd( cnovrLPrerender, 0, PrerenderFunction );
	CNOVRListAdd( cnovrLRender2, 0, RenderFunction );
	//CNOVRListAdd( cnovrLCollide, 0, CollideFunction );
	printf( "+++ Example scene setup complete\n" );

	shutting_down = 0;
	thdmax = OGCreateThread( PhysicsThread, 0 );

}


void start( const char * identifier )
{
	store = CNOVRNamedPtrData( "ovrballstore", 0, sizeof( *store ) + 1024 );
	printf( "=== Initializing %p\n", store );

	sentinal1 = 0xaaaaaaaa;
	sentinal2 = 0xaaaaaaaa;

	identifier = strdup(identifier);
	CNOVRJobTack( cnovrQPrerender, example_scene_setup, 0, 0, 0 );
	printf( "=== Example start %s(%p) + %p %p\n", identifier, identifier );

}

void stop( const char * identifier )
{
	shutting_down = 1;
	OGJoinThread( thdmax );
	printf( "=== End Example stop\n" );
}


