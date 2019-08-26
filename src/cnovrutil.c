#include <cnovrutil.h>
#include <stdlib.h>
#include <cnhash.h>
#include <os_generic.h>
#include <string.h>
#include <stdio.h>
#include <cnovrindexedlist.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>


char * FileToString( const char * fname, int * length )
{
	FILE * f = fopen( fname, "rb" );
	if( !f ) return 0;
	fseek( f, 0, SEEK_END );
	*length = ftell( f );
	fseek( f, 0, SEEK_SET );
	char * ret = malloc( *length + 1 );
	int r = fread( ret, *length, 1, f );
	fclose( f );
	ret[*length] = 0;
	if( r != 1 )
	{
		free( ret );
		return 0;
	}
	return ret;
}

char ** SplitStrings( const char * line, char * split, char * white, int merge_fields )
{
	if( !line || strlen( line ) == 0 )
	{
		char ** ret = malloc( sizeof( char * )  );
		*ret = 0;
		return ret;
	}

	int elements = 1;
	char ** ret = malloc( elements * sizeof( char * )  );
	int * lengths = malloc( elements * sizeof( int ) ); 
	int i = 0;
	char c;
	int did_hit_not_white = 0;
	int thislength = 0;
	int thislengthconfirm = 0;
	int needed_bytes = 1;
	const char * lstart = line;
	do
	{
		int is_split = 0;
		int is_white = 0;
		char k;
		c = *(line);
		for( i = 0; (k = split[i]); i++ )
			if( c == k ) is_split = 1;
		for( i = 0; (k = white[i]); i++ )
			if( c == k ) is_white = 1;

		if( c == 0 || ( ( is_split ) && ( !merge_fields || did_hit_not_white ) ) )
		{
			//Mark off new point.
			lengths[elements-1] = thislengthconfirm + 1; //XXX BUGGY ... Or is bad it?  I can't tell what's wrong.  the "buggy" note was from a previous coding session.
			ret[elements-1] = (char*)lstart + 0; //XXX BUGGY //I promise I won't change the value.
			needed_bytes += thislengthconfirm + 1;
			elements++;
			ret = realloc( ret, elements * sizeof( char * )  );
			lengths = realloc( lengths, elements * sizeof( int ) );
			lstart = line;
			thislength = 0;
			thislengthconfirm = 0;
			did_hit_not_white = 0;
			line++;
			continue;
		}

		if( !is_white && ( !(merge_fields && is_split) ) )
		{
			if( !did_hit_not_white )
			{
				lstart = line;
				thislength = 0;
				did_hit_not_white = 1;
			}
			thislengthconfirm = thislength;
		}

		if( is_white )
		{
			if( did_hit_not_white ) 
				is_white = 0;
		}

		if( did_hit_not_white )
		{
			thislength++;
		}
		line++;
	} while ( c );

	//Ok, now we have lengths, ret, and elements.
	ret = realloc( ret, sizeof( char * ) * elements  + needed_bytes );
	char * retend = ((char*)ret) + (sizeof( char * ) * elements);
	for( i = 0; i < elements; i++ )
	{
		int len = lengths[i];
		memcpy( retend, ret[i], len );
		retend[len] = 0;
		ret[i] = (i == elements-1)?0:retend;
		retend += len + 1;
	}
	free( lengths );
	return ret;
}

int StringCompareEndingCase( const char * thing_to_search, const char * check_extension )
{
	if( !thing_to_search || !check_extension ) return -1;
	int tsclen = strlen( thing_to_search );
	return strcasecmp( thing_to_search + tsclen - strlen( check_extension ), check_extension );
}

///////////////////////////////////////////////////////////////////////////////

static og_thread_t   thdFileTimeCacher;
static volatile int  intStopFileTimeCacher;
static og_mutex_t    mutFileTimeCacher;
static cnhashtable * htFileTimeCacher;
static og_sema_t     semPendinger;
struct filetimetagged_t;

typedef struct filetimetagged_t
{
	void * tag;
	void * opaquev;
	cnovr_cb_fn * fn;
	struct filetimetagged_t * next;
	struct filetimetagged_t * prev;
	CNOVRIndexedListByTag * correspondance;
} filetimetagged;

typedef struct filetimedata_t
{
	double time;
	filetimetagged * front;
} filetimedata;

static CNOVRIndexedList * ftindexlist;
static filetimetagged ftstaged; //Current callback, used to make sure we don't delete something ongoing.

void * thdfiletimechecker( void * v )
{
	int i;
	while( !intStopFileTimeCacher )
	{
		//Tricky: This is safe because array_size only increases.
		OGLockMutex( mutFileTimeCacher );
		for( i = 0; i < htFileTimeCacher->array_size; i++ )
		{
			cnhashelement * e = htFileTimeCacher->elements + i;
			if( e->data )
			{
				double ft = OGGetFileTime( e->key );
				filetimedata * k = ((filetimedata*)e->data);
				if( k->time != ft )
				{
					k->time = ft;
					filetimetagged * l = k->front;
					filetimetagged * staged = &ftstaged;
					while( l )
					{
						staged->tag = l->tag;
						staged->opaquev = l->opaquev;
						staged->fn = l->fn;
						OGUnlockMutex( mutFileTimeCacher );
						l->fn( l->tag, l->opaquev );
						OGLockMutex( mutFileTimeCacher );
						staged->tag = 0;
						staged->opaquev = 0;
						staged->fn = 0;
					}
				}
				while( OGGetSema( semPendinger ) == 0 ) OGUnlockSema( semPendinger ); 
				OGUnlockMutex( mutFileTimeCacher );
				OGUSleep( 1000 );
				OGLockMutex( mutFileTimeCacher );
			}
		}
		OGUnlockMutex( mutFileTimeCacher );
		OGUSleep( 1000 );
	}
	return 0;
}

double FileTimeCached( const char * fname )
{
	OGLockMutex( mutFileTimeCacher );
	filetimedata * ret = (filetimedata*)CNHashGetValue( htFileTimeCacher, (void*)fname );
	if( ret )
	{
		double r = ret->time;
		OGUnlockMutex( mutFileTimeCacher );
		return r;
	}
	filetimedata * in = malloc( sizeof( filetimedata ) );
	in->front = 0;
	in->time = 0;
	CNHashInsert( htFileTimeCacher, strdup( fname ), in );
	OGUnlockMutex( mutFileTimeCacher );
	return 0;
}

static void ftremovefn( void * tag, void * item, void * opaque )
{
	filetimetagged * t = (filetimetagged*)item;
	filetimedata * d = (filetimedata*)opaque;

	if( d->front == t )
	{
		d->front = t->next;
	}
	if( t->prev )
	{
		t->prev->next = t->next;
	}
	if( t->next )
	{
		t->next->prev = t->prev;
	}
	free( t );

}

void CNOVRInternalStartCacheSystem()
{
	mutFileTimeCacher = OGCreateMutex();
	intStopFileTimeCacher = 0;
	htFileTimeCacher = CNHashGenerate( 0, 0, CNHASH_STRINGS );
	semPendinger = OGCreateSema(); 
	ftindexlist = CNOVRIndexedListCreate( ftremovefn );
	thdFileTimeCacher = OGCreateThread( thdfiletimechecker, 0 );
}

void CNOVRInternalStopCacheSystem()
{
	intStopFileTimeCacher = 1;
	OGJoinThread( thdFileTimeCacher ); 
	OGDeleteMutex( mutFileTimeCacher );
	OGDeleteSema( semPendinger );
	CNOVRIndexedListDestroy( ftindexlist );
}

void FileTimeAddWatch( const char * fname, cnovr_cb_fn fn, void * tag, void * opaquev )
{
	OGLockMutex( mutFileTimeCacher );
	filetimedata * ftd = (filetimedata*)CNHashGetValue( htFileTimeCacher, (void*)fname );
	if( !ftd )
	{
		ftd = malloc( sizeof( filetimedata ) );
		ftd->front = 0;
		ftd->time = 0;
		CNHashInsert( htFileTimeCacher, strdup( fname ), ftd );
	}
	filetimetagged * t = malloc( sizeof( filetimetagged ) );
	t->tag = tag;
	t->opaquev = opaquev;
	t->fn = fn;
	t->prev = 0;
	t->next = ftd->front;
	if( t->next )
	{
		t->next->prev = t;
	}
	ftd->front = t;
	t->correspondance = CNOVRIndexedListInsert( ftindexlist, tag, t, ftd );

	OGUnlockMutex( mutFileTimeCacher );
}


void FileTimeRemoveWatch( const char * fname, cnovr_cb_fn fn, void * tag, void * opaquev )
{
	OGLockMutex( mutFileTimeCacher );
	filetimedata * ret = (filetimedata*)CNHashGetValue( htFileTimeCacher, (void*)fname );
	filetimetagged ** t = &ret->front;
	while( *t )
	{
		if( (*t)->fn == fn && (*t)->tag == tag && (*t)->opaquev == opaquev ) break;
		t = &((*t)->next);
	}
	if( *t )
	{
		CNOVRIndexedListDeleteItemHandle( ftindexlist, (*t)->correspondance );
	}
	OGUnlockMutex( mutFileTimeCacher );
}

void FileTimeRemoveTagged( void * tag, int wait_on_pending )
{
	OGLockMutex( mutFileTimeCacher );
	CNOVRIndexedListDeleteTag( ftindexlist, tag );
	OGUnlockMutex( mutFileTimeCacher );

	while( wait_on_pending && ftstaged.tag == tag )
	{
		OGLockSema( semPendinger );
	}
}


///////////////////////////////////////////////////////////////////////////////

struct CNOVRJobQueue_t;

// Warning: You can only have one consumer per queue with the way locking is currently set up.
typedef struct CNOVRJobElement_t
{
	cnovr_cb_fn * fn;
	void * tag;
	void * opaquev;
	struct CNOVRJobElement_t * next;
	struct CNOVRJobElement_t * prev;
	CNOVRIndexedListByTag * correspondance;
} CNOVRJobElement;

typedef struct CNOVRJobQueue_t
{
	CNOVRJobElement * front;
	CNOVRJobElement * back;
	CNOVRJobElement staged;
	cnhashtable * hash;
	og_mutex_t mut;
	og_sema_t  sem;
	og_sema_t  pendingsem;
} CNOVRJobQueue;

static uint32_t JQhash( void * key, void * opaque ) { CNOVRJobElement * he = (CNOVRJobElement*)key; return ( ((uint32_t)(he->fn-((cnovr_cb_fn*)0)) + (uint32_t)(he->tag-((void*)0)) + (uint32_t)(he->opaquev - (void*)0) )) | 1; }
static int      JQcomp( void * key_a, void * key_b, void * opaque )
{
	if( !key_a || !key_b ) return 1;
	CNOVRJobElement * he = (CNOVRJobElement*)key_a;
	CNOVRJobElement * hf = (CNOVRJobElement*)key_b;
	if( he->fn == hf->fn && 
		he->tag == hf->tag &&
		he->opaquev == hf->opaquev)
		return 0;
	else
		return 1;
} 

static CNOVRJobQueue CNOVRJEQ[cnovrQMAX];
static CNOVRIndexedList * JQELIST;

static void IndexedDestructor( void * tag, void * item, void * opaque )
{
	CNOVRJobElement * je = (CNOVRJobElement*)item;
	CNOVRJobQueue * jq = (CNOVRJobQueue*)opaque;

	//This function is not threadsafe.
	if( jq->front == je )
	{
		jq->front = je->next;
		if( jq->front )
		{
			jq->front->prev = 0;
		}
	}
	if( jq->back == je )
	{
		jq->back = je->prev;
		if( jq->back )
		{
			jq->back->next = 0;
		}
	}
	if( je->prev ) je->prev->next = je->next;
	if( je->next ) je->next->prev = je->prev;

	CNHashDelete( jq->hash, je );
	free( je );	
}

static void BackendDeleteJob( CNOVRJobQueue * jq, CNOVRJobElement * je )
{
	CNOVRIndexedListDeleteItemHandle( JQELIST, je->correspondance );
}

void DEBUGDumpQueue( cnovrQueueType qt )
{
	CNOVRJobQueue * q = &CNOVRJEQ[qt];
	printf( "Q: FRONT: %p   BACK: %p\n", q->front, q->back );
	CNOVRJobElement * e = q->front;
	while( e )
	{
		printf( "  <<%16p %16p(%p) %16p>>\n",  e->prev, e, e->opaquev, e->next );
		e = e->next;
	}
}


static void * CNOVRJobProcessor( void * v )
{
	CNOVRJobQueue * jq = (CNOVRJobQueue*)v;
	while(1)
	{
		OGLockSema( jq->sem );
		OGLockMutex( jq->mut );
		CNOVRJobElement * front = jq->front;
		CNOVRJobElement * staged = &(jq->staged);
		if( front )
		{
			memcpy( staged, front, sizeof( jq->staged ) );
			BackendDeleteJob( jq, front );
		}
		OGUnlockMutex( jq->mut );

		if( front )
		{
			staged->fn( staged->tag, staged->opaquev );

			//If you were to cancel the job, spinlock until e->staged == 0.
			staged->tag = 0;
			staged->opaquev = 0;
			staged->fn = 0;

			//In case any close-outs were pending.
			OGLockMutex( jq->mut );
			while( OGGetSema( jq->pendingsem ) == 0 ) OGUnlockSema( jq->pendingsem ); 
			OGUnlockMutex( jq->mut );
		}
	}
	return 0;
}

void CNOVRJobInit()
{
	int i;

	JQELIST = CNOVRIndexedListCreate( IndexedDestructor );

	for( i = 0; i < cnovrQMAX; i++ )
	{
		CNOVRJEQ[i].mut = OGCreateMutex();
		CNOVRJEQ[i].sem = OGCreateSema();
		CNOVRJEQ[i].pendingsem = OGCreateSema();
		CNOVRJEQ[i].front = 0;
		CNOVRJEQ[i].back = 0;
		CNOVRJEQ[i].hash = CNHashGenerate( 0, 0, JQhash, JQcomp, 0 );
		memset( &CNOVRJEQ[i].staged, 0, sizeof( CNOVRJEQ[i].staged ) );
	}

	OGCreateThread( CNOVRJobProcessor, &CNOVRJEQ[cnovrQLow] );
	OGCreateThread( CNOVRJobProcessor, &CNOVRJEQ[cnovrQAsync] );
}

int CNOVRJobProcessQueueElement( cnovrQueueType q )
{
	CNOVRJobQueue * jq = &CNOVRJEQ[q];
	OGLockMutex( jq->mut );
	CNOVRJobElement * front = jq->front;
	if( front )
	{
		CNOVRJobElement * staged = &(jq->staged);
		memcpy( staged, front, sizeof( jq->staged ) );
		BackendDeleteJob( jq, front );
		OGUnlockMutex( jq->mut );

		staged->fn( staged->tag, staged->opaquev );
		staged->tag = 0;
		staged->opaquev = 0;
		staged->fn = 0;

		//In case any close-outs were pending.
		//This is probably a place worth peeking if there's a problem found with this code
		//verify no race condition in your particular application/fitness
		OGLockMutex( jq->mut );
		while( OGGetSema( jq->pendingsem ) == 0 ) OGUnlockSema( jq->pendingsem ); 
		OGUnlockMutex( jq->mut );
		return 1;
	}

	OGUnlockMutex( jq->mut );
	return 0;
}

void CNOVRJobTack( cnovrQueueType q, cnovr_cb_fn fn, void * tag, void * opaquev, int insert_even_if_pending )
{
	CNOVRJobElement * newe = malloc( sizeof( CNOVRJobElement ) );
	newe->fn = fn;
	newe->tag = tag;
	newe->opaquev = opaquev;
	newe->next = 0;
	newe->prev = 0;

	CNOVRJobQueue * jq = &CNOVRJEQ[q];

	OGLockMutex( jq->mut );
	int is_pending = JQcomp( newe, &jq->staged, 0 ) == 0;

	//Look for duplicates
	if( ( is_pending && !insert_even_if_pending ) || CNHashInsert( jq->hash, newe, newe ) != 0 )
	{
		//Failed to insert.
		free( newe );
	}
	else
	{
		if( jq->back )
		{
			jq->back->next = newe;
			newe->prev = jq->back;
			jq->back = newe;
		}
		else
		{
			jq->back = jq->front = newe;
		}
		newe->correspondance = CNOVRIndexedListInsert( JQELIST, tag, newe, jq );

		OGUnlockSema( jq->sem );
	}
	OGUnlockMutex( jq->mut );
}

void CNOVRJobCancel( cnovrQueueType q, cnovr_cb_fn fn, void * tag, void * opaquev, int wait_on_pending )
{
	CNOVRJobElement compe;
	compe.fn = fn;
	compe.tag = tag;
	compe.opaquev = opaquev;

	CNOVRJobQueue * jq = &CNOVRJEQ[q];

	OGLockMutex( jq->mut );
	//Look for job tdelete
	CNOVRJobElement * dupat = (CNOVRJobElement*)CNHashGetValue( jq->hash, &compe );
	if( dupat )
	{
		BackendDeleteJob( jq, dupat );
	}
	OGUnlockMutex( jq->mut );

	//Make srue we don't have any remaining pends.
	while( wait_on_pending && (JQcomp( &compe, &jq->staged, 0 ) == 0) )
	{
		OGLockSema( jq->pendingsem );
	}
}

void CNOVRJobCancelAllTag( void * tag, int wait_on_pending )
{
	int list;
	for( list = 0; list < cnovrQMAX; list++ )
	{
		OGLockMutex( CNOVRJEQ[list].mut );
	}
	CNOVRIndexedListDeleteTag( JQELIST, tag );
	for( list = 0; list < cnovrQMAX; list++ )
	{
		OGUnlockMutex( CNOVRJEQ[list].mut );
	}

	for( list = 0; list < cnovrQMAX; list++ )
	{
		CNOVRJobQueue * jq = &CNOVRJEQ[list];
		while( wait_on_pending && jq->staged.tag == tag )
		{
			OGLockSema( jq->pendingsem );
		}
	}
}





