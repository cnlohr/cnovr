#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovrcanvas.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <stdlib.h>
#include <string.h>

#define NUM_TEMPLATES 3
int which_template;

cnovr_model * playareacollide;
cnovr_shader * shaderLines;
cnovr_pose    playareapose;
cnovr_shader * rendermodelshader;
cnovr_pose    shadow_pose[2];

cnovr_model * bean;

cnovr_shader * shaderBasic;
cnovr_shader * shaderUV;
cnovr_model * pointer_tile_place[2];
cnovr_model * pointer_tile[NUM_TEMPLATES];
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
	glDisable(GL_CULL_FACE);

	CNOVRRender( shaderLines );

	CNOVRRender( playareacollide );

	CNOVRRender( rendermodelshader );
	CNOVRRender( eightiessun );


	glLineWidth(10.);
	CNOVRRender( shaderUV );

	CNOVRRender( bean );

void glSampleMaski( 	GLuint maskNumber,
  	GLbitfield mask);

#define GL_SAMPLE_MASK 0x8E51
	glEnable(GL_SAMPLE_MASK);
	glSampleMaski(0, 0x3);

	CNOVRModelRenderWithPose( pointer_tile_place[0], &playareapose );
	CNOVRModelRenderWithPose( pointer_tile_place[1], &playareapose );

	CNOVRModelRenderWithPose( our_model, &playareapose );
	glDisable(GL_SAMPLE_MASK);
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
		static int last_mask[2];
		cnovrfocus_properties * prop = CNOVRFocusGetPropsForDev( controller + 1 );

		//Warp the thinger?
		{
			cnovr_model * pt = pointer_tile[which_template];
			cnovr_model * ptp = pointer_tile_place[controller];
			CNOVRModelClearMeshes( ptp );
			CNOVRModelSetNumVBOsWithStrides( ptp, 2, 3, 3 );
			CNOVRModelSetNumIndices( ptp, 0 );
			CNOVRModelResetMarks( ptp );
			CNOVRDelinateGeometry( ptp, "cube" );

			int i;
			int iStartIndex = ptp->pGeos[0]->iVertexCount;
			for( i = 0; i < pt->iIndexCount; i++ )
			{
				CNOVRModelTackIndex( ptp, 1, pt->pIndices[i] + iStartIndex );
			}
			for( i = 0; i < pt->pGeos[0]->iVertexCount; i++ )
			{
				cnovr_point3d xformedvert;
				cnovr_pose p;
				memcpy( &p, &prop->poseTip, sizeof( cnovr_pose ) );
				
				apply_pose_to_point( xformedvert, &p, (pt->pGeos[0]->pVertices) + i*3 );

				//Awful snap mode.
				//Find out if there's any other points nearby.
				int j;
				float bestdist = (1./4.) * .0254;
				for( j = 0; j < our_model->pGeos[0]->iVertexCount; j++ )
				{
					cnovr_point3d * trypt = (cnovr_point3d*)( our_model->pGeos[0]->pVertices + j * 3 );
					float dist = dist3d( *trypt, xformedvert );
					if( dist < bestdist )
					{
						copy3d( xformedvert, *trypt );
					}
				}
				CNOVRVBOTackv( ptp->pGeos[0], 3, xformedvert );
				CNOVRVBOTackv( ptp->pGeos[1], 3, &pt->pGeos[1]->pVertices[i*3] );
			}
			CNOVRVBOTaint( ptp->pGeos[0] );
			CNOVRVBOTaint( ptp->pGeos[1] );
			CNOVRModelTaintIndices( ptp );
		}

		uint64_t mask = prop->buttonmask[0];
		memcpy( &shadow_pose[controller], &prop->poseTip, sizeof( cnovr_pose ) );
		int Adown = mask & 2;
		if( Adown && !(last_mask[controller] & 2 ) )
		{
			which_template = (which_template+1)%NUM_TEMPLATES;
		}

		int down = mask & 1;
		if( down && !(last_mask[controller] & 1) )
		{
			cnovr_model * ptp = pointer_tile_place[controller];
			int i;
			int iStartIndex = our_model->pGeos[0]->iVertexCount;
			for( i = 0; i < ptp->iIndexCount; i++ )
			{
				CNOVRModelTackIndex( our_model, 1, ptp->pIndices[i] + iStartIndex );
			}
			for( i = 0; i < ptp->pGeos[0]->iVertexCount; i++ )
			{
				CNOVRVBOTackv( our_model->pGeos[0], 3, (ptp->pGeos[0]->pVertices) + i*3 );
				CNOVRVBOTackv( our_model->pGeos[1], 3, &ptp->pGeos[1]->pVertices[i*3] );
			}
			CNOVRVBOTaint( our_model->pGeos[0] );
			CNOVRVBOTaint( our_model->pGeos[1] );
			CNOVRModelTaintIndices( our_model );
		}
		last_mask[controller] = mask;
	}
}


static void example_scene_setup( void * tag, void * opaquev )
{
	int i, j;
	printf( "+++ Example scene setup\n" );

	shaderLines = CNOVRShaderCreateWithPrefix( "fakelines", "#define OPAQUIFY" );

	playareacollide = CNOVRModelCreate( 0, GL_TRIANGLES );
	playareacollide->pose = &playareapose;
	pose_make_identity( &playareapose );
	CNOVRModelLoadFromFileAsync( playareacollide, "playarea.obj:barytc" );
	playareacollide->iRenderMesh = 0;

	bean = CNOVRModelCreate( 0, GL_LINES );
	bean->pose = &playareapose;
	CNOVRModelLoadFromFileAsync( bean, "bean.obj:lineify" );

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
	shaderUV = CNOVRShaderCreate( "projects/oct4vr/surfaces" );

	cnovr_point3d vv1[] = { { 0, 0, 0 }, { 0.22, 0.39, 0 }, { -0.22, 0.39, 0 } };
	const float scale1 = 0.07;
	cnovr_point3d vv2[] = { { 0, 0, 0 }, { 0.43, 0.65, 0 }, { 0.22, 1.05, 0 }, { -0.22, 1.05, 0 }, { -0.43, 0.65, 0 } };
	const float scale2 = 0.07;
	cnovr_point3d vv3[] = { { 1.0, 0.0, 0 }, { 0.5, 0.866, 0 }, { -0.5, 0.866, 0 }, { -1.0, 0.0, 0 }, { -0.5, -0.866, 0 }, { 0.5, -0.866, 0 } };
	const float scale3 = 0.07;

	int vvct[3];
	cnovr_point3d * vvs[3] = { vv1, vv2, vv3 };
	float scales[] = { scale1, scale2, scale3 };
	vvct[0] = sizeof(vv1) / sizeof( vv1[0] );
	vvct[1] = sizeof(vv2) / sizeof( vv2[0] );
	vvct[2] = sizeof(vv3) / sizeof( vv3[0] );

	for( j = 0; j < 3; j++ )
		for( i = 0; i < vvct[j]; i++ ) scale3d( vvs[j][i], vvs[j][i], scales[j] );

	printf( "VVCT: %d\n", vvct );
	int pl;
	for( pl = 0; pl < 2; pl++ )
	{
		cnovr_model * m = pointer_tile_place[pl] = CNOVRModelCreate( 0, GL_TRIANGLES );
		CNOVRModelClearMeshes( m );
		CNOVRModelSetNumVBOsWithStrides( m, 2, 3, 3 );
		CNOVRModelSetNumIndices( m, 0 );
		CNOVRModelResetMarks( m );
		CNOVRDelinateGeometry( m, "cube" );
		CNOVRVBOTaint( m->pGeos[0] );
		CNOVRVBOTaint( m->pGeos[1] );
		CNOVRModelTaintIndices( m );
	}

	for( pl = 0; pl < NUM_TEMPLATES; pl++ )
	{
		cnovr_model * ptp = pointer_tile[pl] = CNOVRModelCreate( 0, GL_TRIANGLES );
		int vvctl = vvct[pl];
		CNOVRModelClearMeshes( ptp );
		CNOVRModelSetNumVBOsWithStrides( ptp, 2, 3, 3 );
		CNOVRModelSetNumIndices( ptp, 0 );
		CNOVRModelResetMarks( ptp );
		CNOVRDelinateGeometry( ptp, "cube" );
		CNOVRModelTackIndex( ptp, (vvctl-2)*3, 0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5, 0, 5, 6 );

		for( i = 0; i < vvctl;i++ )
		{
			CNOVRVBOTackv( ptp->pGeos[0], 3, vvs[pl][i] );
		}

		CNOVRVBOTackv( ptp->pGeos[1], 3, (float[3]){ 1, 0, 0 } );
		CNOVRVBOTackv( ptp->pGeos[1], 3, (float[3]){ 0, 1, 0 } );
		CNOVRVBOTackv( ptp->pGeos[1], 3, (float[3]){ 0, 0, 1 } );
		CNOVRVBOTackv( ptp->pGeos[1], 3, (float[3]){ 1, 0, 1 } );
		CNOVRVBOTackv( ptp->pGeos[1], 3, (float[3]){ 0, 1, 1 } );
		CNOVRVBOTackv( ptp->pGeos[1], 3, (float[3]){ 1, 1, 0 } );

		CNOVRVBOTaint( ptp->pGeos[0] );
		CNOVRVBOTaint( ptp->pGeos[1] );
		CNOVRModelTaintIndices( ptp );
	}

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



