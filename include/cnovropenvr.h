#ifndef _CNOVROPENVR_UTIL_H
#define _CNOVROPENVR_UTIL_H

#include <openvr_capi.h>
#include <cnovrmath.h>
#include <cnovr.h>



S_API int VR_InitInternal( EVRInitError *peError, EVRApplicationType eType );
S_API const char * VR_GetVRInitErrorAsEnglishDescription( EVRInitError error );
S_API void VR_ShutdownInternal();
S_API bool VR_IsHmdPresent();
S_API bool VR_IsRuntimeInstalled();
S_API intptr_t VR_GetGenericInterface(const char *pchInterfaceVersion, EVRInitError *peError);
S_API bool VR_IsInterfaceVersionValid(const char *pchInterfaceVersion);



void * CNOVRGetOpenVRFunctionTable( const char * interface );
void CNOVRPoseFromHMDMatrix( cnovr_pose * pose, struct HmdMatrix34_t * matrix ); 

#endif

