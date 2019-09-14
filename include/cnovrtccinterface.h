#ifndef _CNOVRTCCINTERFACE_H
#define _CNOVRTCCINTERFACE_H

#include "../cntools/tccengine/tcccrash.h"
#include "cnovrtcc.h"
#include <os_generic.h>

extern og_tls_t tcctlstag;

//////////////////////////////////////////////////////////////////////////////
// Crash-safe mutex accessing

void OGTSLockMutex( og_mutex_t m );
void OGTSUnlockMutex( og_mutex_t m );
void OGResetSafeMutices();
void OGUnlockSafeMutices();

//////////////////////////////////////////////////////////////////////////////

#define TCCInvocation( v, code ) \
{ \
	OGSetTLS( tcctlstag, v ); \
	if( !v || tcccrash_checkpoint() == 0 ) \
	{ \
		OGResetSafeMutices(); \
		code; \
	} \
	else \
	{ \
		/* We're recovering from a crash */ \
		OGUnlockSafeMutices(); \
	} \
	OGSetTLS( tcctlstag, 0 ); \
	tcccrash_nullifycheckpoint(); \
} \

#define TCCGetTag() ((TCCInstance*)OGGetTLS( tcctlstag ))



//User apps should not include this file.

void InternalShutdownTCC( TCCInstance * tcc );
void InternalPopulateTCC( TCCInstance * tcc );

#endif

