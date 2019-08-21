// Copyright 2019 <>< Charles Lohr licensable under the MIT/X11 or NewBSD licenses.

#ifndef _CNOVR_H
#define _CNOVR_H

#include <cnovrparts.h>
#include <openvr_capi.h>


//Attach an alert to a specific object (or null for the general alarm)
//1 = critical error.
//2 = serious error (missing files, etc.)
//5 = debug.
void CNOVRAlert( cnovr_model * obj, int priority, const char * format, ... );
#define ovrprintf(x...) CNOVRAlert( 0, 5, x)

//////////////////////////////////////////////////////////////////////////////

#define UNIFORMSLOT_MODEL       4
#define UNIFORMSLOT_VIEW        5
#define UNIFORMSLOT_PERSPECTIVE 6


//////////////////////////////////////////////////////////////////////////////
// Globals (State)
struct cnovrstate_t
{
	//Current rendertarget
	uint32_t iRTWidth, iRTHeight;
	cnovr_rf_buffer * sterotargets[2];

	//GL State Stuff
	float fNear;
	float fFar;

	//XXX What about the view and perspective matricies?
	float mModel[16];	//Current model matrix, changes per object. (SLOT=4)
	float mView[16];	//Current view matrix, changes per eye.     (SLOT=5)
	float mPerspective[16];                                      // (SLOT=6)

	//Notes:
	// Standard CNOVR Uniforms:
	// 16 = [ Render Width, Render Height, Far, Near ]
	// 17 = Perspective
	// 18 = View
	// 19 = Model

	cnovr_model * pCurrentModel;

	struct VR_IVRSystem_FnTable * oSystem;
	struct VR_IVRRenderModels_FnTable * oRenderModels;
};

extern struct cnovrstate_t * cnovrstate;

//////////////////////////////////////////////////////////////////////////////
typedef void(cnovr_cb_fn)( void * opaquev, int opaquei );

typedef enum
{
	cnovrQLow,			//Things like making sure we're up to date - not sure?
	cnovrQAsync,		//??? No access to GL thread.
	cnovrQUpdate,		//???
	cnovrQPrerender,	//Happens in the render thread
} cnovrQueueType;

//Async, but, has delays between each completion. TBD: Multiple identical queued items will be squashed? Maybe?  Cancellation must match all parameters.
void CNOVRJobTack( cnovrQueueType q, cnovr_cb_fn fn, void * opaquev, int opaquei );
void CNOVRJobCancel( cnovrQueueType q, cnovr_cb_fn fn, void * opaquev, int opaquei );

//////////////////////////////////////////////////////////////////////////////
int CNOVRInit( const char * appname, int screenx, int screeny );
void CNOVRUpdate();
int CNOVRCheck(); //Check for errors.

#endif

