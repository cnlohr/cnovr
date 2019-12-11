#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovropenvr.h>
#include <cnovr.h>
#include <string.h>
#include <cnovrutil.h>
#include <openvr_capi.h>
#include <stdio.h>
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
int do_lineify;

void init( const char * identifier )
{
}

void RenderObjects( int lines )
{
	int i;
	//Don't render HMD in the HMD.
	int start = (cnovrstate->eyeTarget<2)?1:0;
	if( lines )
	{
		for( i = start; i < MAXRNS; i++ )
		{
			struct renderpair * rp = &rps[i];
			if( rp->loaded && cnovrstate->bRenderPosesValid[i] )
				CNOVRModelRenderWithPose( rp->modellines, &cnovrstate->pRenderPoses[i] );
		}
	}
	else
	{
		for( i = start; i < MAXRNS; i++ )
		{
			struct renderpair * rp = &rps[i];
			if( rp->loaded && cnovrstate->bRenderPosesValid[i] )
				CNOVRModelRenderWithPose( rp->modelsolid, &cnovrstate->pRenderPoses[i] );
		}
	}
}

void UpdateFunction( void * tag, void * opaquev )
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
			char tempname[300];
			const char * dotrendermodel = ".rendermodel";
			if( strstr( rmname, "generic_hmd" ) != 0 )
			{
				rmname = "assets/indexvisor.obj";
				dotrendermodel = "";
			}
			sprintf( tempname, "%s%s:lineify", rmname, dotrendermodel );
			CNOVRModelLoadFromFileAsync( rp->modellines, tempname);
			sprintf( tempname, "%s%s", rmname, dotrendermodel );
			CNOVRModelLoadFromFileAsync( rp->modelsolid, tempname );
			rps[i].loaded = 1;
		}
	}
}

void RenderFunction( void * tag, void * opaquev )
{
	int i;
	if( do_wireframe || do_lineify )
	{
		if( do_lineify )
		{
			CNOVRRender( shadersolid );
			RenderObjects( 0 );
		}
		CNOVRRender( shaderlines );
		RenderObjects( 1 );
	}
	else
	{
		CNOVRRender( shaderrendermodel );
		RenderObjects( 0 );
	}
}

void scene_setup( void * tag, void * opaquev )
{
	shaderlines = CNOVRShaderCreate( "assets/basic" );
	shadersolid = CNOVRShaderCreate( "assets/blackmask" );
	shaderrendermodel = CNOVRShaderCreate( "assets/rendermodel" );

	CNOVRListAdd( cnovrLRender2, 0, RenderFunction );
	CNOVRListAdd( cnovrLUpdate, 0, UpdateFunction );

}


void start( const char * identifier )
{
	if( strstr( identifier, "wireframe" ) != 0 )
	{
		printf( "Wireframe objects\n" );
		do_wireframe = 1;
	}
	if( strstr( identifier, "lineify" ) != 0 )
	{
		printf( "Lineify objects\n" );
		do_lineify = 1;
	}
	identifier = strdup(identifier);
	CNOVRJobTack( cnovrQPrerender, scene_setup, 0, 0, 0 );
	printf( "=== Example start %s(%p) + %p %p\n", identifier, identifier );
}

void stop( const char * identifier )
{
	printf( "=== End Example stop\n" );
}


