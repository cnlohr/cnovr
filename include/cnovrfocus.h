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
	CTRLA_MAX,
} ControllerActions;

struct cnovrfocus_capture_t;
typedef struct cnovrfocus_capture_t cnovrfocus_capture;
struct cnovrfocus_controller_t;
typedef struct cnovrfocus_controller_t cnovrfocus_controller;


//We have "Check" (Checks everything in the list).  It is called on motion, not on presses.
// the callback looks like:
//   typedef void(cnovr_cb_fn)( void * tag, cnovrfocus_controller * controller );
// If you, as a subscriber have interest in this event, call 
//
// If you've claimed interest you'll get:
//    In, Out, Motion, Down, UpNoFocus
// If you have focus, you'll get:
//    Drag, UpFocus

//We need to claim 

typedef enum
{
	CNOVRF_IN = 0,
	CNOVRF_OUT,
	CNOVRF_MOTION,
	CNOVRF_DOWN,
	CNOVRF_UPFOCUS,
	CNOVRF_UPNOFOCUS,
	CNOVRF_DRAG,
	CNOVRF_MAX_EVENTS,
} cnovrfocus_event;

typedef int (*CNOVRFocusEvent)( int event, cnovrfocus_capture * cap, cnovrfocus_controller * control );

struct cnovrfocus_capture_t
{
	void * tag;
	void * opaque;
	CNOVRFocusEvent cb;
};

typedef struct cnovrfocus_controller_t
{
	uint64_t buttonmask[BUTTONMASKSIZE];
	cnovr_pose poseRender;
	cnovr_pose poseFuture;
	cnovrfocus_capture * captured;
	int devid;
} cnovrfocus_controller;

void CNOVRFocusClaim( int devid, cnovrfocus_capture ce, double claimeddistance, int attempt_focus );
void CNOVRFocusRemoveTag( void * tag );
void CNOVRFocusNewState( cnovrfocus_controller * newcontroller );

/* 
  Process

   OpenVR -> 
      For Each Controller -> CNOVRFocusNewState
          CNOVRFocusNewState() -> CNOVRListCall( cnovrLCollide, ... ); 

   Task Callback ->
      CNOVRFocusClaim( ... )

//Private
//void CNOVRFocusSetup();
//void CNOVRFocusShutdown();

//INTERNAL
typedef struct cnovrfocus_t
{
	
} cnovrfocus;

extern cnovrfocus * focussystem;

*/

#endif

