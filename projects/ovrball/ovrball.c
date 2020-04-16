#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovropenvr.h>
#include <cnovrcanvas.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <chew.h>
#include <stdlib.h>
#include <string.h>

const char * identifier;
cnovr_shader * shaderLines;
cnovr_shader_uniform shaderLines_rprops = { "rprops" };
cnovr_shader * shaderEpicenter;
cnovr_shader_uniform shaderEpicenter_ringanimation = { "ringanimation" };
cnovr_shader_uniform shaderEpicenter_ballspeedathit = { "ballspeedathit" };
cnovr_shader * shaderRing;
cnovr_shader_uniform shaderRing_ringanimation = { "ringanimation" };
cnovr_shader * shaderFakeLines;
cnovr_shader * rendermodelshader;
og_thread_t thdmax;

#define TIMESLOTS 10
#define EXTRASLOTS 20
#define PARTICLEVERTS 256

cnovr_pose    eightiessunpose;
cnovr_model * eightiessun;

cnovr_model * paddlesolid;
cnovr_pose    paddlepose1[TIMESLOTS+EXTRASLOTS];
cnovr_pose    paddlepose2[TIMESLOTS+EXTRASLOTS];
cnovr_pose    paddetransform1;
cnovr_pose    paddetransform2;
int racketslot = 0;

cnovr_model * isosphere;
cnovr_pose    isospherepose;
cnovr_pose    isosphereposeLast;
cnovr_pose    isosphereposehist[TIMESLOTS+EXTRASLOTS];
cnovr_point3d  isospheremotionlinear;
cnovr_aamag  isospheremotionrotation;
float isospherehitcooldown;
cnovrfocus_capture isocapture;

int sentinal1;
double isospherelife;
int sentinal2;

int player_has_interacted_with_ball;

cnovr_model * playarea;
cnovr_model * ringmodel;
cnovr_pose    ringpose;
cnovr_point3d HitEpicenter;
double ring_time_of_last_hit;
cnovr_point3d ring_velocity_of_last_hit;
int ring_hit_last;
float         ringz = -18;
cnovr_model * playareacollide;
cnovr_pose    playareapose;
//cnovr_pose   playareaposeepisilondown; //For pushing the triangles down a bit to unmask the lines.
cnovr_pose    boomroot; //Must be origin

cnovr_point3d roomoffset = { 0, 0, 0 };

cnovr_model * explosion_model;
cnovr_shader * explosion_shader;
float * explosion_points;
float * explosion_data;
float * explosion_color;
float * explosion_extradata;

int hitslot = 0;

void EmitParticle( float * pos, float size, float * dir, float * color, float lifetime )
{
	static int exphead;
	float * extra;

	memcpy( explosion_points + exphead*3, pos, sizeof(float)*3 );
	cnovr_point3d halfdir;
	scale3d( halfdir, dir, ((rand()%100)/150.0)+0.2 );
	memcpy( explosion_data + (exphead*4+1), halfdir, sizeof(float)*3 );
	*(explosion_data + (exphead*4+0)) = size;
	memcpy( explosion_color + exphead*4, color, sizeof(float)*4 );
	extra = (explosion_extradata + exphead*4);
	extra[0] = lifetime/5.0; //Current lifetime
	extra[1] = size;
	extra[2] = lifetime;
	extra[3] = 0;

	exphead++;
	memcpy( explosion_points + exphead*3, pos, sizeof(float)*3 );
	memcpy( explosion_data + (exphead*4+1), dir, sizeof(float)*3 );
	*(explosion_data + (exphead*4+0)) = size;
	memcpy( explosion_color + exphead*4, color, sizeof(float)*4 );
	extra = (explosion_extradata + exphead*4);
	extra[0] = lifetime; //Current lifetime
	extra[1] = size;
	extra[2] = lifetime;
	extra[3] = 0;
	exphead++;
	if( exphead == PARTICLEVERTS ) exphead = 0;
}

void Boom( float * pos, int npart, float expand, float lifetime, float * customcolor )
{
	int i;
	for( i = 0; i < npart; i++ )
	{
		cnovr_point3d dir;
		dir[0] = (rand()%102)-50.5;
		dir[1] = (rand()%102)-50.5;
		dir[2] = (rand()%102)-50.5;
		//normalize3d( dir, dir );
		scale3d( dir, dir, 0.4 );
		//^^^^ Ugh buggy compiler
		scale3d( dir, dir, expand );
		float color[4];
		if( customcolor )
		{
			memcpy( color, customcolor, sizeof( color ) );

		}
		else
		{
			color[0] = (rand()%100)+1;
			color[1] = (rand()%100)+1;
			color[2] = (rand()%100)+1;
			color[3] = 1;
			normalize3d( color, color );
		}
		EmitParticle( pos, 20+(rand()%20), dir, color, lifetime );
	}
}


int cpupoints;
int playerpoints;
#define CPUEND -40
#define ACCELEND -26
#define PLAYEREND    5

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
	memcpy( &isosphereposeLast, &isospherepose, sizeof( isospherepose ) );

	isospheremotionlinear[0] = 0;
	isospheremotionlinear[1] = 0;
	isospheremotionlinear[2] = 0;

	isospheremotionrotation[0] = 0;
	isospheremotionrotation[1] = 0;
	isospheremotionrotation[2] = 0;

	player_has_interacted_with_ball = 0;
}

int CheckCollideBallWithMeshSimpleRing( cnovr_model * m, int mesh, cnovr_pose * modelpose )
{
	cnovr_collide_results res;
	cnovr_pose invertedxform;
	cnovr_point3d start, direction;
	pose_invert( &invertedxform, modelpose );
	apply_pose_to_point( start, &invertedxform, isospherepose.Pos); //Get "start" in modelspace.
	apply_pose_to_point( direction, &invertedxform, isospheremotionlinear); //Get "direction" in 
	float radius = 0.1;
	int r = CNOVRModelCollide( m, start, direction, &res, radius, -0.19 );
	if( r < 0 || res.sndist > 0.0 || res.t > 0.0 ) return -1; //No actual collision.

	///XXX TODO BTW This is weird, this is inthe other collide function, but for some reason, here, it really messes things up and we miss collisions.
//	if( res.sndist < -radius || res.t < -radius ) return -1; //We're already on the wrong side of the geometry.

	//Computing half-angle in object-local settings.
	float dotmatch = dot3d( res.collidens, direction );
	if( dotmatch > 0 || dotmatch != dotmatch /*check for nan*/ ) return -1; //don't allow in-line collisions.

	double Now = OGGetAbsoluteTime();
	//Bounce off.
	if( Now - ring_time_of_last_hit > 1 )
	{
		if( isospheremotionlinear[2] < 0 )
		{
			copy3d( HitEpicenter, isospherepose.Pos );
			ring_time_of_last_hit = OGGetAbsoluteTime();
			copy3d( ring_velocity_of_last_hit, isospheremotionlinear );
			ring_hit_last = 1;
			isospheremotionlinear[2] *= -1; 
		}
		else
		{
			//You come through the back of the ring.
			playerpoints += 10;
			copy3d( HitEpicenter, isospherepose.Pos );
			ring_time_of_last_hit = OGGetAbsoluteTime();
			copy3d( ring_velocity_of_last_hit, isospheremotionlinear );
			ring_hit_last = 0;
			ringz =  ( rand() % 20 )/-2.-10.;
		}
	}
	return 0;
}

int CheckCollideBallWithMesh( cnovr_model * m, int mesh, cnovr_pose * modelpose,
	cnovr_pose * modelposelast,
	float dtimelast /*Usually fixed, used to compute paddle speed*/,
	float dtimeframe /*Actual delta time*/, float * modeltarget, float damper, int customcollide )
{
	cnovr_collide_results res;
	cnovr_point3d start, startlast, direction, deltastart;
	cnovr_pose invertedxform, invertedxformlast;
	pose_invert( &invertedxform, modelpose );
	pose_invert( &invertedxformlast, modelposelast );
	apply_pose_to_point( startlast, &invertedxformlast, isosphereposeLast.Pos ); 
	apply_pose_to_point( start, &invertedxform, isospherepose.Pos); //Get "start" in modelspace.
	apply_pose_to_point( direction, &invertedxform, isospheremotionlinear); //Get "direction" in model space.
	sub3d( deltastart, start, startlast );
	scale3d( deltastart, deltastart, 1./dtimelast );
	copy3d( direction, deltastart );

	normalize3d( direction, direction );
	res.t = 1.; //Must actually impact.
	m->iCollideMesh = mesh;
	float radius = 0.1;
	int r = CNOVRModelCollide( m, start, direction, &res, radius, -0.19 );
	if( r < 0 || res.sndist > 0.0 || res.t > 0.0 ) return -1; //No actual collision.
	if( res.sndist < -radius || res.t < -radius ) return -1; //We're already on the wrong side of the geometry.

	//Computing half-angle in object-local settings.
	float dotmatch = dot3d( res.collidens, direction );
	if( dotmatch > 0 || dotmatch != dotmatch /*check for nan*/ ) return -1; //don't allow in-line collisions.


	printf( "COLLIDE: %d ; sndist: %f  res.t: %f  radius: %f  (%f %f %f)\n", r, res.sndist, res.t, radius, PFTHREE( res.collidens ) );
	//COLLISION
	//Here we are in a neat position. We have a collision!  Time to bounce!

	//Part 1:  We fix up the position.  We can do this to "push" our way out of the inside of the object by the worldspace_normal to that object.
	cnovr_point3d worldspace_normal;
	quatrotatevector(worldspace_normal, modelpose->Rot, res.collidens);
	//printf( "Collided depth: %f   %f %f %f  TIME: %f\n", res.t, PFTHREE( res.collidens ), dtimeframe );
	//First, "fix up" location to make sure we don't overpenetrate.
	cnovr_point3d fixup;
	if(res.sndist > -.0001){
		res.sndist = -.0001;
	}
	printf("%f\n", res.sndist);
	scale3d( fixup, worldspace_normal, -res.sndist*1.0 ); //1.0 would be JUST enough to grace the surface.  This buys us some margin.
	add3d( isospherepose.Pos, isospherepose.Pos, fixup );

	//Next we need to "bounce" off. 

	//Figure out collisionpos in 3dspace
	cnovr_point3d collide_pos;
	printf( "CP: %f %f %f\n", PFTHREE( res.collidepos  ) );
	cnovr_point3d localcollidepos;
	scale3d( localcollidepos, res.collidens, -(res.sndist+radius) );
	add3d( localcollidepos, localcollidepos, res.collidepos );
	printf( "CPT %f %f %f   %f\n", PFTHREE( res.collidens ), res.sndist );
	apply_pose_to_point( collide_pos, modelpose, localcollidepos ); 
	printf( "CP: %f %f %f\n", PFTHREE( collide_pos  ) );
	Boom( collide_pos, 10, .1, .5, 0 );

	//We have a rotating sphere.  If it is colliding with a surface moving tangent to it,
	//then we should modify the torque on the ball and torque should affect motion vector 
	// isospheremotionrotation is an aamag of the rotation.
	cnovr_point3d tangent_motion;
	cnovr_point3d torque_motion;
	float motionforce = magnitude3d( deltastart )*3.;
	cross3d( torque_motion, res.collidens, direction );
	cross3d( tangent_motion, torque_motion, res.collidens );

	cnovr_aamag  rotation_in_model_space;
	quatrotatevector( rotation_in_model_space, invertedxform.Pos, isospheremotionrotation );
	//This is effectively a torque vector at this point.
	cnovr_point3d imparted_force;
	cross3d( imparted_force, rotation_in_model_space, res.collidens );
	scale3d( imparted_force, imparted_force, -.1 );
	printf( "Imparted force: %f %f %f\n", PFTHREE( imparted_force ) );
	//For now, discard, just apply other motion.
//	rotation_in_model_space[3] = magnitude3d( tangent_motion );

	//scale3d( rotation_in_model_space, rotation_in_model_space, 0.75 );

	cnovr_aamag  torquey;
	cnovr_point3d normal_in_object_space;
	cross3d( torquey, res.collidens, isospheremotionlinear );
	printf( "TORQUEY: %f %f %f\n", PFTHREE( torquey ) );
	copy3d(  rotation_in_model_space, torquey );

//	normalize3d( rotation_in_model_space, rotation_in_model_space );
//	scale3d(  rotation_in_model_space, rotation_in_model_space, motionforce );

	quatrotatevector(isospheremotionrotation, modelpose->Rot, rotation_in_model_space);

//	isospheremotionrotation[3] = rotation_in_model_space[3];
//	printf( "TORQUE: %f %f %f  %f\n", PFTHREE( isospheremotionrotation ), motionforce );

	//If we have paddle motion and normal in world space, what more do we need?
	//REFLECT: r=d-2(d⋅n)n; d = incoming relative motion vector. (relative_motion_vector)
	cnovr_point3d reflection_local, reflection_world, tmp;
	float dotr = -2*dotmatch;
	scale3d( reflection_local, res.collidens, dotr );
	scale3d( reflection_local, reflection_local, magnitude3d( deltastart ) ); //Apply velocity from original delta speed.
	quatrotatevector(reflection_world, modelpose->Rot, reflection_local);

	scale3d( reflection_world, reflection_world, 0.9 );	//Mute the ball a little
	add3d( reflection_world, imparted_force , reflection_world );
	add3d( isospheremotionlinear, isospheremotionlinear, reflection_world );

	//Need to limit overall speed.
//	if( magnitude3d( isospheremotionlinear ) > 20.0 ) 
//		scale3d( isospheremotionlinear, isospheremotionlinear, 20./magnitude3d( isospheremotionlinear ) );


	return 0;
}




void init( const char * identifier )
{
	ovrprintf( "Example init %s\n", identifier );
}

void * PhysicsThread( void * v )
{
	int after_first;
//	cnovr_pose playareapose;
//	pose_make_identity( &playareapose );
	VRActionHandle_t tip1 = CNOVRFocusGetVRActionHandleFromConrollerAndCtrlA( 1, CTRLA_TIP );
	VRActionHandle_t tip2 = CNOVRFocusGetVRActionHandleFromConrollerAndCtrlA( 2, CTRLA_TIP );

	cnovr_pose poselast1;
	cnovr_pose poselast2;

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
		memcpy( &isosphereposeLast, &isospherepose, sizeof( isospherepose ) );
		isospherelife -= deltatime;
		if( isospherelife < 0.0f ) { ResetIsosphere(); printf( "NEWLIFE %f\n", isospherelife ); }
		cnovr_point3d delta_motion;
		scale3d( delta_motion, isospheremotionlinear, deltatime );
		add3d( isospherepose.Pos, isospherepose.Pos, delta_motion );
		cnovr_aamag aam;
		cnovr_quat qmotion;
		scale3d( aam, isospheremotionrotation, deltatime );
		quatfromaxisanglemag( qmotion, aam );
		quatrotateabout( isospherepose.Rot, qmotion, isospherepose.Rot );

		//Add magnus effect.
		cnovr_point3d magnus;
		cnovr_point3d sqrrot;
//		mult3d( sqrrot, isospheremotionrotation, isospheremotionrotation );
		copy3d( sqrrot, isospheremotionrotation );
		cross3d( magnus, isospheremotionlinear, sqrrot );
		scale3d( magnus, magnus, -.00001 );
		add3d( isospheremotionlinear, isospheremotionlinear, magnus );


		if( player_has_interacted_with_ball )
		{
			cnovr_point3d linearmodfromacceleration = { 0, 0, 0 };
			linearmodfromacceleration[1] = -deltatime * 7.0; //Add gavity.
			add3d( isospheremotionlinear, isospheremotionlinear, linearmodfromacceleration ); 
		}

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

		apply_pose_to_pose( &paddlepose1[racketslot], &pose1, &paddetransform1 );
		apply_pose_to_pose( &paddlepose2[racketslot], &pose2, &paddetransform2 );

		//if( isospherehitcooldown > .05f )
		if( after_first )
		{
			cnovr_point3d target = { 0, -.4f, 0 };  //Kludge -> Target center of mesh.
			int r1 = //-1;
				CheckCollideBallWithMesh( paddlesolid, 1, &paddlepose1[racketslot], &poselast1, 
				deltatime, tnow, target, 1.5f, 0 ); 
			int r2 = //-1;
				CheckCollideBallWithMesh( paddlesolid, 1, &paddlepose2[racketslot], &poselast2, 
				deltatime, tnow, target, 1.5f, 0 );
			int r3 = CheckCollideBallWithMesh( playareacollide, 1, &playareapose, &playareapose,
				deltatime, tnow, 0, 0.9f, 1 );

			CheckCollideBallWithMeshSimpleRing( ringmodel, 0, &ringpose );
			CheckCollideBallWithMeshSimpleRing( ringmodel, 1, &ringpose );
			CheckCollideBallWithMeshSimpleRing( ringmodel, 2, &ringpose );

			if( r1 == 0 || r2 == 0 || r3 == 0 ) {
				if( r1 == 0 || r2 == 0 )
				{
					player_has_interacted_with_ball = 1;

					memcpy( &paddlepose1[hitslot+TIMESLOTS],  &paddlepose1[racketslot], sizeof( cnovr_pose ) );
					memcpy( &paddlepose2[hitslot+TIMESLOTS],  &paddlepose2[racketslot], sizeof( cnovr_pose ) );
					isospherehitcooldown = 0;
					printf( "Writing into slot %d\n", hitslot );
					hitslot++;
					if( hitslot == EXTRASLOTS ) hitslot = 0;
				}
			}
		}
		after_first = 1;

		memcpy( &poselast1, &paddlepose1[racketslot], sizeof( cnovr_pose ) );
		memcpy( &poselast2, &paddlepose2[racketslot], sizeof( cnovr_pose ) );
		memcpy( &isosphereposehist[racketslot], &isospherepose, sizeof( isospherepose ) );
	//	printf( "%d %f %f %f\n", racketslot, PFTHREE( isosphereposehist[racketslot].Pos ) );
		//Check end stops
		if( isospherepose.Pos[2] < CPUEND ) { playerpoints++; Boom( isospherepose.Pos, 100, 2.0, 4.0, 0 ); ResetIsosphere(); printf( "cpuend\n" ); }
		if( isospherepose.Pos[2] > PLAYEREND ) { cpupoints++; Boom( isospherepose.Pos, 100, 2.0, 4.0, (float[4]){ 1., 0., 0., 1. }  ); ResetIsosphere(); printf( "playerend\n" ); }

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
	int i;
	glLineWidth( 2.0 );
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
	static float ovrhist[96];
	static int histhead;
	ovrhist[histhead] = cnovrstate->fFrameTimems;
	histhead = (histhead+1)%96;
	CNOVRCanvasDrawText( canvas, 2, 2, trprintf( "%3.fFPS\n%4d%4d\n%.2fms", 10./fpstime, cpupoints, playerpoints, cnovrstate->fFrameTimems ), 3 );
	for( i = 0; i < 96; i++ )
	{
		int px = ovrhist[(i+histhead)%96]*2.0; 
		canvas->color = 0xff00ff00;
		if( px > 18 ) canvas->color = 0xff0000ff;
		CNOVRCanvasTackSegment( canvas, i, 64, i, 64-px );
	}
	canvas->color = 0xffffffff;
	CNOVRCanvasSwapBuffers( canvas );

	for( i = 0; i < PARTICLEVERTS; i++ )
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

	int ctrl;
	for( ctrl = 1; ctrl < 3; ctrl++ )
	{
		cnovrfocus_properties * p = CNOVRFocusGetPropsForDev( ctrl );
		uint32_t bm = p->buttonmask[0];
		//printf( "%d %d\n", bm, (1<<CTRLA_TRIGGER) );
		if( bm & (1<<CTRLA_TRIGGER) )
		{
			cnovr_point3d target;
			if(ctrl == 1)
			{
				sub3d( target, paddlepose1[(hitslot - 1 + TIMESLOTS) % TIMESLOTS].Pos, isospherepose.Pos );
			}
			else
			{
				sub3d( target, paddlepose2[(hitslot -1 + TIMESLOTS) % TIMESLOTS].Pos, isospherepose.Pos );
			}
			//sub3d( target, roomoffset, isospherepose.Pos );
			target[1] += .5;
			scale3d( isospheremotionlinear, isospheremotionlinear, 0.9 );
			scale3d( target, target, .2 );
			add3d( isospheremotionlinear, isospheremotionlinear, target );
		}
	}

	//Determine distance from camera to ball and set as foregreound point.
	//cnovr_point3d fvcamdist;
	//apply_pose_to_point(fvcamdist, &cnovrstate->pPreviewPose, isospherepose.Pos);
	//float fcamdist = -fvcamdist[2];
	//cnovrstate->fPreviewForegroundSplitDistance = fcamdist;

	return;
}

void PrerenderFunction( void * tag, void * opaquev )
{
//If you want to lock to "Display" paddle, do this.
//	apply_pose_to_pose( &paddlepose1, CNOVRFocusGetTipPose(1), &paddetransform1 );
//	apply_pose_to_pose( &paddlepose2, CNOVRFocusGetTipPose(2), &paddetransform2 );
	int draw_matrix_slot = ((racketslot+TIMESLOTS*2-1)%TIMESLOTS);
	cnovr_pose * isopos = &isosphereposehist[draw_matrix_slot];
	float weight = .01;
	ringpose.Pos[0] = isopos->Pos[0] * weight + ringpose.Pos[0] * (1.-weight);
	ringpose.Pos[1] = isopos->Pos[1] * weight + ringpose.Pos[1] * (1.-weight);
//	copy3d( ringpose.Pos, isopos->Pos );
	ringpose.Pos[2] = ringz;
}

void RenderFunction( void * tag, void * opaquev )
{
	int i;
	CNOVRRender( rendermodelshader );
	CNOVRRender( eightiessun );

	CNOVRRender( shaderFakeLines );
	playareacollide->iRenderMesh = 0;

//player stuff was here
	paddlesolid->iRenderMesh = 1;
	int draw_matrix_slot = ((racketslot+TIMESLOTS*2-1)%TIMESLOTS);
	cnovr_pose * isopos = &isosphereposehist[draw_matrix_slot];
    glDisable(GL_CULL_FACE);
	CNOVRModelRenderWithPose( paddlesolid, &paddlepose1[draw_matrix_slot] );
	CNOVRModelRenderWithPose( paddlesolid, &paddlepose2[draw_matrix_slot] );
	CNOVRModelRenderWithPose( isosphere, isopos );
    glEnable(GL_CULL_FACE);
	ringmodel->iRenderMesh = 2;

	CNOVRRender( shaderEpicenter );

	glUniform4f( CNOVRUniform( &shaderEpicenter_ringanimation ), 
		OGGetAbsoluteTime()-ring_time_of_last_hit, HitEpicenter[0], HitEpicenter[1], HitEpicenter[2] );
	glUniform4f( CNOVRUniform( &shaderEpicenter_ballspeedathit ),
		ring_velocity_of_last_hit[0], ring_velocity_of_last_hit[1], ring_velocity_of_last_hit[2], 0 );
//Boat mode.. :-)
	//Wash over the scene to prevent lines from overdrawing.
	CNOVRModelRenderWithPose( playareacollide, &playareapose );


	CNOVRRender( shaderRing );
	double nestate  = OGGetAbsoluteTime()-ring_time_of_last_hit;
	glUniform4f( CNOVRUniform( &shaderRing_ringanimation ), 
		(float)ring_hit_last, OGGetAbsoluteTime()-ring_time_of_last_hit, 0.0f, 0.0f );
//	copy3d( ringpose.Pos, isosphereposehist[draw_matrix_slot].Pos );
	CNOVRModelRenderWithPose( ringmodel, &ringpose );



	CNOVRRender( shaderLines );
	playarea->iRenderMesh = 2;
	CNOVRRender( playarea );	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	glUniform4f( CNOVRUniform( &shaderLines_rprops ), 1.0f, 0.0f, 0.0f, 0.0f );

	CNOVRRender( explosion_shader );
	CNOVRRender( explosion_model );

	CNOVRRender( canvas );
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


void example_scene_setup( void * tag, void * opaquev )
{
	printf( "+++ Example scene setup\n" );
	int i;
	int j;
	char * rv;
	int controllers = 0;
	int is_vive = 0;
	int is_controller = 0;
	float paddle_angles[] = //In case we create support for other controllers, store angles here
	{.4, .6, .4, .5, 		//Knuckles angles
	0, 0, .7, .7};			//Vive angles
	
	shaderEpicenter = CNOVRShaderCreate( "epicenter" );
	shaderLines = CNOVRShaderCreate( "retrolines" );
	shaderRing = CNOVRShaderCreate( "ringshader" );
	shaderFakeLines = CNOVRShaderCreate( "fakelines" );
//	rendermodelshader = CNOVRShaderCreate( "assets/rendermodel" );
	rendermodelshader = CNOVRShaderCreate( "assets/rendermodelnearestaa" );

	canvas = CNOVRCanvasCreate( "ExampleCanvas", 96, 64, 0 );

	playarea = CNOVRModelCreate( 0, GL_LINES );
	playarea->pose = &playareapose;
	pose_make_identity( &playareapose );
	add3d( playareapose.Pos, playareapose.Pos, roomoffset );
	CNOVRModelLoadFromFileAsync( playarea, "playarea.obj:lineify" );

	//pose_make_identity( &playareaposeepisilondown );
	//playareaposeepisilondown.Pos[1] -= .02;
	//playareaposeepisilondown.Pos[2] = playareapose.Pos[2];
	playareacollide = CNOVRModelCreate( 0, GL_TRIANGLES );
	playareacollide->pose = &playareapose;
	CNOVRModelLoadFromFileAsync( playareacollide, "playarea.obj:barytc" );

	isosphere = CNOVRModelCreate( 0, GL_TRIANGLES );
//	isosphere->pose = &isospherepose;
	CNOVRModelLoadFromFileAsync( isosphere, "isosphere.obj:barytc" );
	ResetIsosphere();

	ringmodel = CNOVRModelCreate( 0, GL_TRIANGLES );
	CNOVRModelLoadFromFileAsync( ringmodel, "ring.obj:barytc" );
	pose_make_identity( &ringpose );
	ringpose.Pos[2] = -20;

	paddlesolid = CNOVRModelCreate( 0, GL_TRIANGLES );
	CNOVRModelLoadFromFileAsync( paddlesolid, "paddle.obj:barytc" );

	pose_make_identity( &paddetransform1 );
	pose_make_identity( &paddetransform2 );
	
	//Default back to knuckles if nothing is found
	//Copy knuckles values in cnovr_quat
	quatcopy( paddetransform1.Rot, &( paddle_angles[0] ) );
	quatcopy( paddetransform2.Rot, paddetransform1.Rot );
	
	//Check every devices tracked
	for( i = 0; i < MAX_POSES_TO_PULL_FROM_OPENVR; ++i )
	{
		rv = CNOVRGetTrackedDeviceString( i, ETrackedDeviceProperty_Prop_ModelNumber_String );
		
		if( strlen(rv) <= 0 ) break;	//We reached the end of valid devices, rv <= 0
		
		is_vive = strstr( rv, "Vive" )?1:0;
		is_controller = strstr( rv, "Controller" )?1:0;

		//Are we using something that is a Vive?
		if( is_vive )
			//Is it a controller?
			if( is_controller )
			{
				printf( "Device %d is HTC Controller\n", i );
				controllers++;
				if( controllers == 1)	//Apply angles for the 1st controller
					 quatcopy( paddetransform1.Rot, &( paddle_angles[4] ) );
					
				if( controllers == 2 )	//Apply angles for the 2nd controller
					quatcopy( paddetransform2.Rot, paddetransform1.Rot );
			}
			else
				printf( "Device %d is HTC Headset\n", i );
	}

//	cnovr_euler_angle eu = { 3.14159, 1.57, -.2 };
//	quatfromeuler( paddetransform.Rot, eu );
	//Paddle transformation.  We are terrible people.
	
	quatnormalize(paddetransform1.Rot,paddetransform1.Rot);
	quatnormalize(paddetransform2.Rot,paddetransform2.Rot);
	
	explosion_shader = CNOVRShaderCreate( "explosion" );
	explosion_model = CNOVRModelCreate( 0, GL_LINES );
	CNOVRModelSetNumVBOsWithStrides( explosion_model, 4, 3, 4, 4, 4 );
	for( i = 0; i < PARTICLEVERTS; i++ )
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

	eightiessun = CNOVRModelCreate( 0, GL_TRIANGLES );
	cnovr_point3d eightiessunsize = { 1, 1, 1 };
	CNOVRModelAppendMesh( eightiessun, 2, 2, 1, eightiessunsize ,0, 0 );
	pose_make_identity( &eightiessunpose );
	eightiessunpose.Pos[2] = -102;
	eightiessunpose.Pos[1] = 17;
	eightiessunpose.Scale = 10;
	eightiessun->pose = &eightiessunpose;
	CNOVRModelApplyTextureFromFileAsync( eightiessun, "80sSun.png" );

	UpdateFunction(0,0);
	CNOVRListAdd( cnovrLUpdate, 0, UpdateFunction );
	CNOVRListAdd( cnovrLPrerender, 0, PrerenderFunction );
	CNOVRListAdd( cnovrLRender2, 0, RenderFunction );
	//CNOVRListAdd( cnovrLCollide, 0, CollideFunction );
	printf( "+++ Example scene setup complete\n" );

	shutting_down = 0;
	thdmax = OGCreateThread( PhysicsThread, 0 );
}


void ovrballstart( const char * identifier )
{
	store = CNOVRNamedPtrData( "ovrballstore", 0, sizeof( *store ) + 1024 );
	printf( "=== Initializing %p\n", store );

	sentinal1 = 0xaaaaaaaa;
	sentinal2 = 0xaaaaaaaa;

	identifier = strdup(identifier);
	CNOVRJobTack( cnovrQPrerender, example_scene_setup, 0, 0, 0 );
	printf( "=== Example start %s(%p)\n", identifier, identifier );

}

void ovrballstop( const char * identifier )
{
	shutting_down = 1;
	OGJoinThread( thdmax );
	printf( "=== End Example stop\n" );
}

//this is for the static compilation version
#ifdef SINGLEBUILD

#include <stdio.h>
#include "../modules/openvrobjects/openvrobjects.c"

int main()
{
#if 0
	FILE * f = fopen( "testmemfile.txt", "rb" );
	printf( "F: %p\n", f );
	if( f )
	{
		printf( "SEEK\n" );
		fseek( f, 0, SEEK_END );
		printf( "SEEK OK\n" );
		printf( "TELL: %ld\n", ftell( f ) );
		fseek( f, 0, SEEK_SET );
		char buf[1024];
		fread( buf, 1, 6, f );
		fclose( f );
		buf[7] = 0;
		printf( "BUF: %s\n", buf );
	}
#endif

//	return 0;

	if( CNOVRInit( "test", 0, 0, 1 ) )
	{
		fprintf( stderr, "Error: Could not init CNOVR.\n" );
		return -1;
	}
	
	cnovrstate->iMultisample = 0;
	
	CNOVRFileSearchAddPath( "ovrball" );
	openvrobjectsstart( "wireframe,nodetail" );
	ovrballstart( "test" );
	while(1)
	{
		CNOVRUpdate();
	}
	//unreachable.
	openvrobjectsstop( 0 );
	ovrballstop( 0 );
}

#endif

