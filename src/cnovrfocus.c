#include "cnovrfocus.h"
#include <cnovrutil.h>
#include <cnovr.h>
#include <stdio.h>
#include <openvr_capi.h>

typedef struct internal_focus_system_t
{
	VRActionSetHandle_t inputactionset;
	VRInputValueHandle_t handsource[2];
	VRActionHandle_t actionhandles[2][CTRLA_MAX];
	InputOriginInfo_t originInfo[2];
	InputPoseActionData_t poseData[2];
	cnovr_shader * shdRenderModel;
	cnovr_model  * mdlRenderModels[2];
	cnovr_texture * texRenderModels[2];
} internal_focus_system;

internal_focus_system FOCUS;






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

		int i;
		for( i = 0 ; i <= CTRLA_GRASP; i++ )
		{
			int r;
			VRActionHandle_t h = FOCUS.actionhandles[ctrl][i];
			if( h == k_ulInvalidActionHandle ) continue;
			if( ( r = GetDigitalActionData( h ) ) < 0 ) { printf( "Err %d on %d\n", r, i ); continue; }
			//R contains the value.
			printf( "%d\n", r );
		}
	}
}

void InternalCNOVRFocusShutdown()
{
}



void InternalCNOVRFocusSetup()
{
	int ctrl, i;

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
					"/actions/m/in/pose"
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

				cnovrstate->oInput->GetPoseActionDataForNextFrame( &FOCUS.actionPose[k], 
					ETrackingUniverseOrigin_TrackingUniverseStanding, &FOCUS.poseData[k], sizeof( FOCUS.poseData[k] ) ); 
			
				GetOriginTrackedDeviceInfo( FOCUS.originInfo, 
			}


			printf( "FOCUS HAND SOURCES: %d %d\n", FOCUS.handsource[0], FOCUS.handsource[1] );
		}
	}

		if ( vr::VRInput()->GetPoseActionDataForNextFrame( m_rHand[eHand].m_actionPose, vr::TrackingUniverseStanding, &poseData, sizeof( poseData ), vr::k_ulInvalidInputValueHandle ) != vr::VRInputError_None
			|| !poseData.bActive || !poseData.pose.bPoseIsValid )
		{
			m_rHand[eHand].m_bShowController = false;
		}
		else
		{

			m_rHand[eHand].m_rmat4Pose = ConvertSteamVRMatrixToMatrix4( poseData.pose.mDeviceToAbsoluteTracking );

			vr::InputOriginInfo_t originInfo;
			if ( vr::VRInput()->GetOriginTrackedDeviceInfo( poseData.activeOrigin, &originInfo, sizeof( originInfo ) ) == vr::VRInputError_None 
				&& originInfo.trackedDeviceIndex != vr::k_unTrackedDeviceIndexInvalid )
			{
				std::string sRenderModelName = GetTrackedDeviceString( originInfo.trackedDeviceIndex, vr::Prop_RenderModelName_String );
				if ( sRenderModelName != m_rHand[eHand].m_sRenderModelName )
				{
					m_rHand[eHand].m_pRenderModel = FindOrLoadRenderModel( sRenderModelName.c_str() );
					m_rHand[eHand].m_sRenderModelName = sRenderModelName;
				}
			}
		}

	FOCUS.shdRenderModel = CNOVRShaderCreate( "rendermodel" );
	for( i = 0; i < 2; i++ )
	{
		char rmtext[256];
		
		cnovr_model * m = mdlRenderModels[i] = CNOVRModelCreate( 0, 0, GL_TRIANGLES );
		CNOVRModelLoadFromFileAsync( m, rendermodel );
		cnovr_texture * texRenderModels[2];
	}
}

