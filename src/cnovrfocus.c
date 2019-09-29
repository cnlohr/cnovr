#include "cnovrfocus.h"
#include <cnovrutil.h>
#include <cnovr.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openvr_capi.h>

typedef struct internal_focus_system_t
{
	VRActionSetHandle_t inputactionset;
	VRInputValueHandle_t handsource[2];
	VRActionHandle_t actionhandles[2][CTRLA_MAX];
	InputOriginInfo_t originInfo[2];
	InputPoseActionData_t poseData[2];
	char * rendermodelnames[2];
	cnovr_shader * shdRenderModel;
	cnovr_model  * mdlRenderModels[2];
	cnovr_pose      poseController[2];
	cnovr_pose      poseTip[2];
	bool bShowController[2];
	cnovrfocus_properties focusProps[3];	//Tricky: Device0 is the HMD, 1 and 2 are the controllers.
	og_mutex_t    mutFocus;
	cnovrfocus_capture * capPassiveTemp; //Careful - if we delete in the operation, this must also be removed.
	int current_devid; //If we're actively pursuing a callback. 
} internal_focus_system;

internal_focus_system FOCUS;

void FocusSystemRender( void * tag, void * opaque )
{
	internal_focus_system * f = &FOCUS;
	if( f->shdRenderModel )
	{
		int i;
		for( i = 0; i < 2; i++ )
		{
			CNOVRRender( f->shdRenderModel );
			cnovr_model * m = f->mdlRenderModels[i];
			if( m )
			{
				CNOVRModelRenderWithPose( m, &f->poseController[i] );
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

	for( ; ctrl < 2; ctrl++ )
	{
		cnovrfocus_properties * props = &FOCUS.focusProps[ctrl];
		cnovrfocus_capture * cap;
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
					if( ( cap = props->capturedFocus   ) ) cap->cb( CNOVRF_DOWNFOCUS, cap, props, i );
					if( ( cap = props->capturedPassive ) ) cap->cb( CNOVRF_DOWNNOFOCUS, cap, props, i );
					OGUnlockMutex( FOCUS.mutFocus );
				}
				else
				{
					props->buttonmask[0] &= ~(1<<i);
					OGLockMutex( FOCUS.mutFocus );
					if( ( cap = props->capturedFocus   ) ) cap->cb( CNOVRF_UPFOCUS, cap, props, i );
					if( ( cap = props->capturedPassive ) ) cap->cb( CNOVRF_UPNOFOCUS, cap, props, i );
					OGUnlockMutex( FOCUS.mutFocus );
				}
			}
		}

		//All updates should be based off of "tip"
		InputPoseActionData_t * p = &FOCUS.poseData[ctrl];
		int ret = cnovrstate->oInput->GetPoseActionDataForNextFrame( FOCUS.actionhandles[ctrl][CTRLA_TIP], 
			ETrackingUniverseOrigin_TrackingUniverseStanding, p, sizeof( *p ), k_ulInvalidInputValueHandle ); 
		if( !ret && p->bActive && p->pose.bPoseIsValid )
		{
			CNOVRPoseFromHMDMatrix( &FOCUS.poseTip[ctrl], &p->pose.mDeviceToAbsoluteTracking );
			memcpy( &props->poseTip, &FOCUS.poseTip[ctrl], sizeof( cnovr_pose ) );

			FOCUS.capPassiveTemp = props->capturedPassive;

			OGLockMutex( FOCUS.mutFocus );
			FOCUS.current_devid = ctrl+1;
			//props->NewCapturedFocus = 0; //Do not uncomment.  This would break captured focus.
			props->NewCapturedPassive = 0;
			props->NewPassiveRealDistance = 1000;
			CNOVRListCall( cnovrLCollide, props, 0 );
			props->capturedPassive = props->NewCapturedPassive;
			props->capturedPassiveDistance = props->NewPassiveRealDistance;
			FOCUS.current_devid = -1;
			OGUnlockMutex( FOCUS.mutFocus );

			OGLockMutex( FOCUS.mutFocus );
			if( FOCUS.capPassiveTemp != props->capturedPassive )
			{
				if( FOCUS.capPassiveTemp ) FOCUS.capPassiveTemp->cb( CNOVRF_OUT, FOCUS.capPassiveTemp, props, i );
				if( props->capturedPassive ) props->capturedPassive->cb( CNOVRF_IN, props->capturedPassive, props, i );
			}
			if( ( cap = props->capturedFocus   ) ) cap->cb( CNOVRF_DRAG, cap, props, i );
			if( ( cap = props->capturedPassive ) ) cap->cb( CNOVRF_MOTION, cap, props, i );
			OGUnlockMutex( FOCUS.mutFocus );
		}

		//Render model updates, etc. can be based off of "hand"
		ret = cnovrstate->oInput->GetPoseActionDataForNextFrame( FOCUS.actionhandles[ctrl][CTRLA_HAND], 
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
	}
}

void InternalCNOVRFocusShutdown()
{
	OGDeleteMutex( FOCUS.mutFocus );
}



void InternalCNOVRFocusSetup()
{
	int ctrl, i;

	FOCUS.mutFocus = OGCreateMutex();

	for( i = 0; i < 3; i++ )
	{
		cnovrfocus_properties * p = &FOCUS.focusProps[i];
		memset( p, 0, sizeof( *p ) );
		p->devid = i;
		pose_make_identity( &p->poseTip );
	}

	for( ctrl = 0; ctrl < 2; ctrl++ )
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

			for( ctrl = 0; ctrl < 2; ctrl++ )
			{
				const char * hand = ctrl?"right":"left";
				sprintf( stmp, "/user/hand/%s", hand );
				if( ( ret = cnovrstate->oInput->GetInputSourceHandle( stmp, &FOCUS.handsource[ctrl] ) ) < 0 ) printf( "Error %d\n", ret );

				static const char * eventnames[] = {
					"/actions/m/in/trigclick",
					"/actions/m/in/buttona",
					"/actions/m/in/buttonb",
					"/actions/m/in/graspclick",
					"/actions/m/in/trig",
					"/actions/m/in/hand",
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

	FOCUS.shdRenderModel = CNOVRShaderCreate( "rendermodel" );
	CNOVRListAdd( cnovrLRender, &FOCUS, FocusSystemRender );
}


//This is still kind of awkward.  Probably could use some fixing.
//Also, considerin switching it up so parts of this function live as a #define for speed.
void CNOVRFocusRespond( int devid, cnovrfocus_capture * ce, float realdistance, int attempt_focus )
{
	int cdevid = FOCUS.current_devid;
	cnovrfocus_properties * fp = FOCUS.focusProps + devid;
	if( realdistance < fp->NewPassiveRealDistance )
	{
		fp->NewCapturedPassive = ce;
		if( attempt_focus && !fp->capturedFocus )
		{
			fp->capturedFocus = ce;
		}
		fp->NewPassiveRealDistance = realdistance;
	}

}


void CNOVRFocusRemoveTag( void * tag )
{
	OGLockMutex( FOCUS.mutFocus );
	if( FOCUS.capPassiveTemp && FOCUS.capPassiveTemp->tag == tag ) FOCUS.capPassiveTemp = 0;
	int i = 0;
	for( i = 0; i < 3; i++ )
	{
		cnovrfocus_properties * p = &FOCUS.focusProps[i];
		cnovrfocus_capture ** ct[3] = { 
			&p->capturedFocus,
			&p->capturedPassive,
			&p->NewCapturedPassive };
		int j;
		for( j = 0; j < 3; j++ )
		{
			cnovrfocus_capture * c = *ct[j];
			if( c && c->tag == tag ) *ct[j] = 0;
		}
	}
	OGUnlockMutex( FOCUS.mutFocus );
}


