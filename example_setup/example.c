#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <stdlib.h>
#include <string.h>

og_thread_t thdmax;
const char * identifier;
cnovr_shader * shader;
cnovr_model * model;
cnovr_simple_node * node;

#define MAX_SPINNERS 50
cnovr_model * spinner_m[MAX_SPINNERS];
cnovr_simple_node * spinner_n[MAX_SPINNERS];
float zapped[MAX_SPINNERS];
int shutting_down;
cnovrfocus_capture focusblock[MAX_SPINNERS];

int draggingid[CNOVRINPUTDEVS] = { -1, -1, -1 };
cnovr_pose draggingpose[CNOVRINPUTDEVS]; //Pose of object relative to tip.

int FocusEvent( int event, cnovrfocus_capture * cap, cnovrfocus_properties * prop, int buttoninfo );

void * my_thread( void * v )
{
	while(1 )
	{
		//printf( "THREADS 10 %s %p\n", v, thdmax );
		OGUSleep(10000);
		if( !shutting_down )
		{
			node->pose.Pos[0] = (rand()%10 - 5)/10.;
			node->pose.Pos[1] = (rand()%10 - 5)/10.;
			node->pose.Pos[2] = (rand()%10 - 5)/10.;
			node->pose.Scale = .1;
		}
	}
	return 0;
}


void init( const char * identifier )
{
	ovrprintf( "Example init %s\n", identifier );
}

void UpdateFunction( void * tag, void * opaquev )
{
	static double start;
	double now = OGGetAbsoluteTime();
	if( start < 1 ) start = now;

	int i;
	double truedt = now - start;
	for (i = 0; i < MAX_SPINNERS; i++ )
	{
		double dt =  truedt*.2*1 - i * 3.14159;
		double ang = (i * .4) + (now - start)*.2*1;
		spinner_n[i]->pose.Scale = .2;
//		spinner_n[i]->pose.Pos[1] = 2;
		if( zapped[i] >= 1.0 ) { continue; } //spinner_n[i]->pose.Scale = .4;
		if( zapped[i] > 0 ) zapped[i] -= .001;
		if( zapped[i] < 0 ) zapped[i] = 0;
		float z = zapped[i];
		spinner_n[i]->pose.Pos[0] = cnovr_lerp( sin( ang ), spinner_n[i]->pose.Pos[0], z );
		spinner_n[i]->pose.Pos[1] = cnovr_lerp( sin( dt*1.25), spinner_n[i]->pose.Pos[1], z );
		spinner_n[i]->pose.Pos[2] = cnovr_lerp( cos( ang ), spinner_n[i]->pose.Pos[2], z );

		cnovr_euler_angle e;
		e[0] = 0;
		e[1] = 0; //add back ang
		e[2] = 0;
		cnovr_quat targetquat;
		quatfromeuler( targetquat, e );
		quatslerp( spinner_n[i]->pose.Rot, targetquat, spinner_n[i]->pose.Rot, z );
	}

	return;
}


void RenderFunction( void * tag, void * opaquev )
{
	//Nothing
}

int FocusEvent( int event, cnovrfocus_capture * cap, cnovrfocus_properties * prop, int buttoninfo )
{
	//printf( "EVENT: %d %d %d\n", event, cap->opaque, buttoninfo );
	int opa = (int)cap->opaque;

	switch( event )
	{
		case CNOVRF_DOWNNOFOCUS:
			printf( "DOWN %d %d\n", prop->devid, buttoninfo );
			if( buttoninfo == 3 ) CNOVRFocusAcquire( cap, 1 );
			if( buttoninfo == 0 ) { zapped[opa] = .999; }
			break;
		case CNOVRF_DRAG:
		{
			if( draggingid[prop->devid] == opa )
			{
				cnovr_pose * dragout = draggingpose + prop->devid;
				apply_pose_to_pose( &spinner_n[opa]->pose, &prop->poseTip, dragout );
				spinner_n[opa]->pose.Pos[0] = floor( spinner_n[opa]->pose.Pos[0] * 50 ) / 50.;
				spinner_n[opa]->pose.Pos[1] = floor( spinner_n[opa]->pose.Pos[1] * 50 ) / 50.;
				spinner_n[opa]->pose.Pos[2] = floor( spinner_n[opa]->pose.Pos[2] * 50 ) / 50.;
				if( spinner_n[opa]->pose.Rot[0] > .9 || spinner_n[opa]->pose.Rot[0] < -.9 ) { quatidentity( spinner_n[opa]->pose.Rot ); }
				if( spinner_n[opa]->pose.Rot[1] > .9 || spinner_n[opa]->pose.Rot[1] < -.9 ) { quatidentity( spinner_n[opa]->pose.Rot ); }
				if( spinner_n[opa]->pose.Rot[2] > .9 || spinner_n[opa]->pose.Rot[2] < -.9 ) { quatidentity( spinner_n[opa]->pose.Rot ); }
				if( spinner_n[opa]->pose.Rot[3] > .9 || spinner_n[opa]->pose.Rot[3] < -.9 ) { quatidentity( spinner_n[opa]->pose.Rot ); }
			}
			break;
		}
		case CNOVRF_UPFOCUS:
			draggingid[prop->devid] = -1;
			CNOVRFocusAcquire( cap, 0 );
			break;
		case CNOVRF_LOSTFOCUS:
			printf( "LOST FOCUS\n" );
			break;
		case CNOVRF_ACQUIREDFOCUS:
		{
			printf( "GOT FOCUS\n" );
			zapped[opa] = 1;
			draggingid[prop->devid] = opa;
			cnovr_pose * dragout = draggingpose + prop->devid;
			unapply_pose_from_pose( dragout, &prop->poseTip, &spinner_n[opa]->pose );
			break;
		}
	}
}




void CollideFunction( void * tag, void * opaquev )
{
	cnovrfocus_properties * p = (cnovrfocus_properties*)opaquev;
	int i;


	for (i = 0; i < MAX_SPINNERS; i++ )
	{
		cnovr_point3d start = { 0, 0, 0 };
		cnovr_vec3d direction = { 0, 0, 1 };
		cnovr_pose invertedxform;
		pose_invert( &invertedxform, &spinner_n[i]->pose );
		apply_pose_to_point( start, &p->poseTip, start);
		apply_pose_to_point( start, &invertedxform, start);
		apply_pose_to_point( direction, &p->poseTip, direction);
		apply_pose_to_point( direction, &invertedxform, direction);
		sub3d( direction, direction, start );
		cnovr_collide_results res;
		res.t = p->NewPassiveRealDistance;
		int r = CNOVRModelCollide( spinner_m[i], start, direction, &res );
		if( r >= 0 && res.t > 0 )
		{
			CNOVRFocusRespond( &focusblock[i], res.t );
			//printf( "zapped %d\n", i );
			//zapped[i] = 1;
		}
	}
	//printf( "%f\n", p->poseTip.Pos[0] );
	//spinner_n[i]->pose
}

static void example_scene_setup( void * tag, void * opaquev )
{
	int i;
	cnovr_simple_node * root = cnovrstate->pRootNode;
	node = CNOVRNodeCreateSimple( 1 );
	model = CNOVRModelCreate( 0, 3, GL_TRIANGLES );
	CNOVRModelAppendCube( model, (cnovr_point3d){ 1.f, 1.f, 1.f }, 0, 0 );
	shader = CNOVRShaderCreate( "assets/basic" );
	CNOVRNodeAddObject( node, shader );
	CNOVRNodeAddObject( node, model );
	CNOVRNodeAddObject( root, node );

	cnovr_shader * spinner_s;

	for( i = 0; i < MAX_SPINNERS; i++ )
	{
		spinner_n[i] = CNOVRNodeCreateSimple( 1 );
		spinner_m[i] = CNOVRModelCreate( 0, 3, GL_TRIANGLES );
		CNOVRModelAppendCube( spinner_m[i], (cnovr_point3d){ 1.f, 1.f, 1.f }, 0, 0 );
		CNOVRNodeAddObject( spinner_n[i], spinner_m[i] );
		CNOVRNodeAddObject( root, spinner_n[i] );
		focusblock[i].opaque = (void*)i;
		focusblock[i].cb = FocusEvent;
	}

	UpdateFunction(0,0);
	CNOVRListAdd( cnovrLUpdate, 0, UpdateFunction );
	CNOVRListAdd( cnovrLRender, 0, RenderFunction );
	CNOVRListAdd( cnovrLCollide, 0, CollideFunction );

	thdmax = OGCreateThread( my_thread, (void*)identifier );
}


void start( const char * identifier )
{
	identifier = strdup(identifier);
	printf( "Example start %s(%p)                   ++++++++++++++++++++%p %p\n", identifier, identifier );

	CNOVRJobTack( cnovrQPrerender, example_scene_setup, 0, 0, 0 );

	printf( "Example start OK %s                   ++++++++++++++++++++%p %p\n", identifier );
}

void stop( const char * identifier )
{
	shutting_down = 1;
	printf( "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Start stop\n" );
	if( node )
	{
		CNOVRNodeRemoveObject( cnovrstate->pRootNode, node );
		//CNOVRDelete( node );
		int i;
		for( i = 0; i < MAX_SPINNERS; i++ )
		{
			CNOVRNodeRemoveObject( cnovrstate->pRootNode, spinner_n[i] );
			//CNOVRDelete( spinner_n[i] );
		}
	}

	//OGCancelThread( thdmax );
	printf( "Example stop %s                   ---------------------%p %p\n", identifier, thdmax, &thdmax );
	printf( "End stop\n" );
}

