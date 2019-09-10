#include "cnovropenvr.h"
#include <stdio.h>
#include <stdlib.h>

void * CNOVRGetOpenVRFunctionTable( const char * interface )
{
	EVRInitError e;
	char fnTableName[128];
	int result1 = snprintf( fnTableName, 128, "FnTable:%s", interface );
	void * ret = (void *)VR_GetGenericInterface(fnTableName, &e );
	ovrprintf( "Getting System FnTable: %s = %p (%d)", fnTableName, ret, e );
	if( !ret )
	{
		exit( 1 );
	}
	return ret;
}

void CNOVRPoseFromHMDMatrix( cnovr_pose * pose, struct HmdMatrix34_t * matrix )
{
	matrix44_to_pose( pose, &matrix->m[0][0] );
}

