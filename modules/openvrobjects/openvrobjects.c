#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovrcanvas.h>
#include <cnovropenvr.h>
#include <cnovr.h>
#include <string.h>
#include <cnovrutil.h>
#include <openvr_capi.h>
#include <stdio.h>
#include <stdlib.h>
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
	cnovr_model * modelsimple;
	cnovr_canvas * status;
	int show_status;
	int hold_status;
};


struct renderpair rps[MAXRNS];

int nodetail;
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
				CNOVRRender( rp->modellines );
		}
	}
	else
	{
		for( i = start; i < MAXRNS; i++ )
		{
			struct renderpair * rp = &rps[i];
			if( rp->loaded && cnovrstate->bRenderPosesValid[i] )
				CNOVRRender( rp->modelsolid );
		}
	}
	for( i = start; i < MAXRNS; i++ )
	{
		struct renderpair * rp = &rps[i];
		if( rp->show_status || rp->hold_status )
		{
			CNOVRRender( rp->status );
		}
	}
}


int ObjFocusEvent( int event, cnovrfocus_capture * cap, cnovrfocus_properties * prop, int buttoninfo )
{
	#ifdef SECTIONDEBUG
		printf( "CameraFocusEvent start\n" );
	#endif
	
	struct renderpair * rp = ((struct renderpair*)cap->tag);
	
	if( event == CNOVRF_IN )
	{
		rp->show_status = 1;
	}
	if( event == CNOVRF_OUT )
	{
		rp->show_status = 0;
	}
	if( event == CNOVRF_DOWNNOFOCUS && buttoninfo == 0 )
	{
		rp->hold_status = !rp->hold_status;
	}
	return -1;
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
			rp->modellines->pose = &cnovrstate->pRenderPoses[i];
			rp->modelsolid->pose = &cnovrstate->pRenderPoses[i];
			rp->modelsimple = CNOVRModelCreate( 0, GL_TRIANGLES );
			cnovr_point3d size = { .05, .05, .05 };
			CNOVRModelAppendCube( rp->modelsimple, size, 0, 0  );
			rp->modelsimple->pose = &cnovrstate->pRenderPoses[i];
			
			sprintf( tempname, "%s-%s-status", cnovrstate->asTrackedDeviceSerialStrings[i], rmname);
			rp->status = CNOVRCanvasCreate( tempname, 96, 64, CANVAS_PROP_NO_INTERACT );
			//Allow mouse-over objects.
			cnovrfocus_capture * capture = malloc( sizeof( cnovrfocus_capture ) ); //Will be auto deleted.
			capture->tag = rp;
			capture->tcctag = GetTCCTag();
			capture->opaque = 0;
			capture->cb = ObjFocusEvent;
			CNOVRModelSetInteractable( rp->modelsimple, capture );
			if( strstr( rmname, "controller" ) )
				rp->modelsimple->focuscontrol->collide_mask = (strstr( rmname, "left" )?2:0) | (strstr( rmname, "right" )?4:0);
		}
	}
	
	for( i = 0; i < MAXRNS; i++ )
	{
		
		struct renderpair * rp = &rps[i];
		if( ( rp->show_status || rp->hold_status ) && !nodetail )
		{
			memcpy( rp->status->pose, &cnovrstate->pRenderPoses[i], sizeof( cnovr_pose ) );
			char statustext[1024];
			sprintf( statustext, "%s %d\nx %3.3f\n%y %3.3f\nz %3.3f\n", cnovrstate->asTrackedDeviceSerialStrings[i], i, PFTHREE( cnovrstate->pRenderPoses[i].Pos ) );
			CNOVRCanvasClearFrame( rp->status );
			CNOVRCanvasDrawText( rp->status, 2, 2, statustext, 2 );
			CNOVRCanvasSwapBuffers( rp->status );
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
	if( strstr( identifier, "nodetail" ) != 0 )
	{
		printf( "no detail on OpenVR Objects\n" );
		nodetail = 1;
	}
	identifier = strdup(identifier);
	CNOVRJobTack( cnovrQPrerender, scene_setup, 0, 0, 0 );
	printf( "=== Example start %s(%p) + %p %p\n", identifier, identifier );
}

void stop( const char * identifier )
{
	printf( "=== End Example stop\n" );
}


