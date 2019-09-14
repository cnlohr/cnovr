#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <stdlib.h>

og_thread_t thdmax;

cnovr_shader * shader;
cnovr_model * model;
cnovr_simple_node * node;

void * my_thread( void * v )
{
	while(1)
	{
		printf( "THREADS 9 %p %p\n", v, thdmax );
		OGUSleep(500000);
		if( 1 )
		{
			node->pose.Pos[0] = rand()%10 - 5;
			node->pose.Pos[1] = rand()%10 - 5;
			node->pose.Pos[2] = rand()%10 - 5;
		}
	}
	return 0;
}

void init( const char * identifier )
{
	ovrprintf( "Example init %s\n", identifier );
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
}


void start( const char * identifier )
{
	uint64_t stdr = (uint64_t)strdup(identifier);
	printf( "%p\n", stdr );
	printf( "MARRRR %p\n", stdr );
	printf( "MARRRR %s\n", stdr );
	thdmax = OGCreateThread( my_thread, (void*)stdr );
	printf( "Example start %s(%p)                   ++++++++++++++++++++%p %p\n", identifier, identifier, thdmax, &thdmax );

	CNOVRJobTack( cnovrQPrerender, example_scene_setup, 0, 0, 0 );

	printf( "Example start OK %s                   ++++++++++++++++++++%p %p\n", identifier, thdmax, &thdmax );
}

void stop( const char * identifier )
{
	printf( "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Start stop\n" );
	CNOVRNodeRemoveObject( cnovrstate->pRootNode, node );
	CNOVRDelete( shader );
	CNOVRDelete( model );
	CNOVRDelete( node );

	//OGCancelThread( thdmax );
	printf( "Example stop %s                   ---------------------%p %p\n", identifier, thdmax, &thdmax );
	printf( "End stop\n" );
}

