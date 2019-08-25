#ifndef _CNOVR_INDEXED_LIST
#define _CNOVR_INDEXED_LIST

#include <cnhash.h>

//In general, this data structure is intended more for internal use rather than by clients.
//There is no locking in here.  All locking must be controlled by the user of this data structure.
//
//Consider the two applications
// * Jobs engine.  You'll pop off the front.
// * File change notifier.  You'll seek through your list.

typedef struct CNOVRIndexedListByTag_t
{
	struct CNOVRIndexedListByTag_t * next;
	struct CNOVRIndexedListByTag_t * prev;
	void * byitem;
	void * thisopaque;
	void * tag;
} CNOVRIndexedListByTag;

typedef struct CNOVRIndexedList_t
{
	cnhashtable * ht_by_tag;
	cnhash_delete_function df;
} CNOVRIndexedList;

//Just FYI "deletes" from cnhash_delete_function pass in your item as the value.  "key" is not to be trusted in the callback.

CNOVRIndexedList * CNOVRIndexedListCreate( cnhash_delete_function df );

//These both call the destructor.
void CNOVRIndexedListDeleteItemHandle( CNOVRIndexedList * list, CNOVRIndexedListByItem * item_handle );
void CNOVRIndexedListDeleteTag( CNOVRIndexedList * list, void * tag );
void CNOVRIndexedListInsert( CNOVRIndexedList * list, void * tag, void * item, void * thisopaque );

#endif

