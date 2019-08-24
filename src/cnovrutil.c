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

// Warning: You can only have one consumer per queue with the way locking is currently set up.
typedef struct CNOVRJobElement_t
{
	cnovr_cb_fn * fn;
	void * opaquev;
	int opaquei;
	struct CNOVRJobElement_t * next;
	struct CNOVRJobElement_t * prev;
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


static uint32_t JQhash( void * key, void * opaque ) { CNOVRJobElement * he = (CNOVRJobElement*)key; return ( (uint32_t)(he->fn-((cnovr_cb_fn*)0)) + (uint32_t)(he->opaquev-((void*)0)) + he->opaquei ); }
static int      JQcomp( void * key_a, void * key_b, void * opaque )
{
	if( !key_a || !key_b ) return 1;
	CNOVRJobElement * he = (CNOVRJobElement*)key_a;
	CNOVRJobElement * hf = (CNOVRJobElement*)key_b;
	if( he->fn == hf->fn && 
		he->opaquev == hf->opaquev &&
		he->opaquei == hf->opaquei)
		return 0;
	else
		return 1;
} 

static CNOVRJobQueue CNOVRJEQ[cnovrQMAX];

void DEBUGDumpQueue( cnovrQueueType qt )
{
	CNOVRJobQueue * q = &CNOVRJEQ[qt];
	printf( "Q: FRONT: %p   BACK: %p\n", q->front, q->back );
	CNOVRJobElement * e = q->front;
	while( e )
	{
		printf( "  <<%16p %16p(%4d) %16p>>\n",  e->prev, e, e->opaquei, e->next );
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
			jq->front = front->next; 
			if( !jq->front ) jq->back = 0;
			if( jq->front ) jq->front->prev = 0; //safety
			free( front );
			CNHashDelete( jq->hash, staged );
		}
		OGUnlockMutex( jq->mut );

		if( front )
		{
			staged->fn( staged->opaquev, staged->opaquei );

			//If you were to cancel the job, spinlock until e->staged == 0.
			staged->opaquev = 0;
			staged->opaquei = 0;
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
	printf( "JQUI\n" );
	CNOVRJobQueue * jq = &CNOVRJEQ[q];
	OGLockMutex( jq->mut );
	CNOVRJobElement * front = jq->front;
	if( front )
	{
		CNOVRJobElement * staged = &(jq->staged);
		memcpy( staged, front, sizeof( jq->staged ) );
		jq->front = front->next;
		if( !jq->front ) jq->back = 0;
		if( jq->front ) jq->front->prev = 0; //safety
		free( front );
		CNHashDelete( jq->hash, staged );
		OGUnlockMutex( jq->mut );

printf( "IN\n" );
		staged->fn( staged->opaquev, staged->opaquei );
		staged->opaquev = 0;
		staged->opaquei = 0;
		staged->fn = 0;
printf( "X\n" );
		//In case any close-outs were pending.
		//XXX TODO: Stress test verify no race condition.
		OGLockMutex( jq->mut );
		while( OGGetSema( jq->pendingsem ) == 0 ) OGUnlockSema( jq->pendingsem ); 
		OGUnlockMutex( jq->mut );
printf( "Y\n" );
		return 1;
	}

	OGUnlockMutex( jq->mut );
	return 0;
}

void CNOVRJobTack( cnovrQueueType q, cnovr_cb_fn fn, void * opaquev, int opaquei, int insert_even_if_pending )
{
	CNOVRJobElement * newe = malloc( sizeof( CNOVRJobElement ) );
	newe->fn = fn;
	newe->opaquev = opaquev;
	newe->opaquei = opaquei;
	newe->next = 0;
	newe->prev = 0;

	CNOVRJobQueue * jq = &CNOVRJEQ[q];

	OGLockMutex( jq->mut );
	int is_pending = JQcomp( newe, &jq->staged, 0 ) == 0;

	//Look for duplicates
	if( ( is_pending && !insert_even_if_pending ) || CNHashInsert( jq->hash, newe, newe ) )
	{
		printf( "INSERT FAILED\n" );
		//Failed to insert.
		free( newe );
	}
	else
	{
		printf( "Inserting %d\n", newe->opaquei );
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
	}
	OGUnlockSema( jq->sem );
	OGUnlockMutex( jq->mut );
}

void CNOVRJobCancel( cnovrQueueType q, cnovr_cb_fn fn, void * opaquev, int opaquei, int wait_on_pending )
{
	CNOVRJobElement compe;
	compe.fn = fn;
	compe.opaquev = opaquev;
	compe.opaquei = opaquei;

	CNOVRJobQueue * jq = &CNOVRJEQ[q];

	OGLockMutex( jq->mut );

	while( wait_on_pending && (JQcomp( &compe, &jq->staged, 0 ) == 0) )
	{
		//We're going to need to spin on the currently staged operation.
		OGUnlockMutex( jq->mut );
		OGLockSema( jq->pendingsem );
		OGLockMutex( jq->mut );
	}

	//Look for duplicates
	CNOVRJobElement * dupat = (CNOVRJobElement*)CNHashGet( jq->hash, &compe );
	if( dupat )
	{
		//There is a duplicate!
		if( dupat->prev )		dupat->prev->next = dupat->next;
		if( dupat->next )		dupat->next->prev = dupat->prev;
		if( dupat == jq->front )jq->front = dupat->next;
		if( dupat == jq->back )	jq->back = dupat->prev;

		CNHashDelete( jq->hash, dupat );
		free( dupat );
	}
	OGUnlockMutex( jq->mut );
}




