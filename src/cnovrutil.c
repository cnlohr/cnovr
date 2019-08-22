#include <cnovrutil.h>
#include <stdlib.h>
#include <cnhash.h>
#include <os_generic.h>
#include <string.h>
#include <stdio.h>

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
			lengths[elements-1] = thislengthconfirm + 1; //XXX BUGGY
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


static og_thread_t   thdFileTimeCacher;
static volatile int  intStopFileTimeCacher;
static og_mutex_t    mutFileTimeCacher;
static cnhashtable * htFileTimeCacher;

struct filetimetagged_t;

typedef struct filetimetagged_t
{
	void * tag;
	uint8_t * flag;
	struct filetimetagged_t * next;
} filetimetagged;

typedef struct filetimedata_t
{
	double time;
	filetimetagged * tagged;
} filetimedata;

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
					filetimetagged * t = k->tagged;
					while( t )
					{
						*(t->flag) = 0xff;
						t = t->next;
					}
				}
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
	in->tagged = 0;
	in->time = 0;
	CNHashInsert( htFileTimeCacher, strdup( fname ), in );
	OGUnlockMutex( mutFileTimeCacher );
	return 0;
}

void CNOVRInternalStartCacheSystem()
{
	mutFileTimeCacher = OGCreateMutex();
	intStopFileTimeCacher = 0;
	htFileTimeCacher = CNHashGenerate( 0, 0, CNHASH_STRINGS );
	thdFileTimeCacher = OGCreateThread( thdfiletimechecker, 0 );
}

void CNOVRInternalStopCacheSystem()
{
	intStopFileTimeCacher = 1;
	OGJoinThread( thdFileTimeCacher ); 
	OGDeleteMutex( mutFileTimeCacher );
}

int StringCompareEndingCase( const char * thing_to_search, const char * check_extension )
{
	if( !thing_to_search || !check_extension ) return -1;
	int tsclen = strlen( thing_to_search );
	return strcasecmp( thing_to_search + tsclen - strlen( check_extension ), check_extension );
}

void FileTimeAddWatch( const char * fname, uint8_t * flag, void * tag )
{
	OGLockMutex( mutFileTimeCacher );
	filetimedata * ret = (filetimedata*)CNHashGetValue( htFileTimeCacher, (void*)fname );
	if( !ret )
	{
		ret = malloc( sizeof( filetimedata ) );
		ret->tagged = 0;
		ret->time = 0;
		CNHashInsert( htFileTimeCacher, strdup( fname ), ret );
	}
	filetimetagged * t = malloc( sizeof( filetimetagged ) );
	t->tag = tag;
	t->flag = flag;
	t->next = ret->tagged;
	ret->tagged = t;
	OGUnlockMutex( mutFileTimeCacher );
}

void FileTimeRemoveWatch( const char * fname, uint8_t * flag, void * tag )
{
	OGLockMutex( mutFileTimeCacher );
	filetimedata * ret = (filetimedata*)CNHashGetValue( htFileTimeCacher, (void*)fname );
	filetimetagged ** t = &ret->tagged;
	while( *t )
	{
		if( (*t)->flag == flag && (*t)->tag == tag )
		{
			*t = (*t)->next;
		}
		t = &(*t)->next;
	}
	OGUnlockMutex( mutFileTimeCacher );
}

void FileTimeRemoveTagged( void * tag )
{
	//TODO: Make another data structure which holds all tag mapping.
	OGLockMutex( mutFileTimeCacher );
	int i;
	for( i = 0; i < htFileTimeCacher->array_size; i++ )
	{
		cnhashelement * e = htFileTimeCacher->elements + i;
		if( e->data )
		{
			filetimedata * f = e->data;
			filetimetagged ** t = &f->tagged;
			while( *t )
			{
				if( (*t)->tag == tag )
				{
					*t = (*t)->next;
				}
				t = &(*t)->next;
			}
		}
	}

	OGUnlockMutex( mutFileTimeCacher );
}


///////////////////////////////////////////////////////////////////////////////
//
// TODO: We could significantly optimize this using various datastructures.
// TODO: Improve performance or change behavior of an in-progress queue execution.
// TODO: Consider changing behavior of removing with duplicates.
// XXX: Warning: You can only have one consumer per queue.
typedef struct CNOVRJobElement_t
{
	cnovr_cb_fn * fn;
	void * opaquev;
	int opaquei;
	struct CNOVRJobElement_t * next;
} CNOVRJobElement;

typedef struct CNOVRJobQueue_t
{
	CNOVRJobElement * e;
	CNOVRJobElement staged;
	og_mutex_t mut;
	og_sema_t  sem;
} CNOVRJobQueue;

static CNOVRJobQueue CNOVRJEQ[cnovrQMAX];

static void * CNOVRJobProcessor( void * v )
{
	CNOVRJobQueue * q = (CNOVRJobQueue*)v;
	while(1)
	{
		OGLockSema( q->sem );
		OGLockMutex( q->mut );
		CNOVRJobElement * k = q->e;
		if( k )
		{
			CNOVRJobElement * staged = &q->staged;
			staged->fn = k->fn;
			staged->opaquev = k->opaquev;
			staged->opaquei = k->opaquei;
			q->e = k->next;
		}
		OGUnlockMutex( q->mut );

		if( k )
		{
			k->fn( k->opaquev, k->opaquei );

			//If you were to cancel the job, spinlock until e->staged == 0.
			k->opaquev = 0;
			k->opaquei = 0;
			k->fn = 0;
		}
	}
	return 0;
}

void CNOVRJobInit()
{
	int i;
	for( i = 0; i < cnovrQMAX; i++ )
	{
		CNOVRJEQ[i].mut = OGCreateMutex();
		CNOVRJEQ[i].sem = OGCreateSema();
		CNOVRJEQ[i].e = 0;
		memset( &CNOVRJEQ[i].staged, 0, sizeof( CNOVRJEQ[i].staged ) );
	}

	OGCreateThread( CNOVRJobProcessor, &CNOVRJEQ[cnovrQLow] );
	OGCreateThread( CNOVRJobProcessor, &CNOVRJEQ[cnovrQAsync] );
}

int CNOVRProcessQueueElement( cnovrQueueType q )
{
	CNOVRJobQueue * jq = &CNOVRJEQ[q];
	OGLockMutex( jq->mut );
	if( jq->e )
	{
		CNOVRJobElement * staged = &(jq->staged);
		memcpy( staged, jq->e, sizeof( jq->staged ) );
		jq->e = jq->e->next; 
		OGUnlockMutex( jq->mut );

		staged->opaquev = 0;
		staged->opaquei = 0;
		staged->fn = 0;
		return 1;
	}

	OGUnlockMutex( jq->mut );
	return 0;
}

void CNOVRJobTack( cnovrQueueType q, cnovr_cb_fn fn, void * opaquev, int opaquei )
{
	CNOVRJobElement * newe = malloc( sizeof( CNOVRJobElement ) );
	newe->fn = fn;
	newe->opaquev = opaquev;
	newe->opaquei = opaquei;
	newe->next = 0;

	CNOVRJobQueue * jq = &CNOVRJEQ[q];
	OGLockMutex( jq->mut );
	CNOVRJobElement ** el = &CNOVRJEQ[q].e;
	while( *el )
	{
		CNOVRJobElement * elp = *el;
		//Don't add duplicates.
		if( elp->fn == fn && elp->opaquev == opaquev && elp->opaquei == opaquei ) goto skip;
		el = &elp->next;
	}
	//No duplicates?  Tack it on the end.
	*el = newe;
skip:
	OGUnlockSema( jq->sem );
	OGUnlockMutex( jq->mut );
}

void CNOVRJobCancel( cnovrQueueType q, cnovr_cb_fn fn, void * opaquev, int opaquei )
{
	CNOVRJobQueue * jq = &CNOVRJEQ[q];

	OGLockMutex( jq->mut );

	CNOVRJobElement * jes = &jq->staged;

	//If the exact message is currently happening, then spinlock.
	while( jes->fn == fn && jes->opaquev == opaquev && jes->opaquei == opaquei );

	//DO NOT Down-count the semaphore. 
	//It's ok.  You can have extra up semaphores.
	//Search for any matching blocks and bail.
	CNOVRJobElement ** el = &jq->e;
	while( *el )
	{
		CNOVRJobElement * elp = *el;
		//Don't add duplicates.
		if( elp->fn == fn && elp->opaquev == opaquev && elp->opaquei == opaquei )
		{
			*el = elp->next;
			break;
		}
	}

	OGUnlockMutex( jq->mut );
}




