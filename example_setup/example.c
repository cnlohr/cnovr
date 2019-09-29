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
int zapped[MAX_SPINNERS];
int shutting_down;

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
	for (i = 0; i < MAX_SPINNERS; i++ )
	{
		double dt =  (now - start)*.2 - i * 3.14159;
		double ang = (i * .4) + (now - start)*.2;
		spinner_n[i]->pose.Scale = .2;
//		spinner_n[i]->pose.Pos[1] = 2;
		if( zapped[i] ) spinner_n[i]->pose.Scale = .4;
		spinner_n[i]->pose.Pos[0] = sin( ang );
		spinner_n[i]->pose.Pos[1] = sin( dt*1.25);
		spinner_n[i]->pose.Pos[2] = cos( ang );
		cnovr_euler_angle e;
		e[0] = 0;
		e[1] = 0; //add back ang
		e[2] = 0;
		quatfromeuler( spinner_n[i]->pose.Rot, e );
	}

	return;
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
		sub3d( direction, start, direction );
		cnovr_collide_results res;
		res.t = 1000;
		int r = CNOVRModelCollide( spinner_m[i], start, direction, &res );
		if( r >= 0 )
		{
			printf( "%d %f %d %d [%f %f %f]\n", r, res.t, res.whichmesh, res.whichvert, res.collidepos[0], res.collidepos[1], res.collidepos[2] );
			printf( "zapped %d\n", i );
			zapped[i] = 1;
		}
	}
	//printf( "%f\n", p->poseTip.Pos[0] );
	//spinner_n[i]->pose
}

static void example_scene_setup( void * tag, void * opaquev )
{
	cnovr_simple_node * root = cnovrstate->pRootNode;
	node = CNOVRNodeCreateSimple( 1 );
	model = CNOVRModelCreate( 0, 3, GL_TRIANGLES );
	CNOVRModelAppendCube( model, 0 );
	shader = CNOVRShaderCreate( "assets/basic" );
	CNOVRNodeAddObject( node, shader );
	CNOVRNodeAddObject( node, model );
	CNOVRNodeAddObject( root, node );

	cnovr_shader * spinner_s;

	int i;
	for( i = 0; i < MAX_SPINNERS; i++ )
	{
		spinner_n[i] = CNOVRNodeCreateSimple( 1 );
		spinner_m[i] = CNOVRModelCreate( 0, 3, GL_TRIANGLES );
		CNOVRModelAppendCube( spinner_m[i], 0 );
		CNOVRNodeAddObject( spinner_n[i], spinner_m[i] );
		CNOVRNodeAddObject( root, spinner_n[i] );
	}
	UpdateFunction(0,0);
	CNOVRListAdd( cnovrLUpdate, 0, UpdateFunction );
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

