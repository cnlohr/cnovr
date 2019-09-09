#ifndef _CNOVRTCCINTERFACE_H
#define _CNOVRTCCINTERFACE_H

#include "../cntools/tccengine/tcccrash.h"
#include <os_generic.h>

extern og_tls_t tcctlstag;

#define TCCEntry( v, code ) OGSetTLS( tcctlstag, v ); if( tcccrash_checkpoint() == 0 ) code
#define TCCGetTag() OGGetTLS( tcctlstag )

//User apps should not include this file.

struct TCCState;
typedef struct TCCState TCCState;

void InternalShutdownTCC( TCCState * tcc );
void InternalPopulateTCC( TCCState * tcc );

#endif

