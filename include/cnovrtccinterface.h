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

//#define INVOCATEDEBUG(x) printf( "ICDBG %d\n", x ); 
#define INVOCATEDEBUG(x)

#define TCCInvocation( v, code ) \
{ \
	INVOCATEDEBUG(1); \
	OGSetTLS( tcctlstag, v ); \
	INVOCATEDEBUG(2); \
	if( !v || tcccrash_checkpoint() == 0 ) \
	{ \
		INVOCATEDEBUG(3); \
		OGResetSafeMutices(); \
		INVOCATEDEBUG(4); \
		code; \
		INVOCATEDEBUG(5); \
	} \
	else \
	{ \
		/* We're recovering from a crash */ \
		INVOCATEDEBUG(6); \
		OGUnlockSafeMutices(); \
		INVOCATEDEBUG(7); \
	} \
	INVOCATEDEBUG(8); \
	OGSetTLS( tcctlstag, 0 ); \
	INVOCATEDEBUG(9); \
	tcccrash_nullifycheckpoint(); \
	INVOCATEDEBUG(10); \
} \

#define TCCGetTag() ((TCCInstance*)OGGetTLS( tcctlstag ))



//User apps should not include this file.

void InternalShutdownTCC( TCCInstance * tcc );
void InternalPopulateTCC( TCCInstance * tcc );
void InternalInterfaceCreationDone( TCCInstance * tce );


//For a thread-safe malloc/free
void *TCCmalloc(size_t size);
void TCCfree(void *ptr);
void * TCCcalloc(size_t nmemb, size_t size);
void * TCCrealloc(void *ptr, size_t size);
char * TCCstrndup(const char * str, size_t size);
char * TCCstrdup(const char * str );

#endif

