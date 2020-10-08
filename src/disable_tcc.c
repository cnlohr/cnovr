#include <stdlib.h>
#include <os_generic.h>
#include <cnovr.h>
#include "../cntools/tccengine/tcccrash.h"

og_tls_t tcctlstag;

void OGTSLockMutex( og_mutex_t m ) { OGLockMutex( m ); }
void OGTSUnlockMutex( og_mutex_t m ) { OGUnlockMutex( m ); }
void OGResetSafeMutices() { }
void OGUnlockSafeMutices() { }
void internal_tcc_crash_free( void * v ) { free( v ); }
void * internal_tcc_crash_malloc( int s ) { return malloc( s );  }
void InternalSetupTCCInterface() { OGSetTLS( tcctlstag, 0 ); }
void InternalBreakdownRestOfTCCInterface() { }
void CNOVRStopTCCSystem() { }
void CNOVRTCCLog( void * data, const char * tolog )
{
}
