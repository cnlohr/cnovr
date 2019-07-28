#ifndef _CNOVR_H
#define _CNOVR_H

#include <cnovrparts.h>
#include <openvr_capi.h>

#define ovrprintf(x...) printf(x)

//////////////////////////////////////////////////////////////////////////////
// Globals (State)
struct cnovrstate_t
{
	//Current rendertarget
	uint32_t iRTWidth, iRTHeight;
	cnovr_rf_buffer * sterotargets[2];

	//
	

	//GL State Stuff
	float fNear;
	float fFar;
};

extern struct cnovrstate_t * cnovrstate;
extern struct VR_IVRSystem_FnTable * openvr;


//////////////////////////////////////////////////////////////////////////////
int CNOVRInit( const char * appname, int screenx, int screeny );
void CNOVRUpdate();
int CNOVRCheck(); //Check for errors.

#endif

