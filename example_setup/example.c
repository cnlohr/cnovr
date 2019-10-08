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
cnovr_texture * texture;
cnovr_simple_node * node;

#define MAX_SPINNERS 100
cnovr_model * spinner_m[MAX_SPINNERS];
//cnovr_simple_node * spinner_n[MAX_SPINNERS];
int shutting_down;
cnovrfocus_capture focusblock[MAX_SPINNERS];

int draggingid[CNOVRINPUTDEVS] = { -1, -1, -1 };
cnovr_pose draggingpose[CNOVRINPUTDEVS]; //Pose of object relative to tip.


struct staticstore
{
	int initialized;
	cnovr_pose modelpose[MAX_SPINNERS];
	float zapped[MAX_SPINNERS];
} * store;


int ExampleFocusEvent( int event, cnovrfocus_capture * cap, cnovrfocus_properties * prop, int buttoninfo )
{
	//printf( "EVENT: %d %d %d\n", event, cap->opaque, buttoninfo );
	cnovr_model * m = (cnovr_model*)cap->opaque;
	int id = m->iOpaque;
	switch( event )
	{
		case CNOVRF_DOWNNOFOCUS:
			if( buttoninfo == 0 ) { store->zapped[id] = .999; return 0; } //Catpured event
			break;
		case CNOVRF_LOSTFOCUS:
			CNOVRNamedPtrSave( "examplecodestore" );
			break;
		case CNOVRF_ACQUIREDFOCUS:
			store->zapped[id] = 1;
			break;
	}

	CNOVRModelHandleFocusEvent( cap->opaque, prop, event, buttoninfo );

	if( event == CNOVRF_DRAG )
	{
		cnovr_pose * dragout = &store->modelpose[id];
		if(1)
		if( dragout->Rot[0] > .95 || dragout->Rot[0] < -.95 )
		{
			quatidentity( dragout->Rot );
			dragout->Pos[0] = floor( dragout->Pos[0] * 10. ) / 10.;
			dragout->Pos[1] = floor( dragout->Pos[1] * 10. ) / 10.;
			dragout->Pos[2] = floor( dragout->Pos[2] * 10. ) / 10.;
		}
	}
	return 0;
}



int FocusEvent( int event, cnovrfocus_capture * cap, cnovrfocus_properties * prop, int buttoninfo );

void * my_thread( void * v )
{
	return 0;
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
		double ang = (i * .4);// + (now - start)*.2*1;
		cnovr_pose * pose = &store->modelpose[i];
		float * zap = &store->zapped[i];
//		spinner_n[i]->pose.Pos[1] = 2;
		if( *zap >= 1.0 ) { continue; } //spinner_n[i]->pose.Scale = .4;
		if( *zap > 0 ) *zap -= .001;
		if( *zap < 0 ) *zap = 0;
		float z = *zap;
		pose->Pos[0] = cnovr_lerp( sin( ang ) * 6., pose->Pos[0], z );
		pose->Pos[1] = cnovr_lerp( sin( dt*.1) * 2., pose->Pos[1], z );
		pose->Pos[2] = cnovr_lerp( cos( ang ) * 6., pose->Pos[2], z );
		pose->Scale = cnovr_lerp( pose->Scale, .2, .01 );

		cnovr_euler_angle e;
		e[0] = 0;
		e[1] = 0; //add back ang
		e[2] = 0;
		cnovr_quat targetquat;
		quatfromeuler( targetquat, e );
		quatslerp( pose->Rot, targetquat, pose->Rot, z );
	}

	return;
}


void RenderFunction( void * tag, void * opaquev )
{
	int i;
	CNOVRRender( shader );
	CNOVRRender( texture );
	for( i = 0; i < MAX_SPINNERS; i++ )
	{
		//Texture?
		CNOVRRender( spinner_m[i] );
	}

}


static void example_scene_setup( void * tag, void * opaquev )
{
	int i;
	shader = CNOVRShaderCreate( "assets/example" );
/*
	cnovr_simple_node * root = cnovrstate->pRootNode;
	node = CNOVRNodeCreateSimple( 1 );
	model = CNOVRModelCreate( 0, 3, GL_TRIANGLES );
	CNOVRModelAppendCube( model, (cnovr_point3d){ 1.f, 1.f, 1.f }, 0, 0 );
	CNOVRNodeAddObject( node, shader );
	CNOVRNodeAddObject( node, model );
	CNOVRNodeAddObject( root, node );
*/
	printf( "SETUP\n" );

	for( i = 0; i < MAX_SPINNERS; i++ )
	{
		//spinner_n[i] = CNOVRNodeCreateSimple( 1 );
		spinner_m[i] = CNOVRModelCreate( 0, 4, GL_TRIANGLES );
		cnovr_point4d extra = { (rand()%256)/255.0, (rand()%256)/255.0, (rand()%256)/255.0, 0 };
		CNOVRModelAppendCube( spinner_m[i], (cnovr_point3d){ 1.f, 1.f, 1.f }, 0, &extra );
		spinner_m[i]->pose = &store->modelpose[i];
		spinner_m[i]->iOpaque = i;
		focusblock[i].opaque = spinner_m[i];
		focusblock[i].cb = ExampleFocusEvent;
		CNOVRModelSetInteractable( spinner_m[i], &focusblock[i] );
	}

	texture = CNOVRTextureCreate( 0, 0, 0 ); //Set to all 0 to have the load control these details.
	CNOVRTextureLoadFileAsync( texture, "test.png" );

	printf( "Create done\n" );

	UpdateFunction(0,0);
	CNOVRListAdd( cnovrLUpdate, 0, UpdateFunction );
	CNOVRListAdd( cnovrLRender, 0, RenderFunction );
	//CNOVRListAdd( cnovrLCollide, 0, CollideFunction );

	thdmax = OGCreateThread( my_thread, (void*)identifier );
}


void start( const char * identifier )
{

	store = CNOVRNamedPtrData( "examplecodestore", 0, sizeof( *store ) + 1024 );
	printf( "Initializing %p\n", store );
//	store->initialized = 0;
	if( !store->initialized )
	{
		int i;
		for( i = 0; i < MAX_SPINNERS; i++ )
		{
			pose_make_identity( &store->modelpose[i] );
			store->modelpose[i].Scale = .2;
			store->zapped[i] = 0;
		}
		store->initialized = 1;
	}

	identifier = strdup(identifier);
	printf( "Example start %s(%p) + %p %p\n", identifier, identifier );

	CNOVRJobTack( cnovrQPrerender, example_scene_setup, 0, 0, 0 );

	printf( "Example start OK %s + %p %p\n", identifier );
}

void stop( const char * identifier )
{
	shutting_down = 1;
	printf( "^^ Start stop\n" );
	if( node )
	{
		CNOVRNodeRemoveObject( cnovrstate->pRootNode, node );
	}

	//OGCancelThread( thdmax );
	printf( "Example stop %s ----%p %p\n", identifier, thdmax, &thdmax );
	printf( "End stop\n" );
}


