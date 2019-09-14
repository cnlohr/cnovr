#include "cnovrutil.h"
#include <cnovrutil.h>
#include <stdlib.h>
#include <cnhash.h>
#include <os_generic.h>
#include <string.h>
#include <stdio.h>
#include "cnovrindexedlist.h"
#include <stdarg.h>
#include "cnovrtccinterface.h"

#if defined(WINDOWS) || defined( WIN32 ) || defined( WIN64 )
#include <windows.h>
#endif

////////////////////////////////////////////////////////////////////////////////

struct casprintfmt
{
	int size;
	char * buffer;
};
static og_tls_t casprintftls;

int tasprintf( char ** dat, const char * fmt, ... )
{
	int n;
	if( !casprintftls ) casprintftls = OGCreateTLS();
	struct casprintfmt * ca = OGGetTLS( casprintftls );
	if( !ca )
	{
		ca = malloc( sizeof( struct casprintfmt ) );
		ca->size = 256;
		ca->buffer = malloc( ca->size );
		OGSetTLS( casprintftls, ca );
	}
	va_list ap;
	while (1) {
		va_start(ap, fmt);
		n = vsnprintf( ca->buffer, ca->size-1, fmt, ap );
		va_end(ap);
		if( n < 0 )
			return n;
		if( n < ca->size-1 )
		{
			*dat = ca->buffer;
			return n;
		}
		ca->size *= 2;
		ca->buffer = realloc( ca->buffer, ca->size );
	}
}

char * jsmnstrdup( const char * data, int start, int end )
{
	if( !casprintftls ) casprintftls = OGCreateTLS();
	struct casprintfmt * ca = OGGetTLS( casprintftls );
	if( !ca )
	{
		ca = malloc( sizeof( struct casprintfmt ) );
		ca->size = 256;
		ca->buffer = malloc( ca->size );
		OGSetTLS( casprintftls, ca );
	}
	int len = end - start;
	if( ca->size <= len + 1 )
	{
		ca->size *= 2;
		ca->buffer = realloc( ca->buffer, ca->size );
	}
	memcpy( ca->buffer, data + start, len );
	ca->buffer[len] = 0;
	return ca->buffer;
}




void Internalcasprintffree()
{
	if( !casprintftls ) return;
	struct casprintfmt * ca = OGGetTLS( casprintftls );
	if( ca )
	{
		free( ca->buffer );
		free( ca );
	}
}

////////////////////////////////////////////////////////////////////////////////

static cnhashtable * namedptrtable;
static og_mutex_t  namedptrmutex;

struct NamedPtrType
{
	char * typename;
	uint8_t data[1];
};

void * GetNamedPtr( const char * namedptr, const char * type )
{
	struct NamedPtrType * ret;
	OGTSLockMutex( namedptrmutex );
	ret = (struct NamedPtrType *)CNHashGetValue( namedptrtable, namedptr );
	OGTSUnlockMutex( namedptrmutex );
	if( strcmp( type, ret->typename ) == 0 )
		return ret->data;
	return 0;
}

void * NamedPtrFn( const char * namedptr, const char * type, int size )
{
	cnhashelement * e;
	OGTSLockMutex( namedptrmutex );
	e = CNHashIndex( namedptrtable, namedptr ); //If get fails, insert... Same as Insert for non-duplicate hashes.
	if( !e->data )
	{
		int typelen = strlen( type );
		e->data = malloc( sizeof( struct NamedPtrType ) + size + typelen + 1 );
		struct NamedPtrType * t = (struct NamedPtrType*)e->data;
		char * typenameptr = (char*)&t->data[size];
		t->typename = typenameptr;
		memcpy( typenameptr, type, typelen + 1 );
		memset( t->data, 0, size );
		OGUnlockMutex( namedptrmutex );
		return t->data;	
	}
	struct NamedPtrType * t = ((struct NamedPtrType*)e->data);
	OGUnlockMutex( namedptrmutex );
	if( strcmp( type, t->typename ) == 0 )
		return t->data;
	return 0;
}

void InternalSetupNamedPtrs()
{
	namedptrmutex = OGCreateMutex();
	namedptrtable = CNHashGenerate( 0, 0, CNHASH_STRINGS );
}

void InternalShutdownNamedPtrs()
{
	OGLockMutex( namedptrmutex );
	CNHashDestroy( namedptrtable );
	OGDeleteMutex( namedptrmutex );
}

////////////////////////////////////////////////////////////////////////////////

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

char ** SplitStrings( const char * line, char * split, char * white, bool merge_fields )
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


#define MAX_SEARCH_PATHS 10

static char * search_paths[MAX_SEARCH_PATHS];
static og_mutex_t search_paths_mutex;
static og_tls_t   search_path_return;

#if defined(WINDOWS) || defined( WIN32 ) || defined( WIN64 )
#include <windows.h>
int CheckFileExists(const char * szPath)
{
  DWORD dwAttrib = GetFileAttributesA(szPath);

  return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
         !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}
#endif

char * FileSearch( const char * fname )
{
	#if defined(WINDOWS) || defined( WIN32 ) || defined( WIN64 )
	#else
	#define CheckFileExists(x) ( stat( x, &sbuf ) == 0 )
	#endif

	char * cret = OGGetTLS( search_path_return );
	int fnamelen = strlen( fname );
	if( fnamelen >= CNOVR_MAX_PATH ) 
	{
		return 0;
	}

	if( !cret )
	{
		cret = malloc( CNOVR_MAX_PATH );
	}

	if( CheckFileExists( fname ) )
	{
		//File already exists, as-is, is an absolute path, or in our working directory.
		strcpy( cret, fname );
		return cret;
	}

	OGTSLockMutex( search_paths_mutex );
	int i;

	//Search in reverse, find from most recent path first.
	for( i = MAX_SEARCH_PATHS-1; i >= 0; i-- )
	{
		if( search_paths[i] == 0 ) continue;
		int len = snprintf( cret, CNOVR_MAX_PATH, "%s/%s", search_paths[i], fname );
		if( len >= CNOVR_MAX_PATH-1 ) continue;	//Output path would be too long.
		if( CheckFileExists( cret ) )
		{
			break;
		}
	}
	if( i < 0 ) cret[0] = 0;
	OGTSUnlockMutex( search_paths_mutex );
	return cret;
}


void FileSearchAddPath( const char * path )
{
	if( search_paths_mutex == 0 )
	{
		search_paths_mutex = OGCreateMutex();
		search_path_return = OGCreateTLS();
	}

	OGTSLockMutex( search_paths_mutex );
	int i;
	for( i = 0; i < MAX_SEARCH_PATHS; i++ )
	{
		if( search_paths[i] == 0 )
		{
			search_paths[i] = strdup( path );
			break;
		}
	}
	OGTSUnlockMutex( search_paths_mutex );
}

void FileSearchRemovePath( const char * path )
{
	OGTSLockMutex( search_paths_mutex );
	int i;
	for( i = 0; i < MAX_SEARCH_PATHS; i++ )
	{
		if( search_paths[i] == 0 ) continue;
		if( strcmp( search_paths[i], path ) == 0 )
		{
			free( search_paths[i] );
			search_paths[i] = 0;
		}
	}
	OGUnlockMutex( search_paths_mutex );
}

void InternalFileSearchCloseThread()
{
	if( search_path_return )
	{
		char * cret = OGGetTLS( search_path_return );
		if( cret )
			free( cret );
	}
}

void InternalFileSearchShutdown()
{
	OGTSLockMutex( search_paths_mutex );
	int i;
	for( i = 0; i < MAX_SEARCH_PATHS; i++ )
	{
		if( search_paths[i] )
		{
			free( search_paths[i] );
			search_paths[i] = 0;
		}
	}
	OGTSUnlockMutex( search_paths_mutex );
	OGDeleteMutex( search_paths_mutex );
	OGDeleteTLS( search_path_return );
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
	void * tcctag;
	cnovr_cb_fn * fn;
	struct filetimetagged_t * next;
	struct filetimetagged_t * prev;
	CNOVRIndexedListByTag * correspondance;
	int called_this_set;
} filetimetagged;

typedef struct filetimedata_t
{
	double time;
	filetimetagged * front;
	int list_changed;
} filetimedata;

static CNOVRIndexedList * ftindexlist;
static filetimetagged ftstaged; //Current callback, used to make sure we don't delete something ongoing.

void * thdfiletimechecker( void * v )
{
	int i;
	while( !intStopFileTimeCacher )
	{
		//Tricky: This is safe because array_size only increases.
		OGTSLockMutex( mutFileTimeCacher );
		for( i = 0; i < htFileTimeCacher->array_size; i++ )
		{
			cnhashelement * e = htFileTimeCacher->elements + i;
			if( e->data )
			{
				double ft = OGGetFileTime( e->key );
				filetimedata * front = ((filetimedata*)e->data);
				filetimedata * k = front;
				if( k->time != ft )
				{
					double origtime = k->time;
					k->time = ft;

					if( k->time > 1 ) //Make sure this isn't a first-time catch.
					{
						filetimetagged * l;
						filetimetagged * staged;

						l = k->front;
						while( l )
						{
							l->called_this_set = 0;
							l = l->next;
						}
refresh_set:
						front = ((filetimedata*)e->data);
						staged = &ftstaged;
						l = k->front;
						while( l )
						{
							staged->tag = l->tag;
							staged->opaquev = l->opaquev;
							staged->fn = l->fn;
							staged->tcctag = l->tcctag;
							if( l->called_this_set ) { l = l->next; continue; }
							l->called_this_set = 1;
							front->list_changed = 0; //Would not be possible to trigger in callback.
							OGTSUnlockMutex( mutFileTimeCacher );
							printf( "calling %p with *%p* %p in %p\n", l->fn, e->key, l->opaquev, l->tag );
							if( l->fn ) TCCInvocation( l->tcctag, l->fn( l->tag, l->opaquev ) );
							OGTSLockMutex( mutFileTimeCacher );
							if( front->list_changed )
							{
								front->list_changed = 0;
								goto refresh_set;
							}
							staged->tag = 0;
							staged->opaquev = 0;
							staged->fn = 0;
							staged->tcctag = 0;
							l = l->next;
						}
					}
				}
				while( OGGetSema( semPendinger ) == 0 ) OGUnlockSema( semPendinger ); 
				OGTSUnlockMutex( mutFileTimeCacher );
				OGUSleep( 1000 );
				//CNOVRListCall( cnovrLFTCheck, 0, 0 );
				OGTSLockMutex( mutFileTimeCacher );
			}
		}
		OGTSUnlockMutex( mutFileTimeCacher );
		OGUSleep( 1000 );
	}
	return 0;
}

double FileTimeCached( const char * fname )
{
	OGTSLockMutex( mutFileTimeCacher );
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
	OGTSUnlockMutex( mutFileTimeCacher );
	return 0;
}

static void ftremovefn( void * tag, void * item, void * opaque )
{
	filetimetagged * t = (filetimetagged*)item;
	filetimedata * d = (filetimedata*)opaque;

	d->list_changed = 1;

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

void CNOVRFileTimeAddWatch( const char * fname, cnovr_cb_fn fn, void * tag, void * opaquev )
{
	printf( "Adding FileTimeWatch %s %p\n", fname, fn );
	OGTSLockMutex( mutFileTimeCacher );
	filetimedata * ftd = (filetimedata*)CNHashGetValue( htFileTimeCacher, (void*)fname );
	if( !ftd )
	{
		ftd = malloc( sizeof( filetimedata ) );
		ftd->front = 0;
		ftd->time = 0;
		CNHashInsert( htFileTimeCacher, strdup( fname ), ftd );
	}

	//First, see if the element already exists.
	filetimetagged * tmp = ftd->front;
	while( tmp )
	{
		if( tmp->tag == tag && tmp->opaquev == opaquev && tmp->fn == fn ) goto failthrough;
		tmp = tmp->next;
	}

	filetimetagged * t = malloc( sizeof( filetimetagged ) );
	t->tag = tag;
	t->opaquev = opaquev;
	t->fn = fn;
	t->tcctag = TCCGetTag();
	t->prev = 0;
	t->next = ftd->front;
	if( t->next )
	{
		t->next->prev = t;
	}
	ftd->front = t;
	t->correspondance = CNOVRIndexedListInsert( ftindexlist, tag, t, ftd );

failthrough:
	OGUnlockMutex( mutFileTimeCacher );
}


void CNOVRFileTimeRemoveWatch( const char * fname, cnovr_cb_fn fn, void * tag, void * opaquev )
{
	OGTSLockMutex( mutFileTimeCacher );
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
	ret->list_changed = 1;
	OGTSUnlockMutex( mutFileTimeCacher );
}

void CNOVRFileTimeRemoveTagged( void * tag, int wait_on_pending )
{
	OGTSLockMutex( mutFileTimeCacher );
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
	void * tcctag;
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
	bool is_staged;
	cnhashtable * hash;
	og_mutex_t mut;
	og_sema_t  sem;
	og_sema_t  pendingsem;

	//Prevent any new queuing of objects if deleting a tag.
	og_mutex_t deletingmut;
	void * deletingtag;
	bool deletingnow;
	bool quittingnow;
} CNOVRJobQueue;

static intptr_t JQhash( const void * key, void * opaque ) { CNOVRJobElement * he = (CNOVRJobElement*)key; return ( ((uint32_t)(he->fn-((cnovr_cb_fn*)0)) + (uint32_t)(he->tag-((void*)0)) + (uint32_t)(he->opaquev - (void*)0) )) | 1; }
static int      JQcomp( const void * key_a, const void * key_b, void * opaque )
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

static og_thread_t jt1;
static og_thread_t jt2;

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
	while( !jq->quittingnow )
	{
		OGLockSema( jq->sem );
		OGTSLockMutex( jq->mut );
		CNOVRJobElement * front = jq->front;
		CNOVRJobElement * staged = &(jq->staged);
		if( front )
		{
			memcpy( staged, front, sizeof( jq->staged ) );
			jq->is_staged = 1;
			BackendDeleteJob( jq, front );
		}
		OGTSUnlockMutex( jq->mut );

		if( front )
		{
			if( staged->fn ) TCCInvocation( staged->tcctag, staged->fn( staged->tag, staged->opaquev ) );
			
			//If you were to cancel the job, spinlock until e->staged == 0.
			staged->tag = 0;
			staged->tcctag = 0;
			staged->opaquev = 0;
			staged->fn = 0;
			jq->is_staged = 0;
			//In case any close-outs were pending.
			OGTSLockMutex( jq->mut );
			while( OGGetSema( jq->pendingsem ) == 0 ) OGUnlockSema( jq->pendingsem ); 
			OGTSUnlockMutex( jq->mut );
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
		CNOVRJobQueue * jq = &CNOVRJEQ[i];
		jq->mut = OGCreateMutex();
		jq->sem = OGCreateSema();
		jq->pendingsem = OGCreateSema();
		jq->deletingmut = OGCreateMutex();
		jq->deletingtag = 0;
		jq->deletingnow = 0;
		jq->front = 0;
		jq->back = 0;
		jq->is_staged = 0;
		jq->hash = CNHashGenerate( 0, 0, 0, JQhash, JQcomp, 0 );
		jq->quittingnow = 0;
		memset( &CNOVRJEQ[i].staged, 0, sizeof( CNOVRJEQ[i].staged ) );
	}

	jt1 = OGCreateThread( CNOVRJobProcessor, &CNOVRJEQ[cnovrQLow] );
	jt2 = OGCreateThread( CNOVRJobProcessor, &CNOVRJEQ[cnovrQAsync] );
}

void CNOVRJobStop()
{
	int i;
	for( i = 0; i < cnovrQMAX; i++ )
	{
		CNOVRJobQueue * jq = &CNOVRJEQ[i];
		jq->quittingnow = 1;
		OGUnlockSema( jq->sem );
	}

	OGJoinThread( jt1 );
	OGJoinThread( jt2 );

	CNOVRIndexedListDestroy( JQELIST );

	for( i = 0; i < cnovrQMAX; i++ )
	{
		CNOVRJobQueue * jq = &CNOVRJEQ[i];
		CNHashDestroy( jq->hash );
		OGDeleteSema( jq->sem );
		OGDeleteSema( jq->pendingsem );
		OGDeleteMutex( jq->deletingmut );
		OGDeleteMutex( jq->mut );
	}
}

int CNOVRJobProcessQueueElement( cnovrQueueType q )
{
	CNOVRJobQueue * jq = &CNOVRJEQ[q];
	OGTSLockMutex( jq->mut );
	CNOVRJobElement * front = jq->front;
	if( front )
	{
		CNOVRJobElement * staged = &(jq->staged);
		memcpy( staged, front, sizeof( jq->staged ) );
		jq->is_staged = 1;

		BackendDeleteJob( jq, front );
		OGUnlockMutex( jq->mut );

		if( staged->fn ) TCCInvocation( staged->tcctag, staged->fn( staged->tag, staged->opaquev ) );

		jq->is_staged = 0;
		staged->tag = 0;
		staged->tcctag = 0;
		staged->opaquev = 0;
		staged->fn = 0;

		//In case any close-outs were pending.
		//This is probably a place worth peeking if there's a problem found with this code
		//verify no race condition in your particular application/fitness
		OGTSLockMutex( jq->mut );
		while( OGGetSema( jq->pendingsem ) == 0 ) OGUnlockSema( jq->pendingsem ); 
		OGUnlockMutex( jq->mut );
		return 1;
	}

	OGUnlockMutex( jq->mut );
	return 0;
}

void CNOVRJobTack( cnovrQueueType q, cnovr_cb_fn fn, void * tag, void * opaquev, bool insert_even_if_pending )
{
	CNOVRJobElement * newe = malloc( sizeof( CNOVRJobElement ) );
	newe->fn = fn;
	newe->tag = tag;
	newe->tcctag = TCCGetTag();
	newe->opaquev = opaquev;
	newe->next = 0;
	newe->prev = 0;

	CNOVRJobQueue * jq = &CNOVRJEQ[q];

	OGTSLockMutex( jq->mut );

	//Make sure we don't permit addition of a delete-in-progress, in case the user has chained events.
	if( jq->deletingnow && jq->deletingtag == tag ) goto fail;

	int is_pending = JQcomp( newe, &jq->staged, 0 ) == 0;

	//Look for duplicates
	if( ( is_pending && !insert_even_if_pending ) || CNHashInsert( jq->hash, newe, newe ) == 0 )
	{
		goto fail;
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
	return;
fail:
	//Failed to insert.
	free( newe );
	OGUnlockMutex( jq->mut );
}

void CNOVRJobCancel( cnovrQueueType q, cnovr_cb_fn fn, void * tag, void * opaquev, bool wait_on_pending )
{
	CNOVRJobElement compe;
	compe.fn = fn;
	compe.tag = tag;
	compe.opaquev = opaquev;

	CNOVRJobQueue * jq = &CNOVRJEQ[q];

	OGTSLockMutex( jq->mut );
	//Look for job tdelete
	CNOVRJobElement * dupat = (CNOVRJobElement*)CNHashGetValue( jq->hash, &compe );
	if( dupat )
	{
		BackendDeleteJob( jq, dupat );
	}
	OGUnlockMutex( jq->mut );

	//Make srue we don't have any remaining pends.
	OGUnlockSema( jq->pendingsem );
	while( wait_on_pending && jq->is_staged && (JQcomp( &compe, &jq->staged, 0 ) == 0) )
	{
		OGLockSema( jq->pendingsem );
	}
}

void CNOVRJobCancelAllTag( void * tag, int wait_on_pending )
{
	int list;
	for( list = 0; list < cnovrQMAX; list++ )
	{
		OGTSLockMutex( CNOVRJEQ[list].mut );
		OGTSLockMutex( CNOVRJEQ[list].deletingmut );
		CNOVRJEQ[list].deletingtag = tag;
		CNOVRJEQ[list].deletingnow = 1;
	}
	CNOVRIndexedListDeleteTag( JQELIST, tag );
	for( list = 0; list < cnovrQMAX; list++ )
	{
		OGTSUnlockMutex( CNOVRJEQ[list].mut );
	}

	for( list = 0; list < cnovrQMAX; list++ )
	{
		CNOVRJobQueue * jq = &CNOVRJEQ[list];
		OGUnlockSema( jq->pendingsem );
		//XXX TRICKY: this seems to sometimes fail, locked open.
		while( wait_on_pending && jq->is_staged && jq->staged.tag == tag )
		{
			OGLockSema( jq->pendingsem );
		}
	}
	for( list = 0; list < cnovrQMAX; list++ )
	{
		CNOVRJEQ[list].deletingtag = 0;
		OGTSUnlockMutex( CNOVRJEQ[list].deletingmut );
	}
}

///////////////////////////////////////////////////////////////////////////////

typedef struct JobListItem_t
{
	cnovr_cb_fn * fn;
	void * tcctag;
} JobListItem;

static cnhashtable * ListHTs[cnovrLMAX];
static og_mutex_t    ListMTs[cnovrLMAX];

void DeleteJLE( void * key, void * data, void * opaque )
{
	free( data );
}

void CNOVRListSystemInit()
{
	int i;
	for( i = 0; i < cnovrLMAX; i++ )
	{
		ListHTs[i] = CNHashGenerate( 0, 0, 0, cnhash_ptrhf, cnhash_ptrcf, DeleteJLE );
 
		ListMTs[i] = OGCreateMutex();
	}
}

void CNOVRListSystemDestroy()
{	
	int i;
	for( i = 0; i < cnovrLMAX; i++ )
	{
		OGLockMutex( ListMTs[i] );
		CNHashDestroy( ListHTs[i] );
		OGDeleteMutex( ListMTs[i] );
	}
}

void CNOVRListCall( cnovrRunList l, void * data, int delete_on_call )
{
	cnhashtable * t = ListHTs[l];
	og_mutex_t  m = ListMTs[l];
	int i;
	OGTSLockMutex( m );
	for( i = 0; i < t->array_size; i++ )
	{
		cnhashelement * e = &t->elements[i];
		JobListItem * jle = (JobListItem*)e->data;
		if( jle && jle->fn )
		{
			OGTSUnlockMutex( m );
			TCCInvocation( jle->tcctag, jle->fn( e->key, data ) );
			OGTSLockMutex( m );
			CNHashDelete( t, e->key );
		}
	}
	OGTSUnlockMutex( m );
}


void CNOVRListAdd( cnovrRunList l, void * b, cnovr_cb_fn * fn )
{
	og_mutex_t  m = ListMTs[l];
	cnhashelement * e;

	OGTSLockMutex( m );
	e = CNHashInsert( ListHTs[l], b, fn );
	JobListItem * jli = e->data;
	if( !jli ) jli = malloc( sizeof( JobListItem ) );
	jli->fn = fn;
	jli->tcctag = TCCGetTag();
	e->data = jli;	//Overwrite if called again.
	OGTSUnlockMutex( m );
}

void CNOVRListDeleteTag( void * b )
{
	int l;
	for( l = 0; l < cnovrLMAX; l++ )
	{
		og_mutex_t  m = ListMTs[l];
		OGTSLockMutex( m );
		CNHashDelete( ListHTs[l], b );
		OGTSUnlockMutex( m );
	}
}




