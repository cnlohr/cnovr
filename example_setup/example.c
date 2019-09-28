#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <stdlib.h>
#include <string.h>

og_thread_t thdmax;
const char * identifier;
cnovr_shader * shader;
cnovr_model * model;
cnovr_simple_node * node;

#define MAX_SPINNERS 200
cnovr_simple_node * spinner_n[MAX_SPINNERS];
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
		double dt =  (now - start)*.1 - i * .04;
		double ang = i * .04;
		spinner_n[i]->pose.Scale = .1;
		spinner_n[i]->pose.Pos[0] = sin( ang );
		spinner_n[i]->pose.Pos[1] = sin(dt*5.0);
		spinner_n[i]->pose.Pos[2] = cos( ang );
		cnovr_euler_angle e;
		e[0] = 0;
		e[1] = ang;
		e[2] = 0;
		quatfromeuler( spinner_n[i]->pose.Rot, e );
	}

	return;
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
		cnovr_model * spinner_m;
		spinner_m = CNOVRModelCreate( 0, 3, GL_TRIANGLES );
		CNOVRModelAppendCube( spinner_m, 0 );
		spinner_s = CNOVRShaderCreate( "assets/basic" );
		CNOVRNodeAddObject( spinner_n[i], spinner_s );
		CNOVRNodeAddObject( spinner_n[i], spinner_m );
		CNOVRNodeAddObject( root, spinner_n[i] );
	}

	CNOVRListAdd( cnovrLUpdate, 0, UpdateFunction );

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

