// Copyright 2019 <>< Charles Lohr licensable under the MIT/X11 or NewBSD licenses.

#ifndef _CNOVR_H
#define _CNOVR_H

#include <cnovrparts.h>
#include <stdbool.h>
#include <cnovropenvr.h>
#include <stdarg.h>

#define MAX_POSES_TO_PULL_FROM_OPENVR 16

//Attach an alert to a specific object (or null for the general alarm)
//1 = critical error.
//2 = serious error (missing files, etc.)
//5 = debug.
int CNOVRAlert( void * tag, int priority, const char * format, ... );
int CNOVRAlertv( void * tag, int priority, const char * format, va_list ap );
#define ovrprintf(x...) CNOVRAlert( 0, 5, x)

//////////////////////////////////////////////////////////////////////////////

#define UNIFORMSLOT_MODEL       4
#define UNIFORMSLOT_VIEW        5
#define UNIFORMSLOT_PERSPECTIVE 6
#define UNIFORMSLOT_RENDERPROPS 7
#define UNIFORMSLOT_TEXTURES    8 //Provides 8 textures total.

//////////////////////////////////////////////////////////////////////////////
// Globals (State)
struct cnovrstate_t
{
	//Current rendertarget
	cnovr_rf_buffer * sterotargets[2];
	cnovr_rf_buffer * previewtarget;

	//GL State Stuff (in the "renderprops" array)
	float iRTWidth;
	float iRTHeight;
	float fNear;
	float fFar;
	float mModel[16];	//Current model matrix, changes per object. (SLOT=4)
	float mView[16];	//Current view matrix, changes per eye.     (SLOT=5)
	float mPerspective[16];                                      // (SLOT=6)

	cnovr_simple_node * pRootNode;

	struct VR_IVRSystem_FnTable * oSystem;
	struct VR_IVRRenderModels_FnTable * oRenderModels;
	struct VR_IVRCompositor_FnTable * oCompositor;
	struct VR_IVRInput_FnTable * oInput;

	//These are things like lighthouses, HMD, controllers, etc.
	struct TrackedDevicePose_t * openvr_renderposes;
	struct TrackedDevicePose_t * openvr_trackedposes;
	cnovr_pose * pRenderPoses;
	cnovr_pose * pTrackedPoses;
	uint8_t * bRenderPosesValid;
	uint8_t * bTrackedPosesValid;
	cnovr_pose * pEyeToHead; 

	cnovr_pose pPreviewPose;
	float fPreviewFOV;
	float fFrameTimems;

	short    iPreviewWidth, iPreviewHeight;
	short    iEyeRenderWidth, iEyeRenderHeight;

	uint8_t  eyeTarget;
	uint8_t  has_ovr;
	uint8_t  has_preview;
} __attribute__((packed));

#if defined( TCCINSTANCE ) && defined( WINDOWS )
__declspec( dllimport ) struct cnovrstate_t * cnovrstate;
#else
extern struct cnovrstate_t * cnovrstate;
#endif

//////////////////////////////////////////////////////////////////////////////
int CNOVRInit( const char * appname, int screenx, int screeny, int allow_init_without_vr );
void CNOVRShutdown();
void CNOVRUpdate();
int CNOVRCheck(); //Check for errors.

void CNOVRShaderLoadedSetUniformsInternal();

#endif

