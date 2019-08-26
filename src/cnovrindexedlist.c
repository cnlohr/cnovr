#include <cnovrindexedlist.h>
#include <stdlib.h>
#include <string.h>

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

void CNOVRIndexedListDestroy( CNOVRIndexedList * list )
{
	int i;
	if( list->df )
	{
		for( i = 0; i < list->ht_by_tag->array_size; i++ )
		{
			cnhashelement * e = &list->ht_by_tag->elements[i];
			if( e->hashvalue )
			{
				CNOVRIndexedListByTag * d = (CNOVRIndexedListByTag *)e->data;
				list->df( 0, d->byitem, d->thisopaque );
			}
		}
	}
	CNHashDestroy( list->ht_by_tag );
}

//These both call the destructor.
void CNOVRIndexedListDeleteItemHandle( CNOVRIndexedList * list, CNOVRIndexedListByTag * bylisttag )
{
	if( !bylisttag->prev )
	{
		if( !bylisttag->next )
		{
			//Last item, delete element.
			CNHashDelete( list->ht_by_tag, bylisttag->tag ); 
		}
		else
		{
			//Tricky: We're the first in the list.  We need to update the index.
			cnhashelement * e = CNHashIndex( list->ht_by_tag, bylisttag->tag ); //If get fails, insert
			e->data = bylisttag->next;
		}
	}
	else
	{
		bylisttag->prev->next = bylisttag->next;
	}
	if( bylisttag->next )
	{
		bylisttag->next->prev = bylisttag->prev;
	}

	if( list->df ) list->df( 0, bylisttag->byitem, bylisttag->thisopaque );

	free( bylisttag );
}

void CNOVRIndexedListDeleteTag( CNOVRIndexedList * list, void * tag )
{
	CNOVRIndexedListByTag * e = CNHashGetValue( list->ht_by_tag, tag );
	while( e )
	{
		if( list->df ) list->df( e->tag, e->byitem, e->thisopaque );
		CNOVRIndexedListByTag * next = e->next;
		free( e );
		e = next;
	}
	CNHashDelete( list->ht_by_tag, tag );
}

CNOVRIndexedListByTag * CNOVRIndexedListInsert( CNOVRIndexedList * list, void * tag, void * item, void * thisopaque )
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
	return newt;
}


