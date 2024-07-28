#ifndef _CNOVROPENVR_UTIL_H
#define _CNOVROPENVR_UTIL_H

#include <cnovrmath.h>
#include <cnovr.h>

#undef EXTERN_C
#include <openvr_capi.h>

#if defined( WINDOWS ) || defined(WIN32) || defined(WIN64)
#undef S_API
#define S_API __declspec(dllimport)
#endif

enum HandSkeletonBone
{
	eBone_Root = 0,
	eBone_Wrist,
	eBone_Thumb0,
	eBone_Thumb1,
	eBone_Thumb2,
	eBone_Thumb3,
	eBone_IndexFinger0,
	eBone_IndexFinger1,
	eBone_IndexFinger2,
	eBone_IndexFinger3,
	eBone_IndexFinger4,
	eBone_MiddleFinger0,
	eBone_MiddleFinger1,
	eBone_MiddleFinger2,
	eBone_MiddleFinger3,
	eBone_MiddleFinger4,
	eBone_RingFinger0,
	eBone_RingFinger1,
	eBone_RingFinger2,
	eBone_RingFinger3,
	eBone_RingFinger4,
	eBone_PinkyFinger0,
	eBone_PinkyFinger1,
	eBone_PinkyFinger2,
	eBone_PinkyFinger3,
	eBone_PinkyFinger4,
	eBone_Aux_Thumb,
	eBone_Aux_IndexFinger,
	eBone_Aux_MiddleFinger,
	eBone_Aux_RingFinger,
	eBone_Aux_PinkyFinger,
	eBone_Count
};

S_API int VR_InitInternal( EVRInitError *peError, EVRApplicationType eType );
S_API const char * VR_GetVRInitErrorAsEnglishDescription( EVRInitError error );
S_API void VR_ShutdownInternal();
S_API bool VR_IsHmdPresent();
S_API bool VR_IsRuntimeInstalled();
S_API intptr_t VR_GetGenericInterface(const char *pchInterfaceVersion, EVRInitError *peError);
S_API bool VR_IsInterfaceVersionValid(const char *pchInterfaceVersion);



void * CNOVRGetOpenVRFunctionTable( const char * interfacename );
void CNOVRPoseFromHMDMatrix( cnovr_pose * pose, struct HmdMatrix34_t * matrix ); 
char * CNOVRGetTrackedDeviceString( TrackedDeviceIndex_t unDevice, TrackedDeviceProperty prop );
int32_t CNOVRGetTrackedDeviceInt32(TrackedDeviceIndex_t unDevice, TrackedDeviceProperty property);
int32_t CNOVRGetControllerHandFromDeviceID(TrackedDeviceIndex_t unDevice);

#endif

