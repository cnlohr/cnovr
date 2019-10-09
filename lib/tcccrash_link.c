#include "cnovrtccinterface.h"

//Tricky: tcccrash will malloc() data inside threads. We want it to be automatically cleared.
#define TCCCRASH_THREAD_MALLOC  internal_tcc_crash_malloc
#define TCCCRASH_THREAD_FREE    internal_tcc_crash_free

void * internal_tcc_crash_malloc( int size );
void internal_tcc_crash_free( void * data );

#include "../cntools/tccengine/tcccrash.c"

