#ifndef _CNOVRTCCINTERFACE_H
#define _CNOVRTCCINTERFACE_H

#include "../cntools/tccengine/tcccrash.h"
#include "cnovrtcc.h"
#include <os_generic.h>

extern og_tls_t tcctlstag;

#define TCCInvocation( v, code ) { OGSetTLS( tcctlstag, v ); if( tcccrash_checkpoint() == 0 ) { code; } OGSetTLS( tcctlstag, 0 ); }
#define TCCGetTag() ((TCCInstance*)OGGetTLS( tcctlstag ))



//User apps should not include this file.

void InternalShutdownTCC( TCCInstance * tcc );
void InternalPopulateTCC( TCCInstance * tcc );

#endif

