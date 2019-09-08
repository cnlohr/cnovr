#undef OSG_NOSTATIC
#undef OSG_PREFIX
#undef OSG_TERM_THREAD_CODE

#define OSG_PREFIX

//This code is executed any time a thread is closed.

void Internalcasprintffree();
void InternalFileSearchCloseThread();

//XXX TODO: Inform the TCC engine of the thread closure maybe? 

#define OSG_TERM_THREAD_CODE \
	InternalFileSearchCloseThread();\
	Internalcasprintffree();

#include <os_generic.h>

