#ifndef _CNOVR_FOCUS_H
#define _CNOVR_FOCUS_H

//The idea behind this is a general purpose focus system for multiple controllers.

#include <cnovrmath.h>
#include <stdint.h>

#define CNOVRFOCUSMAXDEVS 16
#define BUTTONMASKSIZE    2 //(16 * 8 buttons)
#define CNOVRFOCUS_FAR  1000


#define CNOVRINPUTDEVS 3
#define CNOVRINPUTDEV_LEFT  1
#define CNOVRINPUTDEV_RIGHT 2
//Dev 0 = HMD / Keyboard
//Dev 1 = Controller Left
//Dev 2 = Controller Right

typedef enum
{
	CTRLA_TRIGGER = 0,
	CTRLA_BUTTONA,
	CTRLA_BUTTONB,
	CTRLA_PINCHBTN,
	CTRLA_GRASP, //Last "button"
	CTRLA_TRIG, //Special
	CTRLA_MODEL, //Special
	CTRLA_TIP, //Special
	CTRLA_MAX,
} ControllerActions;

struct cnovrfocus_capture_t;
typedef struct cnovrfocus_capture_t cnovrfocus_capture;
struct cnovrfocus_properties_t;
typedef struct cnovrfocus_properties_t cnovrfocus_properties;


//We have "Check" (Checks everything in the list).  It is called on motion, not on presses.
// the callback looks like:
//   typedef void(cnovr_cb_fn)( void * tag, cnovrfocus_properties * controller );
// If you, as a subscriber have interest in this event, call 
//
// If you've claimed interest you'll get:
//    In, Out, Motion, DownNoFocus, UpNoFocus
// If you have focus, you'll get:
//    Drag, UpFocus, DownFocus

//We need to claim 

typedef enum
{
	CNOVRF_IN = 0,
	CNOVRF_OUT,
	CNOVRF_DOWNFOCUS,
	CNOVRF_DOWNNOFOCUS,
	CNOVRF_UPFOCUS,
	CNOVRF_UPNOFOCUS,
	CNOVRF_MOTION,
	CNOVRF_DRAG,
	CNOVRF_ACQUIREDFOCUS,
	CNOVRF_LOSTFOCUS,
	CNOVRF_MAX_EVENTS,
} cnovrfocus_event;

typedef int (*CNOVRFocusEvent)( int event, cnovrfocus_capture * cap, cnovrfocus_properties * prop, int buttoninfo );

typedef struct cnovrfocus_capture_t
{
	void * tcctag;
	void * tag;
	void * opaque;
	CNOVRFocusEvent cb;
} cnovrfocus_capture;


struct cnovr_collide_results_t;
struct cnovr_model_t;

typedef struct cnovrfocus_properties_t
{
	int devid;
	uint64_t buttonmask[BUTTONMASKSIZE];
	cnovr_pose poseTip;
	cnovrfocus_capture * capturedFocus;
	cnovrfocus_capture * capturedPassive;
	float capturedPassiveDistance;

	//These are only valid for down, up and motion events.
	struct cnovr_collide_results_t collide_results;
	struct cnovr_model_t * which_model;

	//This is transitory information stored when excuting the collision list.  Don't mess with it unless you know what you're doing.
	cnovrfocus_capture * NewCapturedPassive;
	float NewPassiveRealDistance;
	float NewPassiveProps[4];
} cnovrfocus_properties;

//Ugh this is awkward.  Need to fix.

//YOU own the 'ce' object. We just store it for you.
void CNOVRFocusRespond( cnovrfocus_capture * ce, float realdistance, float * fdprops );
void CNOVRFocusAcquire( cnovrfocus_capture * ce, int wantfocus );
void CNOVRFocusRemoveTag( void * tag );
cnovr_pose * CNOVRFocusGetTipPose( int device );

uint64_t CNOVRFocusGetVRActionHandleFromConrollerAndCtrlA( int dev, int ctrl ); //Where ctrl = stuff like CTRLA_TIP

// Subsystem designed to link cnovr_models and make them trackable.

struct cnovr_model_t;
typedef struct cnovr_model_focus_controller_t
{
	cnovr_pose ** focusgrab; //array, [INPUTDEVS] //If set, currently dragging.
	cnovrfocus_capture * focusevent; //Return collide events with this.
	cnovr_pose * twohandgrab_last[CNOVRINPUTDEVS]; //Points to pose inside focus controller
	float initial_grab_z[CNOVRINPUTDEVS];
	cnovr_point3d gplast[CNOVRINPUTDEVS];
	//struct cnovr_model_t * mparent;
	cnovr_pose pose_internal;
	int collide_mask;	//0 is default, unmask all devs.
} cnovr_model_focus_controller;

// Focus Stuff

void CNOVRModelSetInteractable( struct cnovr_model_t * m, cnovrfocus_capture * focusevent );
void CNOVRGeneralHandleFocusEvent( cnovr_model_focus_controller * fc, cnovr_pose * pose, cnovrfocus_properties * prop, int event, int buttoninfo, int grabbutton /*default should be CTRLA_PINCHBTN*/ );

//Expects Opaque to be a model which has a pose assigned.
int CNOVRFocusDefaultFocusEvent( int event, cnovrfocus_capture * cap, cnovrfocus_properties * prop, int buttoninfo );

cnovrfocus_properties * CNOVRFocusGetPropsForDev( int ctrl );

/* If you want to manually hook the event...

cnovr_pose some_pose;
cnovr_model_focus_controller focuscontrol;

void CollisionChecker( void * tag, void * opaquev )
{
	//(2)
	cnovrfocus_properties * p = (cnovrfocus_properties*)opaquev;
	//tag is tag
	//Use 'p' to figure out where the collision happened.
	CNOVRFocusRespond( focuscontrol.focusevent, 60.0 ); //We're just returning 60.0
}

int EventChecker( int event, cnovrfocus_capture * cap, cnovrfocus_properties * prop, int buttoninfo )
{
	//(3)
	void * tag = (void *)cap->opaque;

	//You can intercept events here and return to prevent them from being passed to CNOVRGeneralHandleFocusEvent

	CNOVRGeneralHandleFocusEvent( &focuscontrol, &some_pose, prop, event, buttoninfo, 3 );
	return 0;
}

cnovrfocus_capture focuseventdata = { 0, 0, 0, EventChecker };

void SetupFocusHandler()
{
	memset( &focuscontrol, 0, sizeof(focuscontrol) );
	//if( fc->focusevent ) CNOVRListDeleteTag( tag ); 
	focuseventdata.tcctag = GetTCCTag();
	focuscontrol.focusevent = &focuseventdata;
	CNOVRListAdd( cnovrLCollide, 0, CollisionChecker );  //(1) '0' is 'tag'
}


*/


#endif

