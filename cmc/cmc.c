// The name of this game is Cosmic Miscreant Conditioner
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


int points;


#include "cmccanvas.h"
#include "boom.h"

#include "player.h"
#include "projectiles.h"
#include "robots.h"
#include "ui.h"

//Player/Guns
//Projectiles
//Robots
//UI
//Play area lives here.



const char * identifier;
cnovr_shader * shaderLines;
cnovr_shader * shaderFakeLines;
cnovr_shader * rendermodelshader;

og_thread_t thdmax;

#define TIMESLOTS 10
#define EXTRASLOTS 20

cnovr_pose    eightiessunpose;
cnovr_model * eightiessun;
cnovr_model * playarea;
cnovr_model * playareacollide;
cnovr_pose    playareapose;

cnovr_point3d roomoffset = { 0, 0, 0 };


int hitslot = 0;

struct ovrballstore_t
{
	int i;
} * store;

int shutting_down;

void init( const char * identifier )
{
	ovrprintf( "Example init %s\n", identifier );
}

void * PhysicsThread( void * v )
{
	int after_first;
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

#if 0
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

		int did_hit_this_frame = 0;
#endif
		if( !cnovrstate->oInput ) continue;
		InputPoseActionData_t pad1, pad2;
		float tnow = -0.001;
		EVRInputError e1 = cnovrstate->oInput->GetPoseActionDataRelativeToNow( tip1, ETrackingUniverseOrigin_TrackingUniverseStanding, tnow, &pad1, sizeof( pad1 ), 0 );
		EVRInputError e2 = cnovrstate->oInput->GetPoseActionDataRelativeToNow( tip2, ETrackingUniverseOrigin_TrackingUniverseStanding, tnow, &pad2, sizeof( pad2 ), 0 );

		cnovr_pose pose1, pose2;
		CNOVRPoseFromHMDMatrix( &pose1, &pad1.pose.mDeviceToAbsoluteTracking );
		CNOVRPoseFromHMDMatrix( &pose2, &pad2.pose.mDeviceToAbsoluteTracking );
		//printf( "%f %f %f\n", PFTHREE( pose2.Pos ) );

#if 0
		apply_pose_to_pose( &paddlepose1[racketslot], &pose1, &paddetransform );
		apply_pose_to_pose( &paddlepose2[racketslot], &pose2, &paddetransform );

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
#endif
	}

	return 0;
}


void UpdateFunction( void * tag, void * opaquev )
{
	int i;
	static double start;
	static double last;
	static double lastframetime;
	double now = OGGetAbsoluteTime();
	if( start < 1 ) { start = now; last = now; return; }
	double runtime = now - start;
	float deltatime = (float)(now - last);

	glLineWidth( 2.0 );

    UpdateCanvas( deltatime );
    UpdateExplosionData( deltatime );

#if 0
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
#endif

	return;
}

void PrerenderFunction( void * tag, void * opaquev )
{
#if 0
//If you want to lock to "Display" paddle, do this.
//	apply_pose_to_pose( &paddlepose1, CNOVRFocusGetTipPose(1), &paddetransform );
//	apply_pose_to_pose( &paddlepose2, CNOVRFocusGetTipPose(2), &paddetransform );
	int draw_matrix_slot = ((racketslot+TIMESLOTS*2-1)%TIMESLOTS);
	cnovr_pose * isopos = &isosphereposehist[draw_matrix_slot];
	float weight = .01;
	ringpose.Pos[0] = isopos->Pos[0] * weight + ringpose.Pos[0] * (1.-weight);
	ringpose.Pos[1] = isopos->Pos[1] * weight + ringpose.Pos[1] * (1.-weight);
//	copy3d( ringpose.Pos, isopos->Pos );
	ringpose.Pos[2] = ringz;
#endif
}

void RenderFunction( void * tag, void * opaquev )
{
	int i;
	CNOVRRender( rendermodelshader );
	CNOVRRender( eightiessun );

	CNOVRRender( shaderFakeLines );
	playareacollide->iRenderMesh = 0;
	CNOVRModelRenderWithPose( playareacollide, &playareapose );

	CNOVRRender( shaderLines );
	playarea->iRenderMesh = 2;
	CNOVRRender( playarea );	
#if 0

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
	glUniform4f( 21, OGGetAbsoluteTime()-ring_time_of_last_hit, HitEpicenter[0], HitEpicenter[1], HitEpicenter[2] );
	glUniform4f (22, ring_velocity_of_last_hit[0], ring_velocity_of_last_hit[1], ring_velocity_of_last_hit[2], 0 );
//Boat mode.. :-)
	//Wash over the scene to prevent lines from overdrawing.
	CNOVRModelRenderWithPose( playareacollide, &playareapose );


	CNOVRRender( shaderRing );
	double nestate  = OGGetAbsoluteTime()-ring_time_of_last_hit;
	glUniform4f( 21, (float)ring_hit_last, OGGetAbsoluteTime()-ring_time_of_last_hit, 0.0f, 0.0f );
//	copy3d( ringpose.Pos, isosphereposehist[draw_matrix_slot].Pos );
	CNOVRModelRenderWithPose( ringmodel, &ringpose );



#endif
	CNOVRRender( shaderLines );
	playarea->iRenderMesh = 2;
	CNOVRRender( playarea );	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	glUniform4f( 19, 1.0f, 0.0f, 0.0f, 0.0f );

#define GL_VERTEX_PROGRAM_POINT_SIZE 0x8642
	glHint(GL_POINT_SMOOTH_HINT, GL_FASTEST);
	glEnable( GL_VERTEX_PROGRAM_POINT_SIZE );
//	glDisable(GL_POINT_SMOOTH);

    RenderExplosions();

    RenderCanvas();
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


void example_scene_setup( void * tag, void * opaquev )
{
	printf( "+++ Example scene setup\n" );
	int i;
//	shaderEpicenter = CNOVRShaderCreate( "ovrball/epicenter" );
	shaderLines = CNOVRShaderCreate( "ovrball/retrolines" );
//	shaderRing = CNOVRShaderCreate( "ovrball/ringshader" );
	shaderFakeLines = CNOVRShaderCreate( "assets/fakelines" );
//	rendermodelshader = CNOVRShaderCreate( "assets/rendermodel" );
	rendermodelshader = CNOVRShaderCreate( "assets/rendermodelnearestaa" );

    InitCanvas();

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

	//isosphere = CNOVRModelCreate( 0, GL_TRIANGLES );
//	isosphere->pose = &isospherepose;
//	CNOVRModelLoadFromFileAsync( isosphere, "isosphere.obj:barytc" );
//	ResetIsosphere();

//	ringmodel = CNOVRModelCreate( 0, GL_TRIANGLES );
//	CNOVRModelLoadFromFileAsync( ringmodel, "ring.obj:barytc" );
//	pose_make_identity( &ringpose );
//	ringpose.Pos[2] = -20;

//	paddlesolid = CNOVRModelCreate( 0, GL_TRIANGLES );
//	CNOVRModelLoadFromFileAsync( paddlesolid, "paddle.obj:barytc" );

//	pose_make_identity( &paddetransform );

//	cnovr_euler_angle eu = { 3.14159, 1.57, -.2 };
//	quatfromeuler( paddetransform.Rot, eu );
	//Paddle transformation.  We are terrible people.
//	paddetransform.Rot[0] = 0.4;
//	paddetransform.Rot[1] = 0.6;
//	paddetransform.Rot[2] = 0.4;
///	paddetransform.Rot[3] = 0.5;
//	quatnormalize(paddetransform.Rot,paddetransform.Rot);

    ExplosionInit();

	eightiessun = CNOVRModelCreate( 0, GL_TRIANGLES );
	cnovr_point3d eightiessunsize = { 1, 1, 1 };
	CNOVRModelAppendMesh( eightiessun, 2, 2, 1, eightiessunsize ,0, 0 );
	pose_make_identity( &eightiessunpose );
	eightiessunpose.Pos[2] = -102;
	eightiessunpose.Pos[1] = 17;
	eightiessunpose.Scale = 10;
	eightiessun->pose = &eightiessunpose;
	CNOVRModelApplyTextureFromFileAsync( eightiessun, "ovrball/80sSun.png" );

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

//	sentinal1 = 0xaaaaaaaa;
//	sentinal2 = 0xaaaaaaaa;

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


