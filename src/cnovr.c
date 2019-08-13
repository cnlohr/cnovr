#include <stdio.h>
#include <CNFGFunctions.h>
#include <chew.h>
#include <cnovr.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

struct VR_IVRSystem_FnTable * openvr;
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


int CNOVRInit( const char * appname, int screenx, int screeny )
{
	int r;

	ovrprintf( "Initializing Window.\n" );

	//Create Companion Window.
	if( ( r = CNFGSetup( appname, screenx, screeny ) ) )
	{
		return r;
	}

	ovrprintf( "Initializing OpenVR.\n" );

	EVRInitError e;
	uint32_t vrtoken = VR_InitInternal( &e, EVRApplicationType_VRApplication_Scene );
	if( !vrtoken )
	{
		ovrprintf( "Error calling VR_InitInternal: %d (%s)\n", e, VR_GetVRInitErrorAsEnglishDescription( e ) );
		return -e;
	}

	if ( ! VR_IsInterfaceVersionValid(IVRSystem_Version) )
	{
		ovrprintf( "OpenVR Interface Invalid.\n" );
		return -1;
	}

	char fnTableName[128];
	int result1 = sprintf(fnTableName, "FnTable:%s", IVRSystem_Version);
	openvr = (struct VR_IVRSystem_FnTable *)VR_GetGenericInterface(fnTableName, &e);
	ovrprintf( "OpenVR: %p (%d)\n", openvr, e );


	ovrprintf( "Initializing Extensions.\n" );

	chewInit();

	CNOVRCheck();

	{
		char strbuf[128];
		memset( strbuf, 0, sizeof( strbuf ) );
		r = openvr->GetStringTrackedDeviceProperty( k_unTrackedDeviceIndex_Hmd, ETrackedDeviceProperty_Prop_SerialNumber_String, strbuf, sizeof(strbuf)-1, 0 );
		ovrprintf( "Using HMD: %s\n", strbuf );
	}

	//OK, OpenVR is set up.  Now, set up rendering system.
	cnovrstate = malloc( sizeof( *cnovrstate ) );
	memset( cnovrstate, 0, sizeof( *cnovrstate ) );
	cnovrstate->fNear = 0.01;
	cnovrstate->fFar = 100.0;


	if( 1 )
	{
		glDebugMessageCallback( (GLDEBUGPROC)DebugCallback, 0 );
		glDebugMessageControl( GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, 0, GL_TRUE );
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	}



	return 0;
}

void CNOVRUpdate()
{
	//Possibly update stereo target resolutions.
	{
		int iRenderWidth, iRenderHeight;
		openvr->GetRecommendedRenderTargetSize( &iRenderWidth, &iRenderHeight );
		if( iRenderWidth != cnovrstate->iRTWidth || iRenderHeight != cnovrstate->iRTHeight )
		{
			CNOVRDelete( cnovrstate->sterotargets[0] );
			CNOVRDelete( cnovrstate->sterotargets[1] );
	 
			//Resize the render targets.
			cnovrstate->sterotargets[0] = CNOVRRFBufferCreate( iRenderWidth, iRenderHeight, 4 );
			cnovrstate->sterotargets[1] = CNOVRRFBufferCreate( iRenderWidth, iRenderHeight, 4 );
			cnovrstate->iRTWidth = iRenderWidth;
			cnovrstate->iRTHeight = iRenderHeight;
		}
	}

	//Update everything
	//Prerender everything
	//Render everything
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


void CNOVRAlert( cnovr_header * obj, int priority, const char * format, ... )
{
	va_list args;
	char buffer[BUFSIZ];
	va_start(args, format);
	vsnprintf(buffer, sizeof buffer, format, args);
	va_end(args);
	puts( buffer );

	//XXX TODO: Use asprintf
	//XXX TODO: Actually put warning somewhere.
}









