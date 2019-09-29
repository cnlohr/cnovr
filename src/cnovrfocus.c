#include "cnovrfocus.h"
#include <cnovrutil.h>
#include <cnovr.h>
#include <stdio.h>
#include <string.h>
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
	cnovr_texture * texRenderModels[2];
	cnovr_pose      poseController[2];
	bool bShowController[2];
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
			//R contains the value. XXX TODO Pick up here.
		}

		InputPoseActionData_t * p = &FOCUS.poseData[ctrl];

		int ret = cnovrstate->oInput->GetPoseActionDataForNextFrame( FOCUS.actionhandles[ctrl][CTRLA_HAND], 
			ETrackingUniverseOrigin_TrackingUniverseStanding, p, sizeof( *p ), k_unTrackedDeviceIndexInvalid ); 
		if( ret || !p->bActive || !p->pose.bPoseIsValid )
		{
			printf( "%d %d %d  %08x\n", ret, p->bActive, p->pose.bPoseIsValid, FOCUS.actionhandles[ctrl][CTRLA_HAND] );
			FOCUS.bShowController[ctrl] = 0;
		}
		else
		{
			CNOVRPoseFromHMDMatrix( &FOCUS.poseController[ctrl], &p->pose.mDeviceToAbsoluteTracking );

			InputOriginInfo_t * o = &FOCUS.originInfo[ctrl];

			if ( cnovrstate->oInput->GetOriginTrackedDeviceInfo( p->activeOrigin, o, sizeof( *o ) ) == EVRInputError_VRInputError_None 
				&& o->trackedDeviceIndex != k_unTrackedDeviceIndexInvalid )
			{
				printf( "((((((((((((((((((((((  Focus Got\n" );
				if( FOCUS.rendermodelnames[ctrl] == 0 || FOCUS.bShowController[ctrl] == 0 )
				{
					char * rmname = CNOVRGetTrackedDeviceString( o->trackedDeviceIndex, ETrackedDeviceProperty_Prop_RenderModelName_String );
					printf( "(((((((((((((((( GOT RM NAME %s\n", rmname );
					if( rmname )
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
							cnovr_texture * t = FOCUS.texRenderModels[ctrl];
							if( !t ) t = FOCUS.texRenderModels[ctrl] = CNOVRTextureCreate( 1, 1, 4 );
							CNOVRTextureLoadFileAsync( t, rmname2 );
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
					"/actions/m/in/hand"
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

}

