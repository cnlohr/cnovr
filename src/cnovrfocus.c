#include "cnovrfocus.h"
#include <cnovrutil.h>
#include <cnovr.h>
#include <chew.h>
#include <cnovrtccinterface.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openvr_capi.h>

typedef struct internal_focus_system_t
{
	VRActionSetHandle_t inputactionset;
	VRInputValueHandle_t handsource[CNOVRINPUTDEVS];
	VRActionHandle_t actionhandles[CNOVRINPUTDEVS][CTRLA_MAX];
	InputOriginInfo_t originInfo[CNOVRINPUTDEVS];
	InputPoseActionData_t poseData[CNOVRINPUTDEVS];
	char * rendermodelnames[CNOVRINPUTDEVS];
	cnovr_shader * shdRenderModel;
	cnovr_model  * mdlRenderModels[CNOVRINPUTDEVS];
	cnovr_pose      poseController[CNOVRINPUTDEVS];
	bool bShowController[CNOVRINPUTDEVS];

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

cnovr_pose * CNOVRFocusGetTipPose( int device )
{
	if( device < 0 || device > sizeof( FOCUS.poseTip ) / sizeof( FOCUS.poseTip[0] ) ) return 0;
	return FOCUS.poseTip + device;
}

void FocusSystemRender( void * tag, void * opaque )
{
	internal_focus_system * f = &FOCUS;
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
							if( !m ) m = FOCUS.mdlRenderModels[ctrl] = CNOVRModelCreate( 0, 0, GL_TRIANGLES );
							char rmname2[256];
							sprintf( rmname2, "%s.rendermodel", rmname );
							CNOVRModelLoadFromFileAsync( m, rmname2 );
						}
					}
				}
			}
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
	FOCUS.mdlPointer = CNOVRModelCreate( 0, 3, GL_TRIANGLES );
	FOCUS.mdlHitPos = CNOVRModelCreate( 0, 3, GL_TRIANGLES );
	CNOVRModelAppendCube( FOCUS.mdlHitPos, (cnovr_point3d){ .02, .02, .02 }, 0, 0 );
	CNOVRModelAppendCube( FOCUS.mdlPointer, (cnovr_point3d){ .01, .01, CNOVRFOCUS_FAR }, 0, 0 );
	FOCUS.shdPointer = CNOVRShaderCreate( "pointer" );	
	FOCUS.shdRenderModel = CNOVRShaderCreate( "rendermodel" );
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
		char * manifestpath = FileSearchAbsolute( "default_actions.json" );
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

	CNOVRListAdd( cnovrLRender, &FOCUS, FocusSystemRender );
	CNOVRJobTack( cnovrQPrerender, InternalCNOVRFocusPrerenderStartup, &FOCUS, 0, 1 );
}


//This is still kind of awkward.  Probably could use some fixing.
//Also, considerin switching it up so parts of this function live as a #define for speed.
void CNOVRFocusRespond( cnovrfocus_capture * ce, float realdistance )
{
	int cdevid = FOCUS.current_devid;
	if( cdevid < 0 ) return;
	cnovrfocus_properties * fp = FOCUS.focusProps + cdevid;
	if( realdistance < fp->NewPassiveRealDistance )
	{
		fp->NewCapturedPassive = ce;
		fp->NewPassiveRealDistance = realdistance;
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

