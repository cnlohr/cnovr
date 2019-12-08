#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovropenvr.h>
#include <cnovr.h>
#include <string.h>
#include <cnovrutil.h>
#include <openvr_capi.h>
#include <chew.h>

const char *  identifier;
cnovr_shader * shaderlines;
cnovr_shader * shadersolid;
cnovr_shader * shaderrendermodel;

#define MAXRNS MAX_POSES_TO_PULL_FROM_OPENVR

struct renderpair
{
	int loaded;
	cnovr_model * modellines;
	cnovr_model * modelsolid;
};

struct renderpair rps[MAXRNS];

int do_wireframe;

void init( const char * identifier )
{
}

static void RenderObjects( int lines )
{
	int i;
	if( lines )
	{
		for( i = 1; i < MAXRNS; i++ )
		{
			struct renderpair * rp = &rps[i];
			if( rp->loaded && cnovrstate->bRenderPosesValid[i] )
				CNOVRModelRenderWithPose( rp->modellines, &cnovrstate->pRenderPoses[i] );
		}
	}
	else
	{
		for( i = 1; i < MAXRNS; i++ )
		{
			struct renderpair * rp = &rps[i];
			if( rp->loaded && cnovrstate->bRenderPosesValid[i] )
				CNOVRModelRenderWithPose( rp->modelsolid, &cnovrstate->pRenderPoses[i] );
		}
	}
}


	//		rp->modellines->pose = &cnovrstate->pRenderPoses[i];
	//		rp->modelsolid->pose = &cnovrstate->pRenderPoses[i];

static void UpdateFunction( void * tag, void * opaquev )
{
	int i;

	for( i = 0; i < MAXRNS; i++ )
	{
		// o->trackedDeviceIndex
		struct renderpair * rp = &rps[i];
		if( cnovrstate->bRenderPosesValid[i] && !rp->loaded )
		{
			char * rmname = CNOVRGetTrackedDeviceString( i, ETrackedDeviceProperty_Prop_RenderModelName_String );
			rp->modellines = CNOVRModelCreate( 0, GL_LINES );
			rp->modelsolid = CNOVRModelCreate( 0, GL_TRIANGLES );
			printf( "%d\n", i );
			char tempname[300];
			sprintf( tempname, "%s.rendermodel:lineify", rmname );
			CNOVRModelLoadFromFileAsync( rp->modellines, tempname);
			sprintf( tempname, "%s.rendermodel", rmname );
			CNOVRModelLoadFromFileAsync( rp->modelsolid, tempname );
			rps[i].loaded = 1;

#if 0
							if( FOCUS.rendermodelnames[ctrl] ) free(  FOCUS.rendermodelnames[ctrl] );
							rmname = FOCUS.rendermodelnames[ctrl] = strdup( rmname );
							cnovr_model * m = FOCUS.mdlRenderModels[ctrl];
							if( !m ) m = FOCUS.mdlRenderModels[ctrl] = CNOVRModelCreate( 0, GL_TRIANGLES );
							char rmname2[256];
							sprintf( rmname2, "%s.rendermodel", rmname );
							CNOVRModelLoadFromFileAsync( m, rmname2 );
#endif
			//Actually load.
			//rp->modellines
		#if 0
			char * rmname = CNOVRGetTrackedDeviceString( o->trackedDeviceIndex, ETrackedDeviceProperty_Prop_RenderModelName_String );
			if( rmname && (!FOCUS.rendermodelnames[ctrl] || strcmp( FOCUS.rendermodelnames[ctrl], rmname ) != 0 ) )
			{
				int rmlen = strlen( rmname );
			}
			printf( "...\n" );
		#endif
#if 0
			CNOVRPoseFromHMDMatrix( &FOCUS.poseController[ctrl], &p->pose.mDeviceToAbsoluteTracking );
			InputOriginInfo_t * o = &FOCUS.originInfo[ctrl];

			if ( cnovrstate->oInput->GetOriginTrackedDeviceInfo( p->activeOrigin, o, sizeof( *o ) ) == EVRInputError_VRInputError_None 
				&& o->trackedDeviceIndex != k_unTrackedDeviceIndexInvalid )
			{
				if( FOCUS.rendermodelnames[ctrl] == 0 || FOCUS.bShowController[ctrl] == 0 )
				{
					char * rmname = CNOVRGetTrackedDeviceString( o->trackedDeviceIndex, ETrackedDeviceProperty_Prop_RenderModelName_String );
					if( rmname && (!FOCUS.rendermodelnames[ctrl] || strcmp( FOCUS.rendermodelnames[ctrl], rmname ) != 0 ) )
					{
						int rmlen = strlen( rmname );
						if( rmlen < 128 )
						{
							if( FOCUS.rendermodelnames[ctrl] ) free(  FOCUS.rendermodelnames[ctrl] );
							rmname = FOCUS.rendermodelnames[ctrl] = strdup( rmname );
							cnovr_model * m = FOCUS.mdlRenderModels[ctrl];
							if( !m ) m = FOCUS.mdlRenderModels[ctrl] = CNOVRModelCreate( 0, GL_TRIANGLES );
						}
					}
				}
			}
			FOCUS.bShowController[ctrl] = 1;
#endif
#if 0
#endif

		}
	}
}

static void RenderFunction( void * tag, void * opaquev )
{
	int i;
	if( do_wireframe )
	{
		CNOVRRender( shadersolid );
		RenderObjects( 0 );
		CNOVRRender( shaderlines );
		RenderObjects( 1 );
	}
	else
	{
		CNOVRRender( shaderrendermodel );
		RenderObjects( 0 );
	}
}

static void scene_setup( void * tag, void * opaquev )
{
	shaderlines = CNOVRShaderCreate( "assets/basic" );
	shadersolid = CNOVRShaderCreate( "assets/blackmask" );
	shaderrendermodel = CNOVRShaderCreate( "assets/rendermodel" );

	CNOVRListAdd( cnovrLRender2, 0, RenderFunction );
	CNOVRListAdd( cnovrLUpdate, 0, UpdateFunction );

#if 0
	modelcameralines = CNOVRModelCreate( 0, GL_LINES );
	modelcamerasolid = CNOVRModelCreate( 0, GL_TRIANGLES );
	modelcameralines->pose = &store->posecamera;
	modelcamerasolid->pose = &store->posecamera;
	CNOVRModelLoadFromFileAsync( modelcameralines, "camera.obj:lineify" );
	CNOVRModelLoadFromFileAsync( modelcamerasolid, "camera.obj" );

	capture.tag = 0;
	capture.opaque = 0;
	capture.cb = CameraFocusEvent;
	CNOVRModelSetInteractable( modelcamerasolid, &capture );

	if( !store->initialized )
	{
		printf( "initializing camera\n" );
		pose_make_identity( &store->posecamera );
		store->initialized = 1;
	}
	UpdateCamera();
#endif
//	printf( "...loaded %f %f %f  %f %f %f %f\n", PFTHREE( store->posecamera.Pos ), PFFOUR( store->posecamera.Rot ) );
}


void start( const char * identifier )
{
	if( strstr( identifier, "lineify" ) != 0 )
	{
		printf( "Wireframe objects\n" );
		do_wireframe = 1;
	}
	identifier = strdup(identifier);
	CNOVRJobTack( cnovrQPrerender, scene_setup, 0, 0, 0 );
	printf( "=== Example start %s(%p) + %p %p\n", identifier, identifier );

}

void stop( const char * identifier )
{
	printf( "=== End Example stop\n" );
}


