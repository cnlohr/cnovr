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
};

extern struct cnovrstate_t * cnovrstate;
extern struct VR_IVRSystem_FnTable * openvr;

//////////////////////////////////////////////////////////////////////////////
typedef void(cnovr_cb_fn)( void * opaquev, int opaquei );

void CNOVRTackJobAsync( cnovr_cb_fn fn, void * opaquev, int opaquei );
void CNOVRTackJobUpdate( cnovr_cb_fn fn, void * opaquev, int opaquei );
void CNOVRTackJobPrerender( cnovr_cb_fn fn, void * opaquev, int opaquei );

//////////////////////////////////////////////////////////////////////////////
int CNOVRInit( const char * appname, int screenx, int screeny );
void CNOVRUpdate();
int CNOVRCheck(); //Check for errors.

#endif

