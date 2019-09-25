#include "cnovrtccinterface.h"

//Tricky: tcccrash will malloc() data inside threads. We want it to be automatically cleared.
#define TCCCRASH_THREAD_MALLOC  TCCmalloc
#define TCCCRASH_THREAD_FREE    TCCfree
 
#include "../cntools/tccengine/tcccrash.c"

