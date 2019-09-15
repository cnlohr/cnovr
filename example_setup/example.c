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

cnovr_shader * spinner_s;
cnovr_model * spinner_m;
cnovr_simple_node * spinner_n;


void * my_thread( void * v )
{
	while(1)
	{
		//printf( "THREADS 10 %s %p\n", v, thdmax );
		OGUSleep(1000000);
		if( 1 )
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

	spinner_n->pose.Scale = .1;
	spinner_n->pose.Pos[0] = sin( now - start );
	spinner_n->pose.Pos[2] = cos( now - start );
	cnovr_euler_angle e;
	e[0] = 0;
	e[1] = now-start;
	e[0] = 0;
	quatfromeuler( spinner_n->pose.Rot, e );

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

	spinner_n = CNOVRNodeCreateSimple( 1 );
	spinner_m = CNOVRModelCreate( 0, 3, GL_TRIANGLES );
	CNOVRModelAppendCube( spinner_m, 0 );
	spinner_s = CNOVRShaderCreate( "assets/basic" );
	CNOVRNodeAddObject( spinner_n, spinner_s );
	CNOVRNodeAddObject( spinner_n, spinner_m );
	CNOVRNodeAddObject( root, spinner_n );

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
	printf( "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Start stop\n" );
	if( node )
	{
		CNOVRNodeRemoveObject( cnovrstate->pRootNode, node );
		CNOVRNodeRemoveObject( cnovrstate->pRootNode, spinner_n );
		CNOVRDelete( node );
		CNOVRDelete( spinner_n );
	}

	//OGCancelThread( thdmax );
	printf( "Example stop %s                   ---------------------%p %p\n", identifier, thdmax, &thdmax );
	printf( "End stop\n" );
}

