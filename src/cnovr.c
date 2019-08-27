#define _GNU_SOURCE
#include <stdio.h>
#include <CNFGFunctions.h>
#include <chew.h>
#include <cnovr.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "cnovrutil.h"
#include "cnovrparts.h"

struct cnovrstate_t  * cnovrstate;

#ifdef TCC
int __dso_handle;
#endif

S_API int VR_InitInternal( EVRInitError *peError, EVRApplicationType eType );
S_API const char * VR_GetVRInitErrorAsEnglishDescription( EVRInitError error );
S_API void VR_ShutdownInternal();
S_API bool VR_IsHmdPresent();
S_API bool VR_IsRuntimeInstalled();
S_API intptr_t VR_GetGenericInterface(const char *pchInterfaceVersion, EVRInitError *peError);
S_API bool VR_IsInterfaceVersionValid(const char *pchInterfaceVersion);




void HandleKey( int keycode, int bDown )
{
}

void HandleButton( int x, int y, int button, int bDown )
{
}

void HandleMotion( int x, int y, int mask )
{
}

void HandleDestroy()
{
}

void APIENTRY DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const char* message, const void* userParam)
{
	ovrprintf( "GL Error: %s\n", message );
}


int CNOVRInit( const char * appname, int screenx, int screeny, int allow_init_without_vr )
{
	int r;

	ovrprintf( "Initializing Window.\n" );

	//Create Companion Window.
	if( ( r = CNFGSetup( appname, screenx, screeny ) ) )
	{
		return r;
	}

	ovrprintf( "Initializing OpenVR.\n" );

	int has_vr = 0;

	EVRInitError e;
	uint32_t vrtoken = VR_InitInternal( &e, EVRApplicationType_VRApplication_Scene );
	if( !vrtoken )
	{
		ovrprintf( "Error calling VR_InitInternal: %d (%s)\n", e, VR_GetVRInitErrorAsEnglishDescription( e ) );
		if( !allow_init_without_vr )
			return -e;
	}
	else
	{
		has_vr = 1;
	}


	if( has_vr )
	{
		if ( ! VR_IsInterfaceVersionValid(IVRSystem_Version) )
		{
			ovrprintf( "OpenVR Interface Invalid.\n" );
			if( !allow_init_without_vr )
				return -1;
			has_vr = 0;
		}
	}


	//OK, OpenVR is set up.  Now, set up rendering system.
	cnovrstate = malloc( sizeof( *cnovrstate ) );
	memset( cnovrstate, 0, sizeof( *cnovrstate ) );
	cnovrstate->fNear = 0.01;
	cnovrstate->fFar = 100.0;
	cnovrstate->has_ovr = has_vr;
	cnovrstate->has_preview = 1;
	cnovrstate->iEyeRenderWidth = -1;
	cnovrstate->iEyeRenderHeight = -1;
	cnovrstate->iPreviewWidth = -1;
	cnovrstate->iPreviewHeight = -1;
	cnovrstate->sterotargets[0] = 0;
	cnovrstate->sterotargets[1] = 0;
	cnovrstate->previewtarget = 0;
	cnovrstate->fPreviewFOV = 55;
	pose_make_identity( &cnovrstate->pPreviewPose );

	char fnTableName[128];
	int result1 = sprintf(fnTableName, "FnTable:%s", IVRSystem_Version);
	cnovrstate->oSystem = (struct VR_IVRSystem_FnTable *)VR_GetGenericInterface(fnTableName, &e);
	ovrprintf( "OpenVR: %p (%d)\n", cnovrstate->oSystem, e );


	ovrprintf( "Initializing Extensions.\n" );

	chewInit();

	CNOVRCheck();

	if( has_vr )
	{
		char strbuf[128];
		memset( strbuf, 0, sizeof( strbuf ) );
		r = cnovrstate->oSystem->GetStringTrackedDeviceProperty( k_unTrackedDeviceIndex_Hmd, ETrackedDeviceProperty_Prop_SerialNumber_String, strbuf, sizeof(strbuf)-1, 0 );
		ovrprintf( "Using HMD: %s\n", strbuf );
	}


	if( 1 )
	{
		glDebugMessageCallback( (GLDEBUGPROC)DebugCallback, 0 );
		glDebugMessageControl( GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, 0, GL_TRUE );
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	}

	cnovrstate->pRootNode = CNOVRNodeCreateSimple( 1 );

	CNOVRJobInit();
	CNOVRInternalStartCacheSystem();

	return 0;
}

void CNOVRUpdate()
{
	//XXX TODO Waitgetposes

	//Possibly update stereo target resolutions.
	if( cnovrstate->has_ovr )
	{
		uint32_t iEyeRenderWidth, iEyeRenderHeight;
		cnovrstate->oSystem->GetRecommendedRenderTargetSize( &iEyeRenderWidth, &iEyeRenderHeight );
		if( iEyeRenderWidth != cnovrstate->iEyeRenderWidth || iEyeRenderHeight != cnovrstate->iEyeRenderHeight )
		{
			CNOVRDelete( cnovrstate->sterotargets[0] );
			CNOVRDelete( cnovrstate->sterotargets[1] );
	 
			//Resize the render targets.
			cnovrstate->sterotargets[0] = CNOVRRFBufferCreate( iEyeRenderWidth, iEyeRenderHeight, 4 );
			cnovrstate->sterotargets[1] = CNOVRRFBufferCreate( iEyeRenderWidth, iEyeRenderHeight, 4 );
			cnovrstate->iEyeRenderWidth = iEyeRenderWidth;
			cnovrstate->iEyeRenderHeight = iEyeRenderHeight;
		}
	}

	if( cnovrstate->has_preview )
	{
		short iPreviewWidth, iPreviewHeight;
		CNFGGetDimensions( &iPreviewWidth, &iPreviewHeight );
		if( iPreviewWidth != cnovrstate->iPreviewWidth || iPreviewHeight != cnovrstate->iPreviewHeight )
		{
			CNOVRDelete( cnovrstate->previewtarget );
			cnovrstate->previewtarget = CNOVRRFBufferCreate( iPreviewWidth, iPreviewHeight, 4 );
			cnovrstate->iPreviewWidth  = iPreviewWidth;
			cnovrstate->iPreviewHeight = iPreviewHeight;
		}
	}

	cnovr_simple_node * root = cnovrstate->pRootNode;

	//Scene Graph Pre-Render
	root->header.Update( root );

	CNOVRJobProcessQueueElement( cnovrQPrerender );
	root->header.Prerender( root );

	//Scene Graph Render
	if( cnovrstate->has_ovr )
	{
		int i;
		for( i = 0; i < 2; i++ )
		{
			//XXX TODO: handle getting eyes.

			//float width = cnovrstate->iRTWidth = cnovrstate->iEyeRenderWidth;
			//float height = cnovrstate->iRTHeight = cnovrstate->iEyeRenderHeight;
			//matrix44perspective( cnovrstate->mPerspective, cnovrstate->fPreviewFOV, width/height, cnovrstate->fNear, cnovrstate->fFar );
			//pose_to_matrix44( cnovrstate->mView, cnovrstate->pPreviewPose );

			if( !cnovrstate->sterotargets[i] ) continue;
			CNOVRFBufferActivate( cnovrstate->sterotargets[i] );
			cnovrstate->iRTWidth = cnovrstate->iEyeRenderWidth;
			cnovrstate->iRTHeight = cnovrstate->iEyeRenderHeight;
			root->header.Render( root );
			CNOVRFBufferDeactivate( cnovrstate->sterotargets[i] );
		}
		//XXX TODO: Submit eye buffers.
	}

	//XXX TODO: How do we know when we need to update the preview window?
	if( cnovrstate->has_preview )
	{
		int width = cnovrstate->iRTWidth = cnovrstate->iEyeRenderWidth;
		int height = cnovrstate->iRTHeight = cnovrstate->iEyeRenderHeight;
		matrix44perspective( cnovrstate->mPerspective, cnovrstate->fPreviewFOV, width/height, cnovrstate->fNear, cnovrstate->fFar );
		pose_to_matrix44( cnovrstate->mView, &cnovrstate->pPreviewPose );

		CNOVRFBufferActivate( cnovrstate->previewtarget );
		root->header.Render( root );
		CNOVRFBufferDeactivate( cnovrstate->previewtarget );
		glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST );
	}

	//XXX TODO: blit renderbuffer to frame.
}

int CNOVRCheck()
{
	GLenum e = glGetError();
	if( e != GL_NO_ERROR )
	{
		ovrprintf( "CNOVRCheck() -> %d -> ", e );
		if( e == GL_INVALID_ENUM ) ovrprintf( "GL_INVALID_ENUM\n" );
		if( e == GL_INVALID_VALUE ) ovrprintf( "GL_INVALID_VALUE\n" );
		if( e == GL_INVALID_OPERATION ) ovrprintf( "GL_INVALID_OPERATION\n" );
		if( e == GL_INVALID_FRAMEBUFFER_OPERATION ) ovrprintf( "GL_INVALID_FRAMEBUFFER_OPERATION\n" );
		if( e == GL_OUT_OF_MEMORY ) ovrprintf( "GL_OUT_OF_MEMORY\n" );
		if( e == GL_STACK_UNDERFLOW ) ovrprintf( "GL_STACK_UNDERFLOW\n" );
		if( e == GL_STACK_OVERFLOW ) ovrprintf( "GL_STACK_OVERFLOW\n" );
	}
	return e;
}


void CNOVRAlert( cnovr_model * obj, int priority, const char * format, ... )
{
	va_list args;
	va_start(args, format);
	//char buffer[BUFSIZ];
	//int len = vsnprintf(buffer, sizeof buffer, format, args);
	char * buffer = 0;
	int len = vasprintf( &buffer, format, args );
	va_end(args);
	if( len > 0 && buffer[len-1] == '\n' ) buffer[len-1] = 0;
	puts( buffer );
	free( buffer );
	//XXX TODO: Actually put warning somewhere.
}

void CNOVRShaderLoadedSetUniformsInternal()
{
	glUniformMatrix4fv( UNIFORMSLOT_MODEL, 1, 0, cnovrstate->mModel );
	glUniformMatrix4fv( UNIFORMSLOT_VIEW, 1, 0, cnovrstate->mView );
	glUniformMatrix4fv( UNIFORMSLOT_PERSPECTIVE, 1, 0, cnovrstate->mPerspective );
	glUniform4fv( UNIFORMSLOT_RENDERPROPS, 1, &cnovrstate->iRTWidth );
}






