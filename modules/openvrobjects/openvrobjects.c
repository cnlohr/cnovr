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

const char *  openvrobjectsidentifier;
cnovr_shader * openvrobjectsshader;

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

void openvrobjectsinit( const char * identifier )
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
	if( !nodetail )
	{
		for( i = start; i < MAXRNS; i++ )
		{
			struct renderpair * rp = &rps[i];
			if( rp->show_status || rp->hold_status )
			{
				CNOVRRender( rp->status );
			}
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


void OpenVRObjectsUpdateFunction( void * tag, void * opaquev )
{
	int i;
	int j = 2;	//variable for vive controllers
	for( i = 0; i < MAXRNS; i++ )
	{
		// o->trackedDeviceIndex
		struct renderpair * rp = &rps[i];
		if( cnovrstate->bRenderPosesValid[i] && !rp->loaded )
		{
			char * rmname = CNOVRGetTrackedDeviceString( i, ETrackedDeviceProperty_Prop_RenderModelName_String );
			rp->modellines = CNOVRModelCreate( 0, GL_TRIANGLES );
			rp->modelsolid = CNOVRModelCreate( 0, GL_TRIANGLES );
			char tempname[300];
			const char * dotrendermodel = ".rendermodel";
			if( strstr( rmname, "generic_hmd" ) != 0 )
			{
				rmname = "assets/indexvisor.obj";
				dotrendermodel = "";
			}
			sprintf( tempname, "%s%s:barytc", rmname, dotrendermodel );
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
			if( !nodetail )
				rp->status = CNOVRCanvasCreate( tempname, 96, 64, CANVAS_PROP_NO_INTERACT );
			//Allow mouse-over objects.
			cnovrfocus_capture * capture = malloc( sizeof( cnovrfocus_capture ) ); //Will be auto deleted.
			capture->tag = rp;
			capture->tcctag = GetTCCTag();
			capture->opaque = 0;
			capture->cb = ObjFocusEvent;
			CNOVRModelSetInteractable( rp->modelsimple, capture );
			if( strstr( rmname, "controller" ) )
			{
				if( strstr( rmname, "vive" ) )	//if we are using a vive, left and right doesn't exist
				{
					rp->modelsimple->focuscontrol->collide_mask = j;
					j = (j > 2?2:4);	//each time we go through this loop, cycle between 2 and 4
				}
				else
				{
					rp->modelsimple->focuscontrol->collide_mask = (strstr( rmname, "left" )?2:0) | (strstr( rmname, "right" )?4:0);
				}
			}
		}
	}
	
	for( i = 0; i < MAXRNS; i++ )
	{
		
		struct renderpair * rp = &rps[i];
		if( ( rp->show_status || rp->hold_status ) && !nodetail )
		{
			memcpy( rp->status->pose, &cnovrstate->pRenderPoses[i], sizeof( cnovr_pose ) );
			char statustext[1024];
			sprintf( statustext, "%s %d\nx %3.3f\ny %3.3f\nz %3.3f\n", cnovrstate->asTrackedDeviceSerialStrings[i], i, PFTHREE( cnovrstate->pRenderPoses[i].Pos ) );
			CNOVRCanvasClearFrame( rp->status );
			CNOVRCanvasDrawText( rp->status, 2, 2, statustext, 2 );
			CNOVRCanvasSwapBuffers( rp->status );
		}
	}

}



void OpenVRObjectsRenderFunction( void * tag, void * opaquev )
{
	int i;
	if( do_wireframe || do_lineify )
	{
		CNOVRRender( openvrobjectsshader );
		RenderObjects( 1 );
	}
	else
	{
		CNOVRRender( openvrobjectsshader );
		RenderObjects( 0 );
	}
}

void OpenVRObjectsSceneSetup( void * tag, void * opaquev )
{
	if( do_wireframe )
		openvrobjectsshader = CNOVRShaderCreateWithPrefix( "assets/fakelines", "#define OPAQUIFY" );
	else if( do_lineify )
		openvrobjectsshader = CNOVRShaderCreate( "assets/fakelines" );
	else
		openvrobjectsshader = CNOVRShaderCreate( "assets/rendermodel" );

	CNOVRListAdd( cnovrLRender2, openvrobjectsshader, OpenVRObjectsRenderFunction );
	CNOVRListAdd( cnovrLUpdate, openvrobjectsshader, OpenVRObjectsUpdateFunction );
}


void openvrobjectsstart( const char * identifier )
{	
	openvrobjectsidentifier = strdup( identifier );

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
	CNOVRJobTack( cnovrQPrerender, OpenVRObjectsSceneSetup, 0, 0, 0 );
	printf( "=== openvrobjects start %s(%p)\n", openvrobjectsidentifier, openvrobjectsidentifier );
}

void openvrobjectsstop( const char * identifier )
{
	printf( "=== openvrobjects stop\n" );
}


