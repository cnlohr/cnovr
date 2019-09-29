#undef OSG_NOSTATIC
#undef OSG_PREFIX
#undef OSG_TERM_THREAD_CODE

#define OSG_PREFIX

//This code is executed any time a thread is closed.

void InternalFileSearchCloseThread();
void InternalThreadMallocClose();

//XXX TODO: Inform the TCC engine of the thread closure maybe? 

#define OSG_TERM_THREAD_CODE \
	InternalFileSearchCloseThread();\
	InternalThreadMallocClose();

#include <os_generic.h>

