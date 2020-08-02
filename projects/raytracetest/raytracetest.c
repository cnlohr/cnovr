#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovrcanvas.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <stdlib.h>
#include <string.h>

const char * identifier;
double StartTime;
int shutting_down;

cnovr_shader * test;
cnovr_model * square;

struct staticstore
{
	int initialized;
	//cnovr_pose modelpose[MAX_CHAIRS];
	//cnovr_pose targetpose[MAX_CHAIRS];
	//float zapped[MAX_CHAIRS];
} * store;


void RenderFunction( void * tag, void * opaquev )
{
	glDisable(GL_CULL_FACE);
	CNOVRRender( test );
	//umModel = 4

	GLfloat matrix[16];
	memset( matrix, 0, sizeof( matrix ) );
	matrix[0] = 1;
	matrix[5] = 1;
	matrix[10] = 1;
	matrix[15] = 1;
	int uniform;
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( UNIFORMSLOT_MODEL ) ) != INVALIDUNIFORM )
		glUniformMatrix4fv( uniform, 1, 1, matrix );

//	glUniformMatrix4fv( 0, 1, 0, matrix );

	CNOVRRender( square );
}

void UpdateFunction( void * tag, void * opaquev )
{
	static double start;
	static double last;
	double now = OGGetAbsoluteTime();
	if( start < 1 ) { last = start = now; }
}


static void example_scene_setup( void * tag, void * opaquev )
{
	int i, j;

	UpdateFunction(0,0);
	CNOVRListAdd( cnovrLUpdate, 0, UpdateFunction );
	CNOVRListAdd( cnovrLRender2, 0, RenderFunction );

	printf( "+++ Example scene setup\n" );
}


void raytraceteststart( const char * identifier )
{
	store = CNOVRNamedPtrData( "raytracetest", 0, sizeof( *store ) + 1024 );
	printf( "=== Initializing %p\n", store );

	if( !store->initialized || 1 )
	{
		store->initialized = 1;
	}

	test = CNOVRShaderCreateWithPrefix( "test", "#define SOMETHING_SOMETHING" );

	square = CNOVRModelCreate( 0, GL_TRIANGLES );
	cnovr_point3d size = { 1, 1, 1 };
//	CNOVRModelAppendCube( square, size, 0, 0 );
	CNOVRModelAppendMesh( square, 1, 1, 0, (cnovr_point3d){ 1, 1, 1 }, 0, 0 /*extradata*/ );


	StartTime = OGGetAbsoluteTime();
	identifier = strdup(identifier);
	CNOVRJobTack( cnovrQPrerender, example_scene_setup, 0, 0, 0 );
	printf( "=== RayTraceTest start %s(%p) + %p %p\n", identifier, identifier );
}

void raytraceteststop( const char * identifier )
{
	shutting_down = 1;
	//OGCancelThread( thdmax );
	printf( "=== RayTraceTest stop\n" );
}



