#include "cnovrfocus.h"
#include <cnovrutil.h>
#include <cnovr.h>
#include <stdio.h>
#include <openvr_capi.h>

typedef struct internal_focus_system_t
{
	VRActionHandle_t actionhandles[2][CTRLA_MAX];
} internal_focus_system;

internal_focus_system FOCUS;






//XXX TODO: We may want to capture fUpdateTime from actionData.
int GetDigitalActionData( VRActionHandle_t h )
{
	if( !cnovrstate->oInput ) return 0;
	InputDigitalActionData_t actionData;
	if( !cnovrstate->oInput->GetDigitalActionData( h, &actionData, sizeof(actionData), k_ulInvalidInputValueHandle ) ) return 0;
	return actionData.bActive && actionData.bState;
}

float GetAnalogActionData( VRActionHandle_t h )
{
	if( !cnovrstate->oInput ) return 0;
	InputAnalogActionData_t actionData;
	if( !cnovrstate->oInput->GetAnalogActionData( h, &actionData, sizeof(actionData), k_ulInvalidInputValueHandle ) ) return 0;
	return actionData.bActive?actionData.x:0;
}

void InternalCNOVRFocusUpdate()
{
	int ctrl = 0;
	for( ; ctrl < 2; ctrl++ )
	{
		VRActionHandle_t h = FOCUS.actionhandles[ctrl][0];
		if( h == k_ulInvalidActionHandle ) continue;
		printf( "%d %d %d\n", ctrl, 0, GetDigitalActionData( h ) );
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
			cnovrstate->oInput->SetActionManifestPath( manifestpath );
			for( ctrl = 0; ctrl < 2; ctrl++ )
			{
				const char * hand = ctrl?"right":"left";

				static const char * eventnames[] = {
					"/actions/m/trigclick",
					"/actions/m/buttona",
					"/actions/m/buttonb",
					"/actions/m/graspclick",
					"/actions/m/trig",
					"/actions/m/pose"
				};

				for( i = 0; i < CTRLA_MAX; i++ )
				{
					char stmp[128];
					sprintf( stmp, "%s/%s", eventnames[i], hand );
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
}

