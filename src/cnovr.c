#define _GNU_SOURCE
#include <stdio.h>
#include <CNFG.h>
#include <cnovr.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "ovrchew.h"
#include "cnovrutil.h"
#include "cnovrparts.h"
#include "cnovrtcc.h"
#include "cnovrtccinterface.h"

struct cnovrstate_t  * cnovrstate;


//If on TCC, anything that linked to something that needs C++, i.e. OpenVR will require this symbol to be defined.
#ifdef __TINYC__
int __dso_handle;
#endif

void InternalFileSearchShutdown();
void CNOVRInternalStartCacheSystem();
void CNOVRInternalStopCacheSystem();
void CNOVRJobInit(); //Internal
void CNOVRJobStop(); //Internal
void CNOVRListSystemInit(); //internal
void CNOVRListSystemDestroy(); //internal
void InternalSetupTCCInterface();
void CNOVRInternalSetupFreeLaterSet();
void CNOVRStopTCCSystem();
void CNOVRFreeLaterShutdown();
void InternalBreakdownRestOfTCCInterface();
void StopFileTimeChekerThread();
void InternalCNOVRFocusSetup();
void InternalCNOVRFocusShutdown();
void InternalCNOVRFocusUpdate();
void InternalSetupNamedPtrs();

#define DEFAULT_MULTISAMPLE 4

// Bit-mapping:
// ---- ---- -Z[Shift][Space] DSAW
// KeyboardState
void HandleKey( int keycode, int bDown )
{
	if( !cnovrstate ) return;
	if( (keycode == 27 || keycode == 65307) && bDown ) CNOVRShutdown( );
	printf( "!.. %d\n", keycode );

	switch(keycode)
	{
		case 0x77: bDown ? (cnovrstate->KeyboardState |= 0x0001) : (cnovrstate->KeyboardState &= 0xFFFE); break; // W
		case 0x61: bDown ? (cnovrstate->KeyboardState |= 0x0002) : (cnovrstate->KeyboardState &= 0xFFFD); break; // A
		case 0x73: bDown ? (cnovrstate->KeyboardState |= 0x0004) : (cnovrstate->KeyboardState &= 0xFFFB); break; // S
		case 0x64: bDown ? (cnovrstate->KeyboardState |= 0x0008) : (cnovrstate->KeyboardState &= 0xFFF7); break; // D
		case 0x20: bDown ? (cnovrstate->KeyboardState |= 0x0010) : (cnovrstate->KeyboardState &= 0xFFEF); break; // Space
		case 0x10: bDown ? (cnovrstate->KeyboardState |= 0x0020) : (cnovrstate->KeyboardState &= 0xFFDF); break; // Shift
		case 0x7A: bDown ? (cnovrstate->KeyboardState |= 0x0040) : (cnovrstate->KeyboardState &= 0xFFBF); break; // Z
	}
}

// Called from CNOVRUpdate to move the preview camera according to the current mouse and keyboard input.
void HandleHIDInput()
{
	if( !cnovrstate ) return;
	float MovementQty = 0.005;
	if((cnovrstate->KeyboardState & 0x0040) == 0x0040) { MovementQty = 0.03; } // Hold Z for speed

	if((cnovrstate->KeyboardState & 0x0001) == 0x0001) { cnovrstate->pPreviewPose.Pos[2] += MovementQty; }
	if((cnovrstate->KeyboardState & 0x0004) == 0x0004) { cnovrstate->pPreviewPose.Pos[2] -= MovementQty; }
	if((cnovrstate->KeyboardState & 0x0002) == 0x0002) { cnovrstate->pPreviewPose.Pos[0] += MovementQty; }
	if((cnovrstate->KeyboardState & 0x0008) == 0x0008) { cnovrstate->pPreviewPose.Pos[0] -= MovementQty; }
	if((cnovrstate->KeyboardState & 0x0010) == 0x0010) { cnovrstate->pPreviewPose.Pos[1] -= MovementQty; }
	if((cnovrstate->KeyboardState & 0x0020) == 0x0020) { cnovrstate->pPreviewPose.Pos[1] += MovementQty; }
}

void HandleButton( int x, int y, int button, int bDown )
{
}

void HandleMotion( int x, int y, int mask )
{
}

int HandleDestroy()
{
	CNOVRShutdown();
	return 0;
}

void APIENTRY DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const char* message, const void* userParam)
{
	ovrprintf( "GL Error: %s\n", message );
}


int CNOVRInit( const char * appname, int screenx, int screeny, int iInitFlags )
{
	int r;

	ovrprintf( "Installing crash handler.\n" );
	tcccrash_install();

	InternalSetupTCCInterface();
	InternalSetupNamedPtrs();
	OGResetSafeMutices();

	CNOVRListSystemInit();
	CNOVRFileSearchAddPath( "assets" ); //Base fallback (also initializes file search system)
	CNOVRFileSearchAddPath( "modules" ); //Base fallback (also initializes file search system)

	//In case we are being used as a submodule.
	CNOVRFileSearchAddPath( "cnovr/assets" );
	CNOVRFileSearchAddPath( "cnovr/modules" );

	CNOVRFileSearchAddPath( "cnovr" );

	ovrprintf( "Initializing OpenVR.\n" );

	int has_vr = 0;

	if( !( iInitFlags & CNOVR_INIT_DISABLE_OPENVR ) )
	{
		EVRInitError e;
		uint32_t vrtoken;
		vrtoken = VR_InitInternal( &e, (iInitFlags & CNOVR_INIT_IS_OVERLAY)?EVRApplicationType_VRApplication_Overlay:EVRApplicationType_VRApplication_Scene );
		if( !vrtoken )
		{
			ovrprintf( "Error calling VR_InitInternal: %d (%s)\n", e, VR_GetVRInitErrorAsEnglishDescription( e ) );
			if( iInitFlags & CNOVR_INIT_NEED_OPENVR )
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
				if( iInitFlags & CNOVR_INIT_NEED_OPENVR )
					return -1;
				has_vr = 0;
			}
		}
	}
	else
	{
		ovrprintf( "Mode shows OpenVR Disabled.\n" );
	}

	ovrprintf( "Initializing Window.\n" );

	//Create Companion Window.
	if( ( r = CNFGSetup( appname, screenx, screeny ) ) )
	{
		return r;
	}

	//OK, OpenVR is set up.  Now, set up rendering system.
	{
		cnovrstate = malloc( sizeof( *cnovrstate ) );
		
		ovrprintf( "CNOVR State Location: %p\n", cnovrstate );
		
		memset( cnovrstate, 0, sizeof( *cnovrstate ) );
		cnovrstate->fNear = 0.01;
		cnovrstate->fFar = 1000.0;
		cnovrstate->bHasOvr = has_vr;
		cnovrstate->has_preview = screenx!=0 && screeny != 0;
		cnovrstate->iEyeRenderWidth = -1;
		cnovrstate->iEyeRenderHeight = -1;
		cnovrstate->iPreviewWidth = -1;
		cnovrstate->iPreviewHeight = -1;
		cnovrstate->stereotargets[0] = 0;
		cnovrstate->stereotargets[1] = 0;
		cnovrstate->fPreviewFOV = 100;
		cnovrstate->iMultisample = (iInitFlags & CNOVR_INIT_DISABLE_MULTISAMPLE)?0:DEFAULT_MULTISAMPLE;

		//Initial camrea
		pose_make_identity( &cnovrstate->pPreviewPose );
		cnovrstate->pPreviewPose.Pos[0] = 0;
		cnovrstate->pPreviewPose.Pos[1] = 0;
		cnovrstate->pPreviewPose.Pos[2] = -3;
		
		ovrprintf( "Continuing state update.\n" );

		if( has_vr )
		{
			cnovrstate->oSystem = (struct VR_IVRSystem_FnTable *)CNOVRGetOpenVRFunctionTable( IVRSystem_Version );
			cnovrstate->oRenderModels = (struct VR_IVRRenderModels_FnTable *)CNOVRGetOpenVRFunctionTable( IVRRenderModels_Version );
			cnovrstate->oCompositor = (struct VR_IVRCompositor_FnTable *)CNOVRGetOpenVRFunctionTable( IVRCompositor_Version );
			cnovrstate->oOverlay = (struct VR_IVROverlay_FnTable *)CNOVRGetOpenVRFunctionTable( IVROverlay_Version );
			cnovrstate->oInput = (struct VR_IVRInput_FnTable *)CNOVRGetOpenVRFunctionTable( IVRInput_Version );
		}

		cnovrstate->sAppName = (!appname)?strdup("cnovr_unnamed_app"):strdup( appname );

		cnovrstate->openvr_renderposes = malloc( sizeof( struct TrackedDevicePose_t ) * MAX_POSES_TO_PULL_FROM_OPENVR );
		cnovrstate->openvr_trackedposes = malloc( sizeof( struct TrackedDevicePose_t ) * MAX_POSES_TO_PULL_FROM_OPENVR );
		cnovrstate->pRenderPoses = malloc( sizeof( cnovr_pose ) * MAX_POSES_TO_PULL_FROM_OPENVR );
		cnovrstate->pTrackedPoses = malloc( sizeof( cnovr_pose ) * MAX_POSES_TO_PULL_FROM_OPENVR );
		cnovrstate->bRenderPosesValid = malloc( MAX_POSES_TO_PULL_FROM_OPENVR );
		cnovrstate->bTrackedPosesValid = malloc( MAX_POSES_TO_PULL_FROM_OPENVR );
		cnovrstate->bIsOverlay = iInitFlags & CNOVR_INIT_IS_OVERLAY;
		cnovrstate->pEyeToHead = malloc( sizeof( cnovr_pose ) * 2 );
		cnovrstate->is_submodule = !!CNOVRFileSearch( "cnovr/assets/cnovr.glsl" );
	}

	CNOVRInternalSetupFreeLaterSet();

	ovrprintf( "Initializing Extensions.\n" );

	chewInit();

	if( CNOVRCheck() ) ovrprintf( "Init GL check\n" );

	if( has_vr )
	{
		char strbuf[128];
		memset( strbuf, 0, sizeof( strbuf ) );
		r = cnovrstate->oSystem->GetStringTrackedDeviceProperty( k_unTrackedDeviceIndex_Hmd, ETrackedDeviceProperty_Prop_SerialNumber_String, strbuf, sizeof(strbuf)-1, 0 );
		ovrprintf( "Using HMD: %s\n", strbuf );
	}

	ovrprintf( "OpenGL %s\n", glGetString(GL_VERSION) );

	//This is buggy on -rdynamic and older GL implementations.
	if( glDebugMessageControlfnptr && glDebugMessageCallbackfnptr )
	{
		ovrprintf( "Setting debug callback\n" );
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageControl( GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, 0, GL_TRUE );
		glDebugMessageCallback( (GLDEBUGPROC)DebugCallback, 0 );
		ovrprintf( "Debug installed\n" );
	}

	//More OpenGL Setup
	int i;
	for( i = 0; i < 8; i++ )
	{
		glEnableVertexAttribArray( i ); //m->pGeos[i]->nVBO);
	}

	cnovrstate->asTrackedDeviceSerialStrings = calloc( MAX_POSES_TO_PULL_FROM_OPENVR, sizeof( const char * ) );
	cnovrstate->asTrackedDeviceModelStrings = calloc( MAX_POSES_TO_PULL_FROM_OPENVR, sizeof( const char * ) );

	//cnovrstate->pRootNode = CNOVRNodeCreateSimple( 1 );
	//printf( "Malloced State: %p;;; %p = %p\n", cnovrstate, &cnovrstate->pRootNode, cnovrstate->pRootNode );


	CNOVRInternalStartCacheSystem();
	CNOVRJobInit();

	ovrprintf( "Setting up focus\n" );
	InternalCNOVRFocusSetup();

	{
		cnovrstate->fullscreengeo = CNOVRModelCreate( 0, GL_TRIANGLES );
		cnovr_point3d size = { 1., 1., 0. };
		CNOVRModelAppendMesh( cnovrstate->fullscreengeo, 1, 1, 0, size, 0, 0 );
	}

	cnovrstate->fFrameStartTime = OGGetAbsoluteTime();

	ovrprintf( "Waiting 30ms for OpenVR to initialize. (TODO, should figure out why this is needed)\n" );

	//???! How do we make sure OpenVR initialization is complete?
	OGUSleep( 30000 );
	
	ovrprintf( "Init complete.\n" );

	return 0;
}

double FrameStart;

void CNOVRUpdate()
{
//	static struct TrackedDevicePose_t lastframeposes[MAX_POSES_TO_PULL_FROM_OPENVR];

	//Get poses
	int i;

//	memcpy( cnovrstate->openvr_renderposes, lastframeposes, sizeof( lastframeposes ) );

	if( cnovrstate->bHasOvr )
	{

		EVRCompositorError err = cnovrstate->oCompositor->WaitGetPoses( 
			cnovrstate->openvr_renderposes, MAX_POSES_TO_PULL_FROM_OPENVR, 
			cnovrstate->openvr_trackedposes, MAX_POSES_TO_PULL_FROM_OPENVR );

		if( err != EVRCompositorError_VRCompositorError_None )
		{
			//Need another way to at least get HMD position.
			cnovrstate->oSystem->GetDeviceToAbsoluteTrackingPose( ETrackingUniverseOrigin_TrackingUniverseStanding, 0, cnovrstate->openvr_renderposes, MAX_POSES_TO_PULL_FROM_OPENVR);
			cnovrstate->oSystem->GetDeviceToAbsoluteTrackingPose( ETrackingUniverseOrigin_TrackingUniverseStanding, 0, cnovrstate->openvr_trackedposes, MAX_POSES_TO_PULL_FROM_OPENVR);
		}

		//We assume the worst case succeeds.
		{
			for( i = 0; i < MAX_POSES_TO_PULL_FROM_OPENVR; i++ )
			{
				struct TrackedDevicePose_t * trenderpose = &cnovrstate->openvr_renderposes[i];
				struct TrackedDevicePose_t * ttrackedpose = &cnovrstate->openvr_trackedposes[i];

#if 0
				//Enable this code for pose debugging.
				if( i == 0 )
				{
					int j;
					printf( "HMD valid: %d %d\n", trenderpose->bPoseIsValid, trenderpose->bPoseIsValid );
					float * p = &trenderpose->mDeviceToAbsoluteTracking.m[0][0];
					for( j = 0; j < 3; j++ )
						printf( "%f %f %f %f\n", p[j*4+0], p[j*4+1], p[j*4+2], p[j*4+3] );
				}
#endif

				if( ( cnovrstate->bRenderPosesValid[i] = trenderpose->bPoseIsValid ) )
				{
					CNOVRPoseFromHMDMatrix( &cnovrstate->pRenderPoses[i], &trenderpose->mDeviceToAbsoluteTracking );
				}

				if( ( cnovrstate->bTrackedPosesValid[i] = ttrackedpose->bPoseIsValid ) )
				{
					CNOVRPoseFromHMDMatrix( &cnovrstate->pTrackedPoses[i], &ttrackedpose->mDeviceToAbsoluteTracking );
				}

				if( ttrackedpose->bPoseIsValid || trenderpose->bPoseIsValid )
				{
					if( !cnovrstate->asTrackedDeviceSerialStrings[i] )
					{
						const char * rv = CNOVRGetTrackedDeviceString( i, ETrackedDeviceProperty_Prop_SerialNumber_String );
						if( rv && rv[0] )
						{
							cnovrstate->asTrackedDeviceSerialStrings[i] = strdup( rv );
							//We should be able to get model number by now.  If not something crazy happened.
							rv = CNOVRGetTrackedDeviceString( i, ETrackedDeviceProperty_Prop_ModelNumber_String );
							cnovrstate->asTrackedDeviceModelStrings[i] = rv?strdup( rv ):strdup("");
							printf( "Found tracked device %d: %s: %s\n", i, cnovrstate->asTrackedDeviceSerialStrings[i], cnovrstate->asTrackedDeviceModelStrings[i] );
						}
					}
				}
			}
		}
	}

	double Now = OGGetAbsoluteTime();
	cnovrstate->fDeltaTime = (Now-cnovrstate->fFrameStartTime);
	cnovrstate->fFrameStartTime = Now;
	FrameStart = OGGetAbsoluteTime();
	//Update + prerender
	//cnovr_simple_node * root = cnovrstate->pRootNode;

	//Scene Graph Pre-Render
	CNOVRListCall( cnovrLUpdate, 0 );
	while( CNOVRJobProcessQueueElement( cnovrQPrerender ) );
	CNOVRListCall( cnovrLPrerender, 0 );

	//Waste some time...
	CNFGHandleInput();

	if( CNOVRCheck() ) ovrprintf( "Prerender Check\n" );

	if( cnovrstate->bHasOvr && ! cnovrstate->bIsOverlay )
	{
		uint32_t iEyeRenderWidth, iEyeRenderHeight;
		cnovrstate->oSystem->GetRecommendedRenderTargetSize( &iEyeRenderWidth, &iEyeRenderHeight );
		if( iEyeRenderWidth != cnovrstate->iEyeRenderWidth || iEyeRenderHeight != cnovrstate->iEyeRenderHeight )
		{
			CNOVRDelete( cnovrstate->stereotargets[0] );
			CNOVRDelete( cnovrstate->stereotargets[1] );
	 
			//Resize the render targets.
			cnovrstate->stereotargets[0] = CNOVRRFBufferCreate( iEyeRenderWidth, iEyeRenderHeight, cnovrstate->iMultisample, 1 );
			cnovrstate->stereotargets[1] = CNOVRRFBufferCreate( iEyeRenderWidth, iEyeRenderHeight, cnovrstate->iMultisample, 1 );
			cnovrstate->iEyeRenderWidth = iEyeRenderWidth;
			cnovrstate->iEyeRenderHeight = iEyeRenderHeight;
		}
	}

	if( cnovrstate->has_preview )
	{
		HandleHIDInput();
		short iPreviewWidth, iPreviewHeight;
		CNFGGetDimensions( &iPreviewWidth, &iPreviewHeight );
		if( iPreviewWidth != cnovrstate->iPreviewWidth || iPreviewHeight != cnovrstate->iPreviewHeight )
		{
			cnovrstate->iPreviewWidth  = iPreviewWidth;
			cnovrstate->iPreviewHeight = iPreviewHeight;
		}
	}

	//Probably should do some other stuff while anything from the prerender step is still ticking.

	glCullFace( GL_BACK );
	glEnable( GL_CULL_FACE );
	glClearColor( 0, 0, 0, 1 );

	glEnable( GL_DEPTH_TEST );
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);

	//Scene Graph Render
	if( cnovrstate->bHasOvr && !cnovrstate->bIsOverlay )
	{
		int i;
		for( i = 0; i < 2; i++ )
		{
			cnovrstate->eyeTarget = i;
			{
				//In case eye-to-head changes, we need to get that
				HmdMatrix34_t xform = cnovrstate->oSystem->GetEyeToHeadTransform( EVREye_Eye_Left + i );
				CNOVRPoseFromHMDMatrix( &cnovrstate->pEyeToHead[i], &xform );

				struct HmdMatrix44_t pm = cnovrstate->oSystem->GetProjectionMatrix( EVREye_Eye_Left + i, cnovrstate->fNear, cnovrstate->fFar );
				memcpy( cnovrstate->mPerspective, &pm.m[0][0], sizeof( HmdMatrix44_t ) );
				cnovr_pose eye_in_worldspace;
				apply_pose_to_pose( &eye_in_worldspace, &cnovrstate->pRenderPoses[0], &cnovrstate->pEyeToHead[i] );
				pose_invert( &eye_in_worldspace, &eye_in_worldspace );
				pose_to_matrix44( cnovrstate->mView, &eye_in_worldspace );
				matrix44identity( cnovrstate->mModel );
			}

			if( !cnovrstate->stereotargets[i] ) continue;

			//This is a balance:  We could render both eyes simultaneously, or we could do this. 
			CNOVRFBufferActivate( cnovrstate->stereotargets[i] );
			int width = cnovrstate->iRTWidth = cnovrstate->iEyeRenderWidth;
			int height = cnovrstate->iRTHeight = cnovrstate->iEyeRenderHeight;
			glViewport(0, 0, width, height );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
			//root->base.header->Render( root );
			CNOVRListCall( cnovrLRender0, 0 ); 
			CNOVRListCall( cnovrLRender1, 0 ); 
			CNOVRListCall( cnovrLRender2, 0 ); 
			CNOVRListCall( cnovrLRender3, 0 ); 
			CNOVRListCall( cnovrLRender4, 0 ); 
			//CNOVRFBufferDeactivate( cnovrstate->stereotargets[i] );
			CNOVRFBufferBlitResolve( cnovrstate->stereotargets[i] );
		}
		for( i = 0; i < 2; i++ )
		{

			Texture_t t;
			t.handle = (void*)(uintptr_t)cnovrstate->stereotargets[i]->nResolveTextureId[0];
			t.eType = ETextureType_TextureType_OpenGL;
			t.eColorSpace = EColorSpace_ColorSpace_Auto;
			cnovrstate->oCompositor->Submit( EVREye_Eye_Left + i, &t, 0, 0 ); 
		}
	}

	if( CNOVRCheck() ) ovrprintf( "Render Check\n" );

	if( cnovrstate->oCompositor && !cnovrstate->bIsOverlay ) cnovrstate->oCompositor->PostPresentHandoff();

	int did_advanced_preview = 1;
	if( cnovrstate->has_preview )
	{
		int r = CNOVRListCall( cnovrLPreviewRender, 0 );
		if( !r )
		{
			int width = cnovrstate->iPreviewWidth;
			int height = cnovrstate->iPreviewHeight;
			did_advanced_preview = 0;
			cnovrstate->eyeTarget = 2;
			matrix44identity( cnovrstate->mModel );
			pose_to_matrix44( cnovrstate->mView, &cnovrstate->pPreviewPose );
			matrix44perspective( cnovrstate->mPerspective, cnovrstate->fPreviewFOV, width/(float)height, cnovrstate->fNear, cnovrstate->fFar );
			glViewport(0, 0, width, height );
			//glClearColor( 1, 0, 1, 1 );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
			//root->base.header->Render( root );
			CNOVRListCall( cnovrLRender0, 0 ); 
			CNOVRListCall( cnovrLRender1, 0 ); 
			CNOVRListCall( cnovrLRender2, 0 ); 
			CNOVRListCall( cnovrLRender3, 0 ); 
			CNOVRListCall( cnovrLRender4, 0 ); 
		}
#if 0
		int width = cnovrstate->iRTWidth = cnovrstate->iPreviewWidth;
		int height = cnovrstate->iRTHeight = cnovrstate->iPreviewHeight;
		cnovrstate->eyeTarget = 2;
		matrix44identity( cnovrstate->mModel );
		pose_to_matrix44( cnovrstate->mView, &cnovrstate->pPreviewPose );

		int i;
		float distancecut = cnovrstate->fPreviewForegroundSplitDistance;
		for( i = 0; i < 2; i++ )
		{
			CNOVRFBufferActivate( cnovrstate->previewtarget[i] );
			//glEnable( GL_CLIP_PLANE0 );
			//GLdouble eqn[4] = { 1., 0., 1., 1. };
			//glClipPlane( GL_CLIP_PLANE0, eqn );
			matrix44perspective( cnovrstate->mPerspective, cnovrstate->fPreviewFOV, width/(float)height, i?cnovrstate->fNear:distancecut, i?distancecut:cnovrstate->fFar );
			glViewport(0, 0, width, height );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
			root->base.header->Render( root );
			CNOVRListCall( cnovrLRender0, 0, 0); 
			CNOVRListCall( cnovrLRender1, 0, 0); 
			CNOVRListCall( cnovrLRender2, 0, 0); 
			CNOVRListCall( cnovrLRender3, 0, 0); 
			CNOVRListCall( cnovrLRender4, 0, 0); 
			//glDisable( GL_CLIP_PLANE0 );
			CNOVRFBufferBlitResolve( cnovrstate->previewtarget[i] );
		}

		glDisable( GL_DEPTH_TEST );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		CNOVRRender( cnovrstate->fullscreenshader );
		glEnable( GL_TEXTURE_2D );
		//printf( "%d\n", cnovrstate->previewtarget->nResolveTextureId );
		glActiveTextureCHEW( GL_TEXTURE0 + 0 );
		glBindTexture( GL_TEXTURE_2D, cnovrstate->previewtarget[0]->nResolveTextureId );
		glActiveTextureCHEW( GL_TEXTURE0 + 1 );
		glBindTexture( GL_TEXTURE_2D, cnovrstate->previewtarget[1]->nResolveTextureId );
		CNOVRRender( cnovrstate->fullscreengeo );
		//glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST );
#endif
	}

	InternalCNOVRFocusUpdate();

	if( CNOVRCheck() ) ovrprintf( "Cycle Check\n" );

	cnovrstate->fFrameTime = (OGGetAbsoluteTime()-cnovrstate->fFrameStartTime);

#if defined( WINDOWS ) || defined( WIN32 ) || defined ( WIN64 )
		wglSwapIntervalEXTCHEW(!cnovrstate->bHasOvr);
#else
		//XXX Hacky - this disables vsync on Linux only
		void   CNFGSetVSync( int vson );
		CNFGSetVSync( !cnovrstate->bHasOvr );
#endif
//k_pch_SteamVR_PreferredRefreshRate 

	if( cnovrstate->bHasOvr )
	{
		cnovrstate->fTargetFPS = cnovrstate->oSystem->GetFloatTrackedDeviceProperty( k_unTrackedDeviceIndex_Hmd, ETrackedDeviceProperty_Prop_DisplayFrequency_Float, 0 );
	}
	else
	{
		cnovrstate->fTargetFPS = 10000;
	}
	
//	double diff = OGGetAbsoluteTime() - FrameStart;
//	if( diff > 0.004 )	printf( "Diff: %f\n", diff );
	if( !did_advanced_preview ) CNFGSwapBuffers(1);
//	FrameStart = OGGetAbsoluteTime();

	CNOVRListCall( cnovrLPostRender, 0 ); 

//	glFlush();
}

void CNOVRShutdown()
{
	int i, k;
	void VR_ShutdownInternal();

	CNOVRStopTCCSystem();

	StopFileTimeChekerThread();

	InternalCNOVRFocusShutdown();

	InternalFileSearchShutdown();

	VR_ShutdownInternal();

	//Free out any remaining tags from the initial list.
	tcccrash_deltag( 0 );

	printf( "Final flush\n" );
	// We flush everything else out here..
	for( k = 0; k < 6; k++ )
	for( i = 0; i < cnovrQMAX; i++ )
	{
		while( CNOVRJobProcessQueueElement( i ) );
	}

	printf( "Stopping TCC interface\n" );
	InternalBreakdownRestOfTCCInterface();

	printf( "Closing cache system\n" );
	CNOVRInternalStopCacheSystem();
	CNOVRListSystemDestroy();
	CNOVRJobStop();

	printf( "Cleanup complete\n" );
	//Flush out any remaining free laters.
	CNOVRFreeLaterShutdown();

	printf( "Unhooking crash handler\n" );
	tcccrash_uninstall();

	free( cnovrstate->sAppName );

	exit( 0 );
}

int CNOVRCheck()
{
	int lastError = -1;
	GLenum e = glGetError();
	if( e != GL_NO_ERROR )
	{
		if( e == lastError ) return 0;
		lastError = e;
		if( e == GL_INVALID_ENUM ) ovrprintf( "GL_INVALID_ENUM\n" );
		if( e == GL_INVALID_VALUE ) ovrprintf( "GL_INVALID_VALUE\n" );
		if( e == GL_INVALID_OPERATION ) ovrprintf( "GL_INVALID_OPERATION\n" );
		if( e == GL_INVALID_FRAMEBUFFER_OPERATION ) ovrprintf( "GL_INVALID_FRAMEBUFFER_OPERATION\n" );
		if( e == GL_OUT_OF_MEMORY ) ovrprintf( "GL_OUT_OF_MEMORY\n" );
		if( e == GL_STACK_UNDERFLOW ) ovrprintf( "GL_STACK_UNDERFLOW\n" );
		if( e == GL_STACK_OVERFLOW ) ovrprintf( "GL_STACK_OVERFLOW\n" );
		return e;
	}
	else
	{
		return 0;
	}
}


int CNOVRAlert( void * tag, int priority, const char * format, ... )
{
	va_list args;
	va_start(args, format);
	int r = CNOVRAlertv( tag, priority, format, args );
	va_end( args );
	return r;
}

int CNOVRAlertv( void * tag, int priority, const char * format, va_list ap )
{
	char * buffer = 0;
	int len = tvasprintf( &buffer, format, ap );
	if( len > 0 && buffer[len-1] == '\n' ) buffer[len-1] = 0; //Strip off extra newlines.
	puts( buffer );
	CNOVRTCCLog( tag, buffer );
	return len;
}

void CNOVRShaderLoadedSetUniformsInternal()
{
	if( CNOVRCheck() ) ovrprintf( "Pre-Uniform Check with shader %s\n", cnovr_current_shader->shaderfilebase );
	const static GLint TextureSlots[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	int uniform;
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( UNIFORMSLOT_MODEL ) ) != INVALIDUNIFORM )
		glUniformMatrix4fv( uniform, 1, 1, cnovrstate->mModel );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( UNIFORMSLOT_VIEW ) )  != INVALIDUNIFORM )
		glUniformMatrix4fv( uniform , 1, 1, cnovrstate->mView );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( UNIFORMSLOT_PERSPECTIVE ) ) != INVALIDUNIFORM )
		glUniformMatrix4fv( uniform, 1, 1, cnovrstate->mPerspective );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( UNIFORMSLOT_RENDERPROPS ) ) != INVALIDUNIFORM )
		glUniform4fv( uniform, 1, &cnovrstate->iRTWidth );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( UNIFORMSLOT_TEXTURES ) ) != INVALIDUNIFORM )
		glUniform1iv( uniform, 8, TextureSlots );
	//Ignore all uniform errors.
	if( CNOVRCheck() ) ovrprintf( "Post-Uniform Check\n" );
	glGetError();
}




