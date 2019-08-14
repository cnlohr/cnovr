#ifndef _CNOVR_H
#define _CNOVR_H

#include <cnovrparts.h>
#include <openvr_capi.h>


//Attach an alert to a specific object (or null for the general alarm)
void CNOVRAlert( cnovr_model * obj, int priority, const char * format, ... );
#define ovrprintf(x...) CNOVRAlert( 0, 5, x)

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
	float mModel[16];	//Current model matrix, changes per object.
	float mView[16];	//Current view matrix, changes per eye.
	float mPerspective[16];

	//Notes:
	// Standard CNOVR Uniforms:
	// 16 = [ Render Width, Render Height, Far, Near ]
	// 17 = Perspective
	// 18 = View
	// 19 = Model

	cnovr_model * pCurrentModel;
};

extern struct cnovrstate_t * cnovrstate;
extern struct VR_IVRSystem_FnTable * openvr;

//////////////////////////////////////////////////////////////////////////////
typedef void(cnovr_cb_fn)( void * opaquev, int opaquei );

typedef enum
{
	cnovrQLow,			//Things like making sure we're up to date - not sure?
	cnovrQAsync,		//
	cnovrQUpdate,		//Happens in the render thread
	cnovrQPrerender,	//Happens in the render thread
} cnovrQueueType;

//Async, but, has delays between each completion.
void CNOVRJobTack( cnovrQueueType q, cnovr_cb_fn fn, void * opaquev, int opaquei );
void CNOVRJobCancel( cnovrQueueType q, cnovr_cb_fn fn, void * opaquev, int opaquei );

//////////////////////////////////////////////////////////////////////////////
int CNOVRInit( const char * appname, int screenx, int screeny );
void CNOVRUpdate();
int CNOVRCheck(); //Check for errors.

#endif

