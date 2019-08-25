#include <cnovrindexedlist.h>

static uint32_t CNILhash( void * key, void * opaque ) { return (key - (void*)0); }
static int      CNILcomp( void * key_a, void * key_b, void * opaque ) { return key_a != key_b; }


CNOVRIndexedList * CNOVRIndexedListCreate( cnhash_delete_function df )
{
	CNOVRIndexedList * ret = malloc( sizeof( CNOVRIndexedList ) );
	memset( ret, 0, sizeof( CNOVRIndexedList ) );
	ret->ht_by_tag = CNHashGenerate( 0, ret, CNILhash, CNILcomp, 0 );
	ret->df = df;
	return ret;
}

//These both call the destructor.
void CNOVRIndexedListDeleteItemHandle( CNOVRIndexedList * list, CNOVRIndexedListByItem * item_handle )
{
	if( !item_handle->prev )
	{
		if( !item_handle->next )
		{
			//Last item, delete element.
			CNHashDelete( list->ht_by_tag, item_handle->tag ); 
		}
		else
		{
			//Tricky: We're the first in the list.  We need to update the index.
			cnhashelement * e = CNHashIndex( list->ht_by_tag, item_handle->tag ); //If get fails, insert
			e->data = item_handle->next;
		}
	}
	else
	{
		item_handle->prev->next = item_handle->next;
	}
	if( item_handle->next )
	{
		item_handle->next->prev = item_handle->prev;
	}

	list->df( 0, item_handle->byitem, item_handle->thisopaque );

	free( item_handle );
}

void CNOVRIndexedListDeleteTag( CNOVRIndexedList * list, void * tag )
{
	CNOVRIndexedListByTag * e = CNHashGetValue( list->ht_by_tag, tag );
	while( e )
	{
		list->df( 0, e->byitem, e->thisopaque );
		CNOVRIndexedListByTag * next = e->next;
		free( e );
		e = next;
	}
	CNHashDelete( list->ht_by_tag, tag );
}

void CNOVRIndexedListInsert( CNOVRIndexedList * list, void * tag, void * item, void * thisopaque )
{
	CNOVRIndexedListByTag * newt = malloc( sizeof( CNOVRIndexedListByTag ) );
	newt->next = 0;
	newt->prev = 0;
	newt->byitem = item;
	newt->thisopaque = thisopaque;
	newt->tag = tag;
	cnhashelement * e = CNHashIndex( list->ht_by_tag, tag );
	newt->next = e->data;
	e->data = newt;
	if( newt->next )
	{
		newt->next->prev = newt;
	}
}







