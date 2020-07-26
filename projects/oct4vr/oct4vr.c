#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovrcanvas.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <stdlib.h>
#include <string.h>


cnovr_model * playareacollide;
cnovr_shader * shaderLines;
cnovr_pose    playareapose;
cnovr_shader * rendermodelshader;

cnovr_shader * shaderBasic;
cnovr_shader * shaderUV;
cnovr_model * pointer_tile;
cnovr_model * our_model;


cnovr_pose    eightiessunpose;
cnovr_model * eightiessun;

const char * identifier;
double StartTime;
int shutting_down;

struct staticstore
{
	int initialized;
	//cnovr_pose modelpose[MAX_CHAIRS];
	//cnovr_pose targetpose[MAX_CHAIRS];
	//float zapped[MAX_CHAIRS];
} * store;


void RenderFunction( void * tag, void * opaquev )
{
	CNOVRRender( shaderLines );

	CNOVRRender( rendermodelshader );
	CNOVRRender( eightiessun );

	glDisable(GL_CULL_FACE);

	CNOVRRender( shaderUV );
	cnovr_pose * ptr = CNOVRFocusGetTipPose(1);
	CNOVRModelRenderWithPose( pointer_tile, ptr );
	CNOVRModelRenderWithPose( our_model, &playareapose );
}

void UpdateFunction( void * tag, void * opaquev )
{
	static double start;
	static double last;
	double now = OGGetAbsoluteTime();
	if( start < 1 ) { last = start = now; }

	int controller = 0;
	for( controller = 0; controller < 2; controller++ )
	{
		static int was_down[2];
		cnovrfocus_properties * prop = CNOVRFocusGetPropsForDev( controller );
		int down = prop->buttonmask[0] & 1;
		if( down && !was_down[controller] )
		{
			cnovr_pose * ptr = CNOVRFocusGetTipPose(1);
			int i;
			for( i = 0; i < pointer_tile->iIndexCount; i++ )
			{
				CNOVRModelTackIndex( our_model, 1, our_model->iIndexCount );
				printf( "INDICES: %d\n", our_model->iIndexCount );
			}
			for( i = 0; i < pointer_tile->pGeos[0]->iVertexCount; i++ )
			{
				cnovr_point3d xformedvert;
				apply_pose_to_point( xformedvert, ptr, ((cnovr_point3d)pointer_tile->pGeos[0]->pVertices) + i );
				printf( "%f %f %f\n", xformedvert[0], xformedvert[1], xformedvert[2] );

				CNOVRVBOTackv( our_model->pGeos[0], 3, xformedvert );
				CNOVRVBOTackv( our_model->pGeos[1], 3, &pointer_tile->pGeos[1]->pVertices[i*3] );
			}
			CNOVRVBOTaint( our_model->pGeos[0] );
			CNOVRVBOTaint( our_model->pGeos[1] );
			CNOVRModelTaintIndices( our_model );
		}
		was_down[controller] = down;
		}
}


static void example_scene_setup( void * tag, void * opaquev )
{
	printf( "+++ Example scene setup\n" );

	shaderLines = CNOVRShaderCreateWithPrefix( "fakelines", "#define OPAQUIFY" );

	playareacollide = CNOVRModelCreate( 0, GL_TRIANGLES );
	playareacollide->pose = &playareapose;
	pose_make_identity( &playareapose );
	CNOVRModelLoadFromFileAsync( playareacollide, "playarea.obj:barytc" );
	playareacollide->iRenderMesh = 0;

	rendermodelshader = CNOVRShaderCreate( "assets/rendermodelnearestaa" );

	eightiessun = CNOVRModelCreate( 0, GL_TRIANGLES );
	cnovr_point3d eightiessunsize = { 1, 1, 1 };
	CNOVRModelAppendMesh( eightiessun, 2, 2, 1, eightiessunsize ,0, 0 );
	pose_make_identity( &eightiessunpose );
	eightiessunpose.Pos[2] = -102;
	eightiessunpose.Pos[1] = 17;
	eightiessunpose.Scale = 10;
	eightiessun->pose = &eightiessunpose;
	CNOVRModelApplyTextureFromFileAsync( eightiessun, "80sSun.png" );


	shaderBasic = CNOVRShaderCreate( "basic" );
	shaderUV = CNOVRShaderCreate( "uvcolors" );

	pointer_tile = CNOVRModelCreate( 0, GL_TRIANGLES );
	//pointer_tile->pose = &playareapose;
	CNOVRModelClearMeshes( pointer_tile );


	CNOVRModelSetNumVBOsWithStrides( pointer_tile, 2, 3, 3 );
	CNOVRModelSetNumIndices( pointer_tile, 0 );
	CNOVRModelResetMarks( pointer_tile );
	CNOVRDelinateGeometry( pointer_tile, "cube" );

	CNOVRModelTackIndex( pointer_tile, 3, 0, 1, 2 );

	float v1[] = { 0, 0, 0 };
	float v2[] = { .07184, .12443, 0 };
	float v3[] = { -.07184, .12443, 0 };

	CNOVRVBOTackv( pointer_tile->pGeos[0], 3, v1 );
	CNOVRVBOTackv( pointer_tile->pGeos[0], 3, v2 );
	CNOVRVBOTackv( pointer_tile->pGeos[0], 3, v3 );

	CNOVRVBOTackv( pointer_tile->pGeos[1], 3, (float[3]){ 1, 0, 0 } );
	CNOVRVBOTackv( pointer_tile->pGeos[1], 3, (float[3]){ 0, 1, 0 } );
	CNOVRVBOTackv( pointer_tile->pGeos[1], 3, (float[3]){ 0, 0, 1 } );

	CNOVRVBOTaint( pointer_tile->pGeos[0] );
	CNOVRVBOTaint( pointer_tile->pGeos[1] );
	CNOVRModelTaintIndices( pointer_tile );


	our_model = CNOVRModelCreate( 0, GL_TRIANGLES );
	CNOVRModelClearMeshes( our_model );
	CNOVRModelSetNumVBOsWithStrides( our_model, 2, 3, 3 );
	CNOVRModelSetNumIndices( our_model, 0 );
	CNOVRModelResetMarks( our_model );
	CNOVRDelinateGeometry( our_model, "our_model" );

	//CNOVRVBOTackm->pGeos[0]

//	texture = CNOVRTextureCreate( 0, 0, 0 ); //Set to all 0 to have the load control these details.
//	CNOVRTextureLoadFileAsync( texture, "test.png" );

	UpdateFunction(0,0);
	CNOVRListAdd( cnovrLUpdate, 0, UpdateFunction );
	CNOVRListAdd( cnovrLRender2, 0, RenderFunction );
	//CNOVRListAdd( cnovrLCollide, 0, CollideFunction );
	printf( "+++ Example scene setup complete\n" );
}


void oct4vrstart( const char * identifier )
{
	store = CNOVRNamedPtrData( "cyber_chair_stacker_store_v2", 0, sizeof( *store ) + 1024 );
	printf( "=== Initializing %p\n", store );

	if( !store->initialized || 1 )
	{
		/*
		int i;
		for( i = 0; i < MAX_CHAIRS; i++ )
		{
			pose_make_identity( &store->modelpose[i] );
			pose_make_identity( &store->targetpose[i] );
			int row = i / 8;
			int col = (i % 8);
			if( col >= 4 ) col += 1;
			col -= 4;
			store->targetpose[i].Pos[2] = row - 4. + CHAIROFFSETINITIAL;
			store->targetpose[i].Pos[0] = (col)*.5; //into or out of the screen			
			store->zapped[i] = 0;
			store->targetpose[i].Scale = CHAIRSCALE;
			store->modelpose[i].Scale = CHAIRSCALE;
		}
		*/
		store->initialized = 1;
	}
	
	StartTime = OGGetAbsoluteTime();
	identifier = strdup(identifier);
	CNOVRJobTack( cnovrQPrerender, example_scene_setup, 0, 0, 0 );
	printf( "=== Oct4VR start %s(%p) + %p %p\n", identifier, identifier );
}

void oct4vrstop( const char * identifier )
{
	shutting_down = 1;
	//OGCancelThread( thdmax );
	printf( "=== Oct4VR stop\n" );
}



