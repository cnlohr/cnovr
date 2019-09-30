#ifndef _CNOVR_FOCUS_H
#define _CNOVR_FOCUS_H

//The idea behind this is a general purpose focus system for multiple controllers.

#include <cnovrmath.h>
#include <stdint.h>

#define CNOVRFOCUSMAXDEVS 16
#define BUTTONMASKSIZE    2 //(16 * 8 buttons)
//Dev 0 = HMD / Keyboard
//Dev 1 = Controller Left
//Dev 2 = Controller Right

typedef enum
{
	CTRLA_TRIGGER = 0,
	CTRLA_BUTTONA,
	CTRLA_BUTTONB,
	CTRLA_GRASP, //Last "button"
	CTRLA_TRIG, //Special
	CTRLA_HAND, //Special
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

typedef struct cnovrfocus_properties_t
{
	int devid;
	uint64_t buttonmask[BUTTONMASKSIZE];
	cnovr_pose poseTip;
	cnovrfocus_capture * capturedFocus;
	cnovrfocus_capture * capturedPassive;
	float capturedPassiveDistance;

	//This is transitory information stored when excuting the collision list.  Don't mess with it unless you know what you're doing.
	cnovrfocus_capture * NewCapturedPassive;
	float NewPassiveRealDistance;
} cnovrfocus_properties;

//Ugh this is awkward.  Need to fix.

//YOU own the 'ce' object. We just store it for you.
void CNOVRFocusRespond( int devid, cnovrfocus_capture * ce, float realdistance, int attempt_focus );
void CNOVRFocusRemoveTag( void * tag );


#endif

