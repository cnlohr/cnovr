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

#endif

