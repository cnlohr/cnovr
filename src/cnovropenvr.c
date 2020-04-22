#include "cnovropenvr.h"
#include "cnovrutil.h"
#include <stdio.h>
#include <stdlib.h>

void * CNOVRGetOpenVRFunctionTable( const char * interfacename )
{
	EVRInitError e;
	char fnTableName[128];
	int result1 = snprintf( fnTableName, 128, "FnTable:%s", interfacename );
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

static og_tls_t tracked_device_buffer;
typedef struct tdbuffer_t { int len; char data[1]; } tdbuffer;
char * CNOVRGetTrackedDeviceString( TrackedDeviceIndex_t unDevice, TrackedDeviceProperty prop )
{
	if( !cnovrstate->oSystem ) return "";
	if( !tracked_device_buffer ) tracked_device_buffer = OGCreateTLS();
	tdbuffer * t = OGGetTLS( tracked_device_buffer );
	if( !t ) {
		OGSetTLS( tracked_device_buffer, ( t = CNOVRThreadMalloc( sizeof( tdbuffer ) + 64 ) ) );
		t->len = 64;
	}
 
	TrackedPropertyError e;
	uint32_t unRequiredBufferLen = cnovrstate->oSystem->GetStringTrackedDeviceProperty( unDevice, prop, NULL, 0, &e );

	if( e && e != ETrackedPropertyError_TrackedProp_BufferTooSmall ) goto err;
	if( unRequiredBufferLen == 0 )
	{
		t->data[0] = 0;
		return t->data;
	}
	if( unRequiredBufferLen > t->len )
	{
		t->len = unRequiredBufferLen + 2;
		t = CNOVRThreadRealloc( t, t->len );
		OGSetTLS( tracked_device_buffer, t );
	}
	cnovrstate->oSystem->GetStringTrackedDeviceProperty( unDevice, prop, t->data, unRequiredBufferLen, &e );
	if( e ) goto err;
	return t->data;
err:
	ovrprintf( "Error getting tracked device string property %d:%d:%d\n", unDevice, prop, e );
	t->data[0] = 0;
	return t->data; 
}

// Gets an int32 property of the given device from the OpenVR system.
// unDevice: The device to get data about
// property: The data to get about the device. See ETrackedDeviceProperty in the OpenVR API for the list of options.
int32_t CNOVRGetTrackedDeviceInt32(TrackedDeviceIndex_t unDevice, TrackedDeviceProperty property)
{
	int32_t Output = 0;

	if( !cnovrstate->oSystem ) return 0;
 
	TrackedPropertyError RetrievalError;
	Output = cnovrstate->oSystem->GetInt32TrackedDeviceProperty(unDevice, property, &RetrievalError);

	printf("Getting property %i of device %i returned a value of %i with error %i.", property, unDevice, Output, RetrievalError);

	if(RetrievalError) { ovrprintf("Error getting tracked device int32 property %d:%d:%d\n", unDevice, property, RetrievalError); }
	return Output;
}


