#include "cnovrfocus.h"
#include <cnovrutil.h>
#include <cnovr.h>
#include <chew.h>
#include <cnovrtccinterface.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openvr_capi.h>
#include <string.h>

typedef struct internal_focus_system_t
{
	VRActionSetHandle_t inputactionset;
	VRInputValueHandle_t handsource[CNOVRINPUTDEVS];
	VRActionHandle_t actionhandles[CNOVRINPUTDEVS][CTRLA_MAX];
	InputOriginInfo_t originInfo[CNOVRINPUTDEVS];
	InputPoseActionData_t poseData[CNOVRINPUTDEVS];
	cnovr_pose      poseController[CNOVRINPUTDEVS];
	int bShowController[CNOVRINPUTDEVS];

	cnovr_model * mdlPointer;
	cnovr_model * mdlHitPos;
	cnovr_shader * shdPointer;


	cnovr_pose      poseTip[CNOVRINPUTDEVS];
	cnovrfocus_properties focusProps[CNOVRINPUTDEVS];	//Tricky: Device0 is the HMD, 1 and 2 are the controllers.
	og_mutex_t    mutFocus;
	
	cnovrfocus_capture * capPassiveTemp; //Careful - if we delete in the operation, this must also be removed.
	cnovrfocus_capture * capFocusTemp; //Careful - if we delete in the operation, this must also be removed.

	int current_devid; //If we're actively pursuing a callback. 
} internal_focus_system;

internal_focus_system FOCUS;

VRActionHandle_t CNOVRFocusGetVRActionHandleFromConrollerAndCtrlA( int dev, int ctrl )
{
	return FOCUS.actionhandles[dev][ctrl];
}

cnovr_pose * CNOVRFocusGetTipPose( int device )
{
	if( device < 0 || device > sizeof( FOCUS.poseTip ) / sizeof( FOCUS.poseTip[0] ) ) return 0;
	return FOCUS.poseTip + device;
}

void FocusSystemRender( void * tag, void * opaque )
{
	internal_focus_system * f = &FOCUS;
#if 0
	if( f->shdRenderModel )
	{
		int i;
		CNOVRRender( f->shdRenderModel );
		for( i = 0; i < CNOVRINPUTDEVS; i++ )
		{
			if( !FOCUS.bShowController[i] ) continue;
			cnovr_model * m = f->mdlRenderModels[i];
			if( m )
			{
				CNOVRModelRenderWithPose( m, &f->poseController[i] );
			}
		}
	}
#endif
	if( f->shdPointer )
	{
		int i;
		CNOVRRender( f->shdPointer );
		cnovr_model * mp = f->mdlPointer;
		cnovr_model * mh = f->mdlHitPos;
		
		for( i = 0; i < CNOVRINPUTDEVS; i++ )
		{
			if( !FOCUS.bShowController[i] ) continue;
			cnovrfocus_properties * props = f->focusProps + i;

			if( mp )
			{
				glUniform4f( 20, props->capturedPassiveDistance, 0, 0, 0 );
				CNOVRModelRenderWithPose( mp, &props->poseTip );
			}

			if( props->capturedPassiveDistance >= CNOVRFOCUS_FAR ) continue;

			if( mh )
			{
				glUniform4f( 20, props->capturedPassiveDistance, 1, 0, 0 );
				cnovr_pose pintersect = {.Rot = {1.0, 0.0, 0.0, 0.0}, .Scale = 1, .Pos = { 0.0, 0.0, props->capturedPassiveDistance } };
				apply_pose_to_pose( &pintersect, &props->poseTip, &pintersect);
				CNOVRModelRenderWithPose( mh, &pintersect );
			}
		}
	}
}




//XXX TODO: We may want to capture fUpdateTime from actionData.
int GetDigitalActionData( VRActionHandle_t h )
{
	if( !cnovrstate->oInput ) return -1;
	InputDigitalActionData_t actionData;
	int ret;
	if( ( ret = cnovrstate->oInput->GetDigitalActionData( h, &actionData, sizeof(actionData), k_ulInvalidInputValueHandle ) ) ) return -ret;
	return actionData.bActive && actionData.bState;
}

float GetAnalogActionData( VRActionHandle_t h )
{
	if( !cnovrstate->oInput ) return -1;
	InputAnalogActionData_t actionData;
	if( cnovrstate->oInput->GetAnalogActionData( h, &actionData, sizeof(actionData), k_ulInvalidInputValueHandle ) ) return -2;
	return actionData.bActive?actionData.x:0;
}

cnovrfocus_properties * CNOVRFocusGetPropsForDev( int ctrl )
{
	return &FOCUS.focusProps[ctrl];
}

void InternalCNOVRFocusUpdate()
{
	int ctrl = 0;
	int r;

	if( !cnovrstate->oInput ) return;
	VRActiveActionSet_t actionSet = { 0 };
	actionSet.ulActionSet = FOCUS.inputactionset;
	cnovrstate->oInput->UpdateActionState( &actionSet, sizeof( actionSet ), 1 );

	for( ; ctrl < CNOVRINPUTDEVS; ctrl++ )
	{
		cnovrfocus_properties * props = &FOCUS.focusProps[ctrl];
		cnovrfocus_capture * cap;

		FOCUS.capFocusTemp = props->capturedFocus;
		FOCUS.current_devid = ctrl;

		int i;
		for( i = 0 ; i <= CTRLA_GRASP; i++ )
		{
			int r;
			VRActionHandle_t h = FOCUS.actionhandles[ctrl][i];
			if( h == k_ulInvalidActionHandle ) continue;
			if( ( r = GetDigitalActionData( h ) ) < 0 ) { printf( "Err %d on %d\n", r, i ); continue; }
			int wasdown = (props->buttonmask[0] & 1<<i)?1:0;
			if( wasdown != r )
			{
				if( r )
				{
					props->buttonmask[0] |= 1<<i;
					OGLockMutex( FOCUS.mutFocus );
					if( ( cap = props->capturedFocus   ) ) { TCCInvocation( cap->tcctag, cap->cb( CNOVRF_DOWNFOCUS, cap, props, i ) ); }
					if( ( cap = props->capturedPassive ) ) { TCCInvocation( cap->tcctag, cap->cb( CNOVRF_DOWNNOFOCUS, cap, props, i ) ); }
					OGUnlockMutex( FOCUS.mutFocus );
				}
				else
				{
					props->buttonmask[0] &= ~(1<<i);
					OGLockMutex( FOCUS.mutFocus );
					if( ( cap = props->capturedFocus   ) ) { TCCInvocation( cap->tcctag, cap->cb( CNOVRF_UPFOCUS, cap, props, i ) ); }
					if( ( cap = props->capturedPassive ) ) { TCCInvocation( cap->tcctag, cap->cb( CNOVRF_UPNOFOCUS, cap, props, i ) ); }
					OGUnlockMutex( FOCUS.mutFocus );
				}
			}
		}

		//All updates should be based off of "tip"
		InputPoseActionData_t * p = &FOCUS.poseData[ctrl];
		int ret = cnovrstate->oInput->GetPoseActionDataForNextFrame( FOCUS.actionhandles[ctrl][CTRLA_TIP], 
			ETrackingUniverseOrigin_TrackingUniverseStanding, p, sizeof( *p ), k_ulInvalidInputValueHandle ); 
		bool bPoseIsValid = false;
		bool bActive = false;

		if( ctrl == 0 )
		{
			bActive = false; //XXX TODO: Make the HMD a focusable thing.
			bPoseIsValid = cnovrstate->bTrackedPosesValid[0];
			cnovr_pose point_from_mouth = (cnovr_pose){.Rot = {1.0, 0.0, 0.0, 0.0}, .Scale = 1, .Pos = { 0.0, -0.1, 0.0 } };
			apply_pose_to_pose( &FOCUS.poseTip[ctrl], cnovrstate->pTrackedPoses, &point_from_mouth );
		}
		else
		{
			bActive = p->bActive;
			bPoseIsValid = p->pose.bPoseIsValid;
			CNOVRPoseFromHMDMatrix( &FOCUS.poseTip[ctrl], &p->pose.mDeviceToAbsoluteTracking );
		}

		//Tricky: we rotate 180 out so Z+ is is forward. TODO should this be around X or Y?  Are we switching coordinate systems?
		quatrotate180X( FOCUS.poseTip[ctrl].Rot );

		if( !ret && bActive && bPoseIsValid )
		{
			memcpy( &props->poseTip, &FOCUS.poseTip[ctrl], sizeof( cnovr_pose ) );

			FOCUS.capPassiveTemp = props->capturedPassive;
	
			OGLockMutex( FOCUS.mutFocus );
			props->NewCapturedPassive = 0;
			props->NewPassiveRealDistance = CNOVRFOCUS_FAR;
			CNOVRListCall( cnovrLCollide, props, 0 );
			props->capturedPassive = props->NewCapturedPassive;
			props->capturedPassiveDistance = props->NewPassiveRealDistance;
			OGUnlockMutex( FOCUS.mutFocus );

			OGLockMutex( FOCUS.mutFocus );
			if( FOCUS.capPassiveTemp != props->capturedPassive )
			{
				if( FOCUS.capPassiveTemp )
				{
					TCCInvocation( FOCUS.capPassiveTemp->tcctag, FOCUS.capPassiveTemp->cb( CNOVRF_OUT, FOCUS.capPassiveTemp, props, 0 ) );
				}
				if( props->capturedPassive )
				{
					TCCInvocation( props->capturedPassive->tcctag, props->capturedPassive->cb( CNOVRF_IN, props->capturedPassive, props, 0 ) );
				}
			}
			if( ( cap = props->capturedFocus   ) ) { TCCInvocation( cap->tcctag, cap->cb( CNOVRF_DRAG, cap, props, 0 ) ); }
			if( ( cap = props->capturedPassive ) ) { TCCInvocation( cap->tcctag, cap->cb( CNOVRF_MOTION, cap, props, 0 ) ); }
			OGUnlockMutex( FOCUS.mutFocus );
		}

		//Render model updates, etc. can be based off of "hand"
		ret = cnovrstate->oInput->GetPoseActionDataForNextFrame( FOCUS.actionhandles[ctrl][CTRLA_MODEL], 
			ETrackingUniverseOrigin_TrackingUniverseStanding, p, sizeof( *p ), k_ulInvalidInputValueHandle ); 
		if( ret || !p->bActive || !p->pose.bPoseIsValid )
		{
			FOCUS.bShowController[ctrl] = 0;
		}
		else
		{
#if 0
			CNOVRPoseFromHMDMatrix( &FOCUS.poseController[ctrl], &p->pose.mDeviceToAbsoluteTracking );

			InputOriginInfo_t * o = &FOCUS.originInfo[ctrl];

			if ( cnovrstate->oInput->GetOriginTrackedDeviceInfo( p->activeOrigin, o, sizeof( *o ) ) == EVRInputError_VRInputError_None 
				&& o->trackedDeviceIndex != k_unTrackedDeviceIndexInvalid )
			{
				if( FOCUS.rendermodelnames[ctrl] == 0 || FOCUS.bShowController[ctrl] == 0 )
				{
					char * rmname = CNOVRGetTrackedDeviceString( o->trackedDeviceIndex, ETrackedDeviceProperty_Prop_RenderModelName_String );
					if( rmname && (!FOCUS.rendermodelnames[ctrl] || strcmp( FOCUS.rendermodelnames[ctrl], rmname ) != 0 ) )
					{
						int rmlen = strlen( rmname );
						if( rmlen < 128 )
						{
							if( FOCUS.rendermodelnames[ctrl] ) free(  FOCUS.rendermodelnames[ctrl] );
							rmname = FOCUS.rendermodelnames[ctrl] = strdup( rmname );
							cnovr_model * m = FOCUS.mdlRenderModels[ctrl];
							if( !m ) m = FOCUS.mdlRenderModels[ctrl] = CNOVRModelCreate( 0, GL_TRIANGLES );
							char rmname2[256];
							sprintf( rmname2, "%s.rendermodel", rmname );
							CNOVRModelLoadFromFileAsync( m, rmname2 );
						}
					}
				}
			}
#endif
			FOCUS.bShowController[ctrl] = 1;
		}

		if( FOCUS.capFocusTemp != props->capturedFocus )
		{
			OGLockMutex( FOCUS.mutFocus );
			if( ( cap = FOCUS.capFocusTemp ) ) { TCCInvocation( cap->tcctag, cap->cb( CNOVRF_LOSTFOCUS, cap, props, i ) ); }
			if( ( cap = props->capturedFocus ) ) { TCCInvocation( cap->tcctag, cap->cb( CNOVRF_ACQUIREDFOCUS, cap, props, i ) ); }
			OGUnlockMutex( FOCUS.mutFocus );
		}
		FOCUS.current_devid = -1;
	}
}

void InternalCNOVRFocusShutdown()
{
	OGDeleteMutex( FOCUS.mutFocus );
}


void InternalCNOVRFocusPrerenderStartup()
{
	FOCUS.mdlPointer = CNOVRModelCreate( 0, GL_TRIANGLES );
	FOCUS.mdlHitPos = CNOVRModelCreate( 0, GL_TRIANGLES );
	CNOVRModelAppendCube( FOCUS.mdlHitPos, (cnovr_point3d){ .0055, .0055, .0055 }, 0, 0 );
	CNOVRModelAppendCube( FOCUS.mdlPointer, (cnovr_point3d){ .002, .002, CNOVRFOCUS_FAR }, 0, 0 );
	FOCUS.shdPointer = CNOVRShaderCreate( "pointer" );	
	//FOCUS.shdRenderModel = CNOVRShaderCreate( "rendermodel" );
}


void InternalCNOVRFocusSetup()
{
	int ctrl, i;

	FOCUS.mutFocus = OGCreateMutex();

	for( i = 0; i < CNOVRINPUTDEVS; i++ )
	{
		cnovrfocus_properties * p = &FOCUS.focusProps[i];
		memset( p, 0, sizeof( *p ) );
		p->devid = i;
		pose_make_identity( &p->poseTip );
	}

	for( ctrl = 0; ctrl < CNOVRINPUTDEVS; ctrl++ )
	{
		for( i = 0; i < CTRLA_MAX; i++ )
		{
			FOCUS.actionhandles[ctrl][i] = k_ulInvalidActionHandle;
		}
	}

	if( cnovrstate->oInput )
	{
		char * manifestpath = CNOVRFileSearchAbsolute( "default_actions.json" );
		if( !manifestpath )
		{
			ovrprintf( "FAULT: Could not find action manifest path.\n" );
		}
		else
		{
			printf( "Setting up action manifests (%s)\n", manifestpath );
			cnovrstate->oInput->SetActionManifestPath( manifestpath );

			char stmp[1024];
			int ret;

			if( ( ret = cnovrstate->oInput->GetActionSetHandle( "/actions/m", &FOCUS.inputactionset) ) )
			{
				printf( "ERROR: Failed to get action set handle: %d\n", ret );
			}

			ctrl = 0;
			{
				//For the HMD
				if( ( ret = cnovrstate->oInput->GetInputSourceHandle( "/user/head", &FOCUS.handsource[ctrl] ) ) < 0 ) printf( "Error %d\n", ret );
				cnovrstate->oInput->GetActionHandle( "/actions/m/in/trigclickhead", &FOCUS.actionhandles[ctrl][CTRLA_TRIGGER] );
				cnovrstate->oInput->GetActionHandle( "/actions/m/in/modelhead", &FOCUS.actionhandles[ctrl][CTRLA_MODEL] );
				cnovrstate->oInput->GetActionHandle( "/actions/m/in/tiphead", &FOCUS.actionhandles[ctrl][CTRLA_TIP] );
			}

			for( ctrl = 1; ctrl < CNOVRINPUTDEVS; ctrl++ )
			{
				const char * hand = (ctrl == CNOVRINPUTDEVS-1)?"right":"left";
				sprintf( stmp, "/user/hand/%s", hand );
				if( ( ret = cnovrstate->oInput->GetInputSourceHandle( stmp, &FOCUS.handsource[ctrl] ) ) < 0 ) printf( "Error %d\n", ret );

				static const char * eventnames[] = {
					"/actions/m/in/trigclick",
					"/actions/m/in/buttona",
					"/actions/m/in/buttonb",
					"/actions/m/in/pinchclick",
					"/actions/m/in/graspclick",
					"/actions/m/in/trig",
					"/actions/m/in/model",
					"/actions/m/in/tip"
				};

				for( i = 0; i < CTRLA_MAX; i++ )
				{
					sprintf( stmp, "%s%s", eventnames[i], hand );
					int ret = cnovrstate->oInput->GetActionHandle( stmp, &FOCUS.actionhandles[ctrl][i] );
					if( ret )
					{
						ovrprintf( "Error getting handle to action %s (%d)\n", stmp, ret );
					}
					else
					{
						ovrprintf( "Got action: %s / %llx\n", stmp, FOCUS.actionhandles[ctrl][i] );
					}
				}
			}
		}
	}

	CNOVRListAdd( cnovrLRender2, &FOCUS, FocusSystemRender );
	CNOVRJobTack( cnovrQPrerender, InternalCNOVRFocusPrerenderStartup, &FOCUS, 0, 1 );
}


//This is still kind of awkward.  Probably could use some fixing.
//Also, considerin switching it up so parts of this function live as a #define for speed.
void CNOVRFocusRespond( cnovrfocus_capture * ce, float realdistance, float * fdprops )
{
	int cdevid = FOCUS.current_devid;
	if( cdevid < 0 ) return;
	cnovrfocus_properties * fp = FOCUS.focusProps + cdevid;
	if( realdistance < fp->NewPassiveRealDistance )
	{
		fp->NewCapturedPassive = ce;
		fp->NewPassiveRealDistance = realdistance;
		if( fdprops ) memcpy( fp->NewPassiveProps, fdprops, sizeof( fp->NewPassiveProps ) );
	}
}

void CNOVRFocusAcquire( cnovrfocus_capture * ce, int wantfocus )
{
	int cdevid = FOCUS.current_devid;
	if( cdevid < 0 ) return;
	cnovrfocus_properties * fp = FOCUS.focusProps + cdevid;
	if( !wantfocus )
	{
		if( ce == fp->capturedFocus )
			fp->capturedFocus = 0;
	}
	else if( !fp->capturedFocus )
	{
		fp->capturedFocus = ce;
	}
}


void CNOVRFocusRemoveTag( void * tag )
{
	printf( "Removing tag START: %p\n", tag );
	OGLockMutex( FOCUS.mutFocus );
	if( FOCUS.capPassiveTemp && FOCUS.capPassiveTemp->tag == tag ) FOCUS.capPassiveTemp = 0;
	if( FOCUS.capFocusTemp && FOCUS.capFocusTemp->tag == tag ) FOCUS.capFocusTemp = 0;
	int i = 0;
	for( i = 0; i < CNOVRINPUTDEVS; i++ )
	{
		cnovrfocus_properties * p = &FOCUS.focusProps[i];
		cnovrfocus_capture ** ct[CNOVRINPUTDEVS] = { 
			&p->capturedFocus,
			&p->capturedPassive,
			&p->NewCapturedPassive };
		int j;
		for( j = 0; j < CNOVRINPUTDEVS; j++ )
		{
			cnovrfocus_capture * c = *ct[j];
			if( c && c->tag == tag ) *ct[j] = 0;
		}
	}
	OGUnlockMutex( FOCUS.mutFocus );
	printf( "Tag removal complete.\n" );
}


static void ModelFocusCollideFunction(void * tag, void * opaquev )
{
	//XXX TODO: This needs to be optimized! There's a lot we can do to improve its performance.
	cnovrfocus_properties * p = (cnovrfocus_properties*)opaquev;
	cnovr_model * m = tag;
	if( !m ) return;
	cnovr_model_focus_controller * fc = m->focuscontrol;
	if( !fc ) return;
	if( !fc->focusevent ) return;
	cnovr_point3d start = { 0, 0, 0 };
	cnovr_vec3d direction = { 0, 0, 1 };
	cnovr_pose invertedxform;
	pose_invert( &invertedxform, m->pose );
	apply_pose_to_point( start, &p->poseTip, start);
	apply_pose_to_point( start, &invertedxform, start);
	apply_pose_to_point( direction, &p->poseTip, direction);
	apply_pose_to_point( direction, &invertedxform, direction);
	sub3d( direction, direction, start );
	cnovr_collide_results res;
	res.t = p->NewPassiveRealDistance;
	int r = CNOVRModelCollide( m, start, direction, &res, 0, 0 );
	if( r >= 0 && res.t > 0 )
	{
		CNOVRFocusRespond( fc->focusevent, res.t, res.collidevs );
	}
}

void CNOVRModelSetInteractable( cnovr_model * m, cnovrfocus_capture * focusevent )
{
	cnovr_model_focus_controller * fc = m->focuscontrol;
	if( !fc ) {
		m->focuscontrol = fc = malloc( sizeof( cnovr_model_focus_controller ) );
		memset( fc, 0, sizeof(*fc) );
	}

	if( fc->focusevent )
	{
		CNOVRListDeleteTag( m ); 
	}

	fc->focusevent = focusevent;

	if( focusevent )
	{
		CNOVRListAdd( cnovrLCollide, m, ModelFocusCollideFunction );
	}
}


void CNOVRGeneralHandleFocusEvent( cnovr_model_focus_controller * fc, cnovr_pose * pose, cnovrfocus_properties * prop, int event, int buttoninfo, int dragbutton )
{
	int devid = prop->devid;
//	cnovr_model_focus_controller * fc = m->focuscontrol;
	if( !fc ) return;


	switch( event )
	{
		case CNOVRF_DOWNNOFOCUS:
			if( buttoninfo == dragbutton ) CNOVRFocusAcquire( fc->focusevent, 1 );
			break;
		case CNOVRF_DRAG:
			if( fc->focusgrab )
			{
				if( fc->focusgrab[CNOVRINPUTDEV_LEFT] && fc->focusgrab[CNOVRINPUTDEV_RIGHT] )
				{
					if( devid == CNOVRINPUTDEV_LEFT )
					{
						fc->twohandgrab_last[CNOVRINPUTDEV_LEFT] = &prop->poseTip;
					}
					else if( devid == CNOVRINPUTDEV_RIGHT && fc->twohandgrab_last[CNOVRINPUTDEV_LEFT] )
					{
						//XXX TODO: Try to make it absolute instead of progressive.

						//Tricky: We're doing a two-hand grab.
						cnovr_point3d grabpos_conroller_space1 = { 0, 0, fc->initial_grab_z[CNOVRINPUTDEV_LEFT] };
						cnovr_point3d grabpos_conroller_space2 = { 0, 0, fc->initial_grab_z[CNOVRINPUTDEV_RIGHT] };
						cnovr_point3d grabposworldspaceLeft;
						cnovr_point3d grabposworldspaceRight;
						apply_pose_to_point(grabposworldspaceLeft, fc->twohandgrab_last[CNOVRINPUTDEV_LEFT], grabpos_conroller_space1 );
						apply_pose_to_point(grabposworldspaceRight, &prop->poseTip, grabpos_conroller_space2 );

						//Ok, so we have two grab positions in world space.  How are we gonna do this?
						//Piece-meal?
						//First, fix fc->gplast[CNOVRINPUTDEV_LEFT].  Then
						// Find transformation FROM grabposworldspace2 to fc->gplast[CNOVRINPUTDEV_RIGHT]
						// Once transformation found, rotate object center, apply rotation.
						cnovr_quat rot2AB;
						cnovr_point3d fixed;
						cnovr_point3d rrpBegin;
						cnovr_point3d rrpEnd;
						cnovr_point3d rrpObjectCenter;
						float scalediff;

						int relativemotion = 1;
						int handfrom = 0;
						if( relativemotion )
						{
							for( handfrom = 0; handfrom < 2; handfrom++ )
							{
								int hand0 = handfrom==0;
								copy3d( fixed, fc->gplast[hand0?CNOVRINPUTDEV_LEFT:CNOVRINPUTDEV_RIGHT] );
								sub3d( rrpBegin, fc->gplast[hand0?CNOVRINPUTDEV_RIGHT:CNOVRINPUTDEV_LEFT], fixed );
								sub3d( rrpEnd, hand0?grabposworldspaceRight:grabposworldspaceLeft, fixed );
								scalediff = magnitude3d( rrpEnd ) / magnitude3d( rrpBegin );
								quatfrom2vectors( rot2AB, rrpBegin, rrpEnd );
								scale3d( rrpEnd, rrpEnd, scalediff );
								//Now, apply this rotation to the object center.
								sub3d( rrpObjectCenter, fc->pose_internal.Pos, fixed );
								quatrotatevector( rrpObjectCenter, rot2AB, rrpObjectCenter );
								scale3d( rrpObjectCenter, rrpObjectCenter, scalediff );
								add3d( fc->pose_internal.Pos, rrpObjectCenter, fixed );
								fc->pose_internal.Scale *= scalediff;
								//Now, apply the rotation to the quaternion.
								quatrotateabout( fc->pose_internal.Rot, rot2AB, fc->pose_internal.Rot );
							}
							memcpy( pose, &fc->pose_internal, sizeof( cnovr_pose ) );
							copy3d( fc->gplast[CNOVRINPUTDEV_LEFT], grabposworldspaceLeft );
							copy3d( fc->gplast[CNOVRINPUTDEV_RIGHT], grabposworldspaceRight );
						}
						else
						{
							//Currently behaves undesirably.
							cnovr_pose cumulative_pose_change;
							memcpy( &cumulative_pose_change, &fc->pose_internal, sizeof( cnovr_pose ) );
							for( handfrom = 0; handfrom < 2; handfrom++ )
							{
								//Absolute control
								int hand0 = handfrom==0;
								copy3d( fixed, fc->gplast[hand0?CNOVRINPUTDEV_LEFT:CNOVRINPUTDEV_RIGHT] );
								sub3d( rrpBegin, fc->gplast[hand0?CNOVRINPUTDEV_RIGHT:CNOVRINPUTDEV_LEFT], fixed );
								sub3d( rrpEnd, hand0?grabposworldspaceRight:grabposworldspaceLeft, fixed );
								scalediff = magnitude3d( rrpEnd ) / magnitude3d( rrpBegin );
								quatfrom2vectors( rot2AB, rrpBegin, rrpEnd );
								scale3d( rrpEnd, rrpEnd, scalediff );
								//Now, apply this rotation to the object center.
								sub3d( rrpObjectCenter, cumulative_pose_change.Pos, fixed );
								quatrotatevector( rrpObjectCenter, rot2AB, rrpObjectCenter );
								scale3d( rrpObjectCenter, rrpObjectCenter, scalediff );
								add3d( cumulative_pose_change.Pos, rrpObjectCenter, fixed );
								cumulative_pose_change.Scale = cumulative_pose_change.Scale * scalediff;
								//Now, apply the rotation to the quaternion.
								quatrotateabout( cumulative_pose_change.Rot, rot2AB, cumulative_pose_change.Rot );
							}
							memcpy( pose, &cumulative_pose_change, sizeof( cnovr_pose ) );
						}

						//Sanity Check
				//		scale3d( rrpEnd, rrpEnd, scalediff );
				//		quatrotatevector( rrpEnd, rot2AB, rrpBegin );
				//		printf( " OUT: %f %f %f  -> %f %f %f\n", PFTHREE( rrpBegin ), PFTHREE(rrpEnd ) );

						//Now, do exactly the same thing, but reverse the point roles.
				/*		copy3d( fixed, fc->gplast[CNOVRINPUTDEV_RIGHT] );
						sub3d( rrpBegin, fc->gplast[CNOVRINPUTDEV_LEFT], fixed );
						sub3d( rrpEnd, grabposworldspaceLeft, fixed );
						scalediff = magnitude3d( rrpEnd ) / magnitude3d( rrpBegin );
						quatfrom2vectors( rot2AB, rrpBegin, rrpEnd );
						//Now, apply this rotation to the object center.
						sub3d( rrpObjectCenter, pose->Pos, fixed );
						scale3d( rrpObjectCenter, rrpObjectCenter, scalediff );
						quatrotatevector( rrpObjectCenter, rot2AB, rrpObjectCenter );
						add3d( pose->Pos, rrpObjectCenter, fixed );
						pose->Scale *= scalediff;
						//Now, apply the rotation to the quaternion.
						quatrotateabout( pose->Rot, rot2AB, pose->Rot );*/

						fc->twohandgrab_last[CNOVRINPUTDEV_RIGHT] = &prop->poseTip;
					}
				}
				else if( fc->focusgrab[devid] )
				{
					//Regular 1-hand grab.
					float keepscale = pose->Scale;
					cnovr_pose * p = fc->focusgrab[devid];
					apply_pose_to_pose( pose, &prop->poseTip, p );
					pose->Scale = keepscale;

					//Just in case we click another controller, we also want to store what the other hand would be.
					float z = fc->initial_grab_z[devid] = prop->NewPassiveRealDistance;
					cnovr_point3d grabpos_conroller_space = { 0, 0, z };
					apply_pose_to_point( fc->gplast[devid], &prop->poseTip, grabpos_conroller_space );
				}
			}
			break;
		case CNOVRF_UPFOCUS:
			if( buttoninfo == dragbutton ) CNOVRFocusAcquire( fc->focusevent, 0 );
			break;
		case CNOVRF_LOSTFOCUS:
			if( fc->focusgrab && fc->focusgrab[devid] )
			{
				if( fc->twohandgrab_last[devid] ) fc->twohandgrab_last[devid] = 0;
				fc->focusgrab[devid] = 0; //fc->twohandgrab_first = 0; //fc->twohandgrab_initialdist = -1;
				//Make sure if we were doing a two-hand grab, we release both.
				if( devid == CNOVRINPUTDEV_LEFT && fc->focusgrab[CNOVRINPUTDEV_RIGHT] )
				{
					unapply_pose_from_pose( fc->focusgrab[CNOVRINPUTDEV_RIGHT], fc->twohandgrab_last[CNOVRINPUTDEV_RIGHT], pose );
				}
				if( devid == CNOVRINPUTDEV_RIGHT && fc->focusgrab[CNOVRINPUTDEV_LEFT] )
				{
					unapply_pose_from_pose( fc->focusgrab[CNOVRINPUTDEV_LEFT], fc->twohandgrab_last[CNOVRINPUTDEV_LEFT], pose );
				}
			}
			break;
		case CNOVRF_ACQUIREDFOCUS:
		{
			cnovr_pose * posein;
			if( !fc->focusgrab ) fc->focusgrab = calloc( CNOVRINPUTDEVS, sizeof( cnovr_pose * ) );

			fc->twohandgrab_last[devid] = &prop->poseTip;

			cnovr_pose * poseout =  fc->focusgrab[devid];

			if( !poseout )
				poseout = fc->focusgrab[devid] = malloc( sizeof( cnovr_pose ) );

			float z = fc->initial_grab_z[devid] = prop->NewPassiveRealDistance;
			cnovr_point3d grabpos_conroller_space = { 0, 0, z };
			apply_pose_to_point( fc->gplast[devid], &prop->poseTip, grabpos_conroller_space );
			unapply_pose_from_pose( fc->focusgrab[devid], &prop->poseTip, pose );

			if( fc->twohandgrab_last[CNOVRINPUTDEV_LEFT] && fc->twohandgrab_last[CNOVRINPUTDEV_RIGHT] )
			{
				//Use this as the starting property for the two-handed grab.
				memcpy( &fc->pose_internal, pose, sizeof( cnovr_pose ) );
			}
			break;
		}
	}
}



int CNOVRFocusDefaultFocusEvent( int event, cnovrfocus_capture * cap, cnovrfocus_properties * prop, int buttoninfo )
{
	cnovr_model * m = cap->opaque;
	CNOVRGeneralHandleFocusEvent( m->focuscontrol, m->pose, prop, event, buttoninfo, CTRLA_PINCHBTN );
	return 0;
}




