#include "cnovrutil.h"
#include <cnovrutil.h>
#include <stdlib.h>
#include <cnhash.h>
#include <os_generic.h>
#include <string.h>
#include <stdio.h>
#include "cnovrindexedlist.h"
#include <stdarg.h>
#include <cnrbtree.h>
#include <stretchy_buffer.h>
#include "cnovrtccinterface.h"
#include "cnovr.h"

#if defined(WINDOWS) || defined( WIN32 ) || defined( WIN64 )
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

////////////////////////////////////////////////////////////////////////////////

struct casprintfmt
{
	//We have multiple buffers here so we can ping-pong between them to prevent
	//trprintf from exploding if printing a buffer it was passed from a
	//previous call to itself.
	int size;
	char * buffer;
	int sizeback;
	char * bufferback;
};

static og_tls_t casprintftls;

void CNOVRInternalClosePrintfBuffer()
{
	struct casprintfmt * ca  = OGGetTLS( casprintftls );
	if( !ca )
	{
		free( ca->buffer );
		free( ca->bufferback );
		free( ca );
	}
	OGSetTLS( ca, 0 );
}

static struct casprintfmt * GetOrInitCASPRINTFMT()
{
	if( !casprintftls ) casprintftls = OGCreateTLS();
	struct casprintfmt * ca  = OGGetTLS( casprintftls );
	if( !ca )
	{
		//For now, we just malloc this junk.  I don't know why.
		//But, if we CNOVRThreadMalloc this buffer and other data, everything
		//seems to come to a flaming halt.
		ca = malloc( sizeof( struct casprintfmt ) );
		ca->size = 128;
		ca->buffer = malloc( ca->size );
		ca->sizeback = 128;
		ca->bufferback = malloc( ca->sizeback );
		OGSetTLS( casprintftls, ca );
	}
	return ca;
}

int tasprintf( char ** dat, const char * fmt, ... )
{
	va_list ap;
	va_start( ap, fmt);
	int ret = tvasprintf( dat, fmt, ap );
	va_end( ap );
	return ret;
}

char * trprintf( const char * format, ... )
{
	va_list ap;
	va_start(ap, format);
	char * ret;
	tvasprintf( &ret, format, ap );
	va_end( ap );
	return ret;
}


int tvasprintf( char ** dat, const char * fmt, va_list ap )
{
	int n;
	struct casprintfmt * ca = GetOrInitCASPRINTFMT();

	//What if one of the strings being passed in are from a trprintf()?
	//Originally, something bad happens, but we are changing that and
	//temporarily writing into a stack buffer. 

	while (1) {
		va_list aq;
		va_copy(aq, ap);
		n = vsnprintf( ca->buffer, ca->size-1, fmt, aq );
		va_end( aq );
		if( n < ca->size-1 && n != -1 )
		{
			//Ping-pong the buffers.
			char * hold1 = ca->buffer;
			int hold1s = ca->size;
			ca->buffer = ca->bufferback;
			ca->size = ca->sizeback;
			ca->bufferback = hold1;
			ca->sizeback = hold1s;
			*dat = hold1;
			return n;
		}
		ca->size *= 2;
		ca->buffer = CNOVRThreadRealloc( ca->buffer, ca->size );
	}
}

char * jsmnstrsn( char * outbuffer, int outlen, const char * data, int start, int end )
{
	int copylen = end-start;
	if( copylen > outlen-1 )
	{
		copylen = outlen - 1;
	}
	strncpy( outbuffer, data + start, copylen );
	outbuffer[copylen] = 0;
	return outbuffer;
}

int    jsmnintparse( const char * data, int start, int end )
{
	int len = end - start;
	char buffer[len+1];
	memcpy( buffer, data + start, len );
	buffer[len] = 0;
	if( strcmp( buffer, "true" ) == 0 ) return 1;
	else return atoi( buffer );
}



////////////////////////////////////////////////////////////////////////////////

static cnhashtable * namedptrtable;
static og_mutex_t  namedptrmutex;

struct NamedPtrType
{
	char * typename;
	int length;
	uint8_t data[1];
};

void * CNOVRNamedPtrGet( const char * namedptr, const char * type )
{
	struct NamedPtrType * ret;
	OGTSLockMutex( namedptrmutex );
	ret = (struct NamedPtrType *)CNHashGetValue( namedptrtable, namedptr );
	OGTSUnlockMutex( namedptrmutex );
	if( ret && ( !type || !*type || strcmp( type, ret->typename ) == 0 ) )
		return ret->data;
	return 0;
}

void * CNOVRNamedPtrData( const char * namedptr, const char * type, int size )
{
	//We do not need to do OGTS locking here, since it is not calling anything from
	//within the save zone that is expected to crash.
	cnhashelement * e;
	OGLockMutex( namedptrmutex );
	e = CNHashIndex( namedptrtable, namedptr ); //If get fails, insert... Same as Insert for non-duplicate hashes.
	if( !e->data )
	{
		int typelen = type?strlen( type ):0;
		e->data = malloc( sizeof( struct NamedPtrType ) + size + typelen + 1 );
		struct NamedPtrType * t = (struct NamedPtrType*)e->data;
		char * typenameptr = (char*)&t->data[size];
		t->typename = typenameptr;
		t->length = size;
		if( type ) memcpy( typenameptr, type, typelen + 1 );
		else typenameptr[0] = 0;

		//See if we have a saved copy.
		char stpath[CNOVR_MAX_PATH];
		snprintf( stpath, CNOVR_MAX_PATH-1, "savenameptr/%s.%s.dat", namedptr, type?type:"" );
		FILE * f = fopen( stpath, "rb" );
		if( f )
		{
			fread( t->data, 1, size, f );
			fclose( f );
		}
		else
		{
			memset( t->data, 0, size );
		}
		OGUnlockMutex( namedptrmutex );
		return t->data;	
	}
	struct NamedPtrType * t = ((struct NamedPtrType*)e->data);
	OGUnlockMutex( namedptrmutex );
	if( !type || strcmp( type, t->typename ) == 0 )
		return t->data;
	return 0;
}

void CNOVRNamedPtrSave( const char * namedptr )
{
	struct NamedPtrType * ret;
	OGTSLockMutex( namedptrmutex );
	ret = (struct NamedPtrType *)CNHashGetValue( namedptrtable, namedptr );
	OGTSUnlockMutex( namedptrmutex );
	if( ret )
	{
		char stpath[CNOVR_MAX_PATH];
		snprintf( stpath, CNOVR_MAX_PATH-1, "savenameptr/%s.%s.dat", namedptr, ret->typename?ret->typename:"" );
		FILE * f = fopen(  stpath, "wb" );
		if( f ) 
		{
			fwrite( ret->data, ret->length, 1, f );
			fclose( f );
		}
	}
}

void CNOVRNamedPtrRevert( const char * namedptr )
{
	struct NamedPtrType * ret;
	OGTSLockMutex( namedptrmutex );
	ret = (struct NamedPtrType *)CNHashGetValue( namedptrtable, namedptr );
	OGTSUnlockMutex( namedptrmutex );
	if( ret )
	{
		char stpath[CNOVR_MAX_PATH];
		snprintf( stpath, CNOVR_MAX_PATH-1, "savenameptr/%s.%s.dat", namedptr, ret->typename?ret->typename:"" );
		FILE * f = fopen(  stpath, "rb" );
		if( f ) 
		{
			fread( ret->data, ret->length, 1, f );
			fclose( f );
		}
	}
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

char ** CNOVRFolderListing( const char * path, int * elements )
{
	struct linkedstrlist
	{
		struct linkedstrlist * next;
		int len;	//Including null terminator.
		char str[1];
	};
	struct linkedstrlist head;
	head.str[0] = 0;
	head.len = 0;
	head.next = 0;
	int entries = 0;
	int needed_bytes = 1;
	struct linkedstrlist * tail = &head;
#if defined(WINDOWS) || defined( WIN32 ) || defined( WIN64 )
	//Currently untested in Windows
	WIN32_FIND_DATA ffd;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	int pathlen = strlen( path );
	char wildcardpath[pathlen+3];
	memcpy( wildcardpath, path, pathlen );
	wildcardpath[pathlen+0] = '/';
	wildcardpath[pathlen+1] = '*';
	wildcardpath[pathlen+2] = '\0';
	hFind = FindFirstFile( wildcardpath, &ffd);
	if( INVALID_HANDLE_VALUE != hFind )
	{
		do
		{
			if( !( ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) ) 
			{
				// filesize.LowPart = ffd.nFileSizeLow;
				// filesize.HighPart = ffd.nFileSizeHigh;
				// filesize.QuadPart
				int len = strlen( ffd.cFileName ) + 1;
				struct linkedstrlist * next = malloc( len + sizeof( struct linkedstrlist ) );	//alloca broken in windows?
				memcpy( next->str, ffd.cFileName, len );
				next->next = 0;
				next->len = len;
				tail->next = next;
				tail = next;
				needed_bytes += len;
				entries++;
			}
		} while (FindNextFile(hFind, &ffd) != 0);
		FindClose(hFind);
	}
#else
	struct dirent *dir;
	DIR * d = opendir( path );
	char full_path[CNOVR_MAX_PATH];
	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			//Condition to check regular file.
			if( dir->d_type == DT_REG )
			{
				int len = strlen( dir->d_name ) + 1;
				struct linkedstrlist * next = alloca( len + sizeof( struct linkedstrlist ) );
				memcpy( next->str, dir->d_name, len );
				next->next = 0;
				next->len = len;
				tail->next = next;
				tail = next;
				needed_bytes += len;
				entries++;
			}
		}
		closedir(d);
	}
#endif
	*elements = entries;
	tail = &head;
	char ** ret = malloc( (entries+1) * sizeof( char* ) + needed_bytes );
	char * data = ((char*)ret) + (entries+1) * sizeof( char* );
	int i;
	for( i = 0; i < entries; i++ )
	{
		struct linkedstrlist * next = tail->next;
#if defined(WINDOWS) || defined( WIN32 ) || defined( WIN64 )
		if( tail != &head ) free( tail );
#endif
		tail = next;
		ret[i] = data;
		int len = tail->len;
		memcpy( data, tail->str, len );
		data += len;
	}
	ret[i] = 0;
	return ret;
}

char * CNOVRFileToString( const char * fname, int * length )
{
	const char * rfn = CNOVRFileSearch( fname );
	if( !rfn ) return 0;
	FILE * f = fopen( rfn, "rb" );
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

char ** CNOVRSplitStrings( const char * line, char * split, char * white, int merge_fields, int * elementcount )
{
	if( elementcount ) *elementcount = 0;

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
			lengths[elements-1] = (did_hit_not_white)?(thislengthconfirm + 1):0; //XXX BUGGY ... Or is bad it?  I can't tell what's wrong.  the "buggy" note was from a previous coding session.
			ret[elements-1] = (char*)lstart + 0; //XXX BUGGY //I promise I won't change the value.
			needed_bytes += thislengthconfirm + 1;
			elements++;
			ret = realloc( ret, elements * sizeof( char * )  );
			lengths = realloc( lengths, elements * sizeof( int ) );
			lengths[elements-1] = 0;
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
	ret = realloc( ret, ( sizeof( char * ) + 1 ) * elements  + needed_bytes );
	char * retend = ((char*)ret) + ( (sizeof( char * )) * elements);
	int lensum1 = 0;
	for( i = 0; i < elements; i++ )
	{
		int len = lengths[i];
		lensum1 += len + 1;
		memcpy( retend, ret[i], len );
		retend[len] = 0;
		ret[i] = (i == elements-1)?0:retend;
		retend += len + 1;
	}
	if( elementcount && elements ) *elementcount = (thislength==0)?(elements-1):elements;
	free( lengths );
	return ret;
}

int CNOVRStringCompareEndingCase( const char * thing_to_search, const char * check_extension )
{
	if( !thing_to_search || !check_extension ) return -1;
	int tsclen = strlen( thing_to_search );
	return strcasecmp( thing_to_search + tsclen - strlen( check_extension ), check_extension );
}

char * CNOVRGetBaseFileName( const char * path )
{
	//returned in trprintf, so you can't re-use the buffer.
	int len = strlen( path );
	int i;
	int start = 0;
	int lastdot = len;
	char c = 0;
	for( i = len-1; i >= 0; i-- )
	{
		c = path[i];
		if( c == '/' || c == '\\' )
			break;
		if( c == '.' )
			lastdot = i;
	}
	if( c == '/' || c == '\\' )
		start = i+1;
	char * ret = trprintf( "%s", path+start );
	ret[lastdot-start] = 0;
	return ret;
}

#define MAX_SEARCH_PATHS 10

static char * search_paths[MAX_SEARCH_PATHS];
static og_mutex_t search_paths_mutex;
static og_tls_t   search_path_return;

static int InternalCheckFileExists( const char * fn );

#if defined(WINDOWS) || defined( WIN32 ) || defined( WIN64 )
#include <windows.h>
static int CheckFileExists(const char * szPath)
{
	if( InternalCheckFileExists( szPath ) ) return 1;
	DWORD dwAttrib = GetFileAttributesA(szPath);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
		!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}
int InternalCheckFileExists( const char * path ) { return 0; }
#endif

char * CNOVRFileSearch( const char * fname )
{
	#if !(defined(WINDOWS) || defined( WIN32 ) || defined( WIN64 ))
	struct stat sbuf;
	#define CheckFileExists(x) InternalCheckFileExists(x)?1:(( stat( x, &sbuf ) == 0 ))
	#endif

	char * cret = OGGetTLS( search_path_return );
	if( !cret ) { OGSetTLS( search_path_return, (cret = CNOVRThreadMalloc( CNOVR_MAX_PATH+1 ) ) ); }

	int fnamelen = strlen( fname );

	if( fnamelen >= CNOVR_MAX_PATH ) 
	{
		return 0;
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
	if( cret[0] == 0 ) return 0;
	else return cret;
}

char * CNOVRFileSearchAbsolute( const char * fname )
{
	char * pathfound = CNOVRFileSearch( fname );
	if( !pathfound ) return 0;
	int pathfoundlen = strlen( pathfound );
	char * pathfoundrealloc = alloca( pathfoundlen+2 );
	memcpy( pathfoundrealloc, pathfound, pathfoundlen+1 );
	char * cret = OGGetTLS( search_path_return );
	memset( cret, 0, CNOVR_MAX_PATH+1 );
#if defined( WIN32 ) || defined( WINDOWS ) || defined( WIN64 )
	_fullpath( cret, pathfoundrealloc, CNOVR_MAX_PATH-1 );
#else
	char * ctr = realpath( pathfoundrealloc, 0 );
	if( !ctr )
	{
		cret[0] = 0;
		return cret;
	}
	int cl = strlen(ctr);
	if( cl >= CNOVR_MAX_PATH ) cl = CNOVR_MAX_PATH-1;
	memcpy( cret, ctr, cl );
	free( ctr );
	cret[cl] = 0;
#endif
	return cret;
}


void CNOVRFileSearchAddPath( const char * path )
{
	TCCInstance * te = TCCGetTag();
	if( te && te->bClosing ) return;

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

void CNOVRFileSearchRemovePath( const char * path )
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

// Here, we hook fopen() in case we wanted to build files into the
//  exectuable cnovr is running from. 

#if defined( WIN32 ) || defined( WINDOWS )

//No wrapping fopen here.

#else

#include <dlfcn.h>

//An example in-memory file.
__attribute__((used)) const char IOF_testmemfile_txt[] = { 0x07, 0x00, 0x00, 0x00, 'm', 'e', 'm', 'f', 'i', 'l', 'e' };

static int InternalCheckFileExists( const char * fname )
{
	//See if this symbol already exists in the executable.
	char stbuff[PATH_MAX+5];

	//The symbol is IOF_(filename) where filename has .'s and /'s replaced with _'s
	stbuff[0] = 'I';
	stbuff[1] = 'O';
	stbuff[2] = 'F';
	stbuff[3] = '_';
	int i;
	for( i = 0; i < PATH_MAX; i++ )
	{
		char c = fname[i];
		if( c == '/' ) c = '_';
		if( c == '.' ) c = '_';
		stbuff[i+4] = c;
		if( c == 0 ) break;
	}
	uint8_t * v = tcccrash_symaddr( 0, stbuff ); //dlsym( 0/*RTLD_DEFAULT?*/, stbuff );
	if( v )
		return 1;
	else
		return 0;
}
#include <signal.h>

FILE * __real_fopen( const char * fname, const char * mode ) __attribute__((weak));
FILE * __wrap_fopen( const char * fname, const char * mode )
{
	if( mode && mode[0] == 'r' && fname && fname[0] )
	{
		//See if this symbol already exists in the executable.
		char stbuff[PATH_MAX+5];

		//The symbol is IOF_(filename) where filename has .'s and /'s replaced with _'s
		stbuff[0] = 'I';
		stbuff[1] = 'O';
		stbuff[2] = 'F';
		stbuff[3] = '_';
		int i;
		for( i = 0; i < PATH_MAX; i++ )
		{
			char c = fname[i];
			if( c == '/' ) c = '_';
			if( c == '.' ) c = '_';
			stbuff[i+4] = c;
			if( c == 0 ) break;
		}
		uint8_t * v = tcccrash_symaddr( 0, stbuff ); //dlsym( 0/*RTLD_DEFAULT?*/, stbuff );
		if( v )
		{
			int size = v[0] | (v[1]<<8) | (v[2]<<16) | (v[3]<<24);
			v += 4;
			return fmemopen( v, size, mode );
		}
		//File was not found.
	}
#if 0
	static FILE * flist;
	if( !flist ) flist = __real_fopen( "filelist.txt", "a" );
	fprintf( flist, "%s\n", fname );
	fflush(flist);
#endif
 
	return __real_fopen( fname, mode );
}

#endif

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
	double time_noticed;
} filetimedata;

static filetimedata * ftopscurrent; //Mechanism to make new adds from in-process adds not get run.


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
				double Now = OGGetAbsoluteTime();
				double ft = OGGetFileTime( e->key );
				filetimedata * front = ((filetimedata*)e->data);
				filetimedata * k = front;
				//Ugh.  Yucky logic.
				#define TIME_TO_WAIT_AFTER_FILE_CHANGE_BEFORE_DOING_SOMETHING .2
				if( k->time != ft && k->time_noticed == 0 ) k->time_noticed = Now;
				if( k->time != ft && ( Now - k->time_noticed > TIME_TO_WAIT_AFTER_FILE_CHANGE_BEFORE_DOING_SOMETHING || k->time < 1 ) )
				{
					k->time_noticed = 0;
					double origtime = k->time;
					k->time = ft;
					ftopscurrent = k;
					if( origtime > 1 ) //Make sure this isn't a first-time catch.
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
						e = htFileTimeCacher->elements + i; //Tricky: If the loop is hit, and we have a table alteration within the loop, the array could become corrupt.
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
							//printf( "calling %p with *%p* %p in %p\n", l->fn, e->key, l->opaquev, l->tag );
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
					ftopscurrent = 0;
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
	in->time_noticed = 0;
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

void StopFileTimeChekerThread()
{
	intStopFileTimeCacher = 1;
	OGJoinThread( thdFileTimeCacher ); 
}

void CNOVRInternalStopCacheSystem()
{
	OGDeleteMutex( mutFileTimeCacher );
	OGDeleteSema( semPendinger );

	CNOVRIndexedListDestroy( ftindexlist );

	//Anything else that hasn't been cleared out by deleting the ftindexlist can get handled here.
	//XXX TODO: Is there anything left?
	int k;
	for( k = 0; k < htFileTimeCacher->array_size; k++ )
	{
		cnhashelement * e = htFileTimeCacher->elements + k;
		if( e->data )
		{
			double ft = OGGetFileTime( e->key );
			filetimedata * frontelem = ((filetimedata*)e->data);
			filetimetagged * dat = frontelem->front;
			while( dat )
			{
				filetimetagged * of = dat;
				dat = dat->next;
				free( of );
			}
		}
	}
	CNHashDestroy( htFileTimeCacher );
}

void CNOVRFileTimeAddWatch( const char * fname, cnovr_cb_fn fn, void * tag, void * opaquev )
{
//	printf( "Adding FileTimeWatch %s [%p] %p\n", fname, tag, fn );
	OGTSLockMutex( mutFileTimeCacher );
	TCCInstance * te = TCCGetTag();
	if( te && te->bClosing ) goto failthrough;
	filetimedata * ftd = (filetimedata*)CNHashGetValue( htFileTimeCacher, (void*)fname );
	if( !ftd )
	{
		ftd = malloc( sizeof( filetimedata ) );
		ftd->front = 0;
		ftd->time = 0;
		ftd->time_noticed = 0;
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
	if( ftd == ftopscurrent )
	{
		t->called_this_set = 1;
	}
	else
	{
		t->called_this_set = 0;
	}

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

	TCCInstance * te = TCCGetTag();
	//printf( "TCE: %p %d\n", te, te?te->bClosing:0 );
	if( te && te->bClosing ) goto fail;

	//printf( "%d %p %p\n", jq->deletingnow, jq->deletingtag, tag );
	//Make sure we don't permit addition of a delete-in-progress, in case the user has chained events.
	if( jq->deletingnow && jq->deletingtag == tag && tag != 0) goto fail;

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
	OGTSUnlockMutex( jq->mut );

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

int CNOVRListCall( cnovrRunList l, void * data, int delete_on_call )
{
	cnhashtable * t = ListHTs[l];
	og_mutex_t  m = ListMTs[l];
	int i;
	int hit = 0;
	OGTSLockMutex( m );
	for( i = 0; i < t->array_size; i++ )
	{
		cnhashelement * e = &t->elements[i];
		JobListItem * jle = (JobListItem*)e->data;
		if( jle && jle->fn )
		{
			hit++;
			OGTSUnlockMutex( m );
			if( jle->fn ) TCCInvocation( jle->tcctag, jle->fn( e->key, data ) );
			OGTSLockMutex( m );
		}
	}
	OGTSUnlockMutex( m );
	return hit;
}


void CNOVRListAdd( cnovrRunList l, void * b, cnovr_cb_fn * fn )
{
	TCCInstance * te = TCCGetTag();
	if( te && te->bClosing ) return;
	og_mutex_t  m = ListMTs[l];
	cnhashelement * e;
	OGTSLockMutex( m );
	e = CNHashIndex( ListHTs[l], b );
	JobListItem * jli = e->data;
	if( !jli ) jli = malloc( sizeof( JobListItem ) );
	jli->fn = fn;
	jli->tcctag = te;
	if( e->data )
	{
		ovrprintf( "Warning overwriting element with CNOVRListAdd\n" );
	}
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

//XXX TODO: FIXME: This is very slow.  Re-architect to make this part fast.
void CNOVRListDeleteTCCTag( void * tcctag )
{
	int l;
	for( l = 0; l < cnovrLMAX; l++ )
	{
		og_mutex_t  m = ListMTs[l];
		OGTSLockMutex( m );
		int i;
		int len = ListHTs[l]->array_size;
		cnhashelement * e = ListHTs[l]->elements;
		for( i = 0; i < len; i++ )
		{
			JobListItem * t = (JobListItem*)e->data;
			if( t && t->tcctag == tcctag )
			{
				e->data = 0;
				e->key = 0;
				e->hashvalue = 0;
				free( t );
			}
			e++;
		}
		OGTSUnlockMutex( m );
	}
}


static int deletelaterpos;
static void ** deletelaterbuffers[FREE_LATER_LAG];

void CNOVRFreeLater( void * tofree )
{
	sb_push( deletelaterbuffers[deletelaterpos], tofree );
}

void CNOVRFreeLaterShutdown()
{
	int h;
	for( h = 0; h < FREE_LATER_LAG; h++ )
	{
		int i;
		void ** dlb = deletelaterbuffers[h];
		if( dlb )
		{
			int len = stb__sbn( dlb );
			for( i = 0; i < len; i++ )
			{
				free( dlb[i] );
			}
			sb_free( dlb );
			deletelaterbuffers[h] = 0;
		}
	}
}

static void DeleteLaterFrameCb( void * tag, void * opaquev )
{
	//This gets called every frame.
	int delete_this_buffer = ( deletelaterpos - 1 + FREE_LATER_LAG ) % FREE_LATER_LAG;
	int i;
	void ** dlb = deletelaterbuffers[delete_this_buffer];
	if( dlb )
	{
		int len = stb__sbn( dlb );
		for( i = 0; i < len; i++ )
		{
			free( dlb[i] );
		}
		stb__sbn( dlb ) = 0;
	}
	deletelaterpos = ( deletelaterpos + 1 ) % FREE_LATER_LAG;
}

void CNOVRInternalSetupFreeLaterSet()
{
	CNOVRListAdd( cnovrLUpdate, 0, DeleteLaterFrameCb );
	int i;
	for( i = 0; i < FREE_LATER_LAG; i++ )
	{
		deletelaterbuffers[i] = 0;
	}
}

////////////////////////////////////////////////////////////////////////

static og_tls_t casthreadmalloc;

void * CNOVRThreadMalloc( int size )
{
	if( !casthreadmalloc ) casthreadmalloc = OGCreateTLS();
	cnptrset * set = OGGetTLS( casthreadmalloc );
	if( !set ) OGSetTLS( casthreadmalloc, set = cnrbtree_rbset_trbset_null_t_create() );
	void * ret = malloc( size );
	cnptrset_insert( set, ret );
	return ret;
}

void * CNOVRThreadRealloc( void * initial, int size )
{
	void * reret = realloc( initial, size );
	if( reret == initial ) return reret;

	if( !casthreadmalloc ) casthreadmalloc = OGCreateTLS();
	cnptrset * set = OGGetTLS( casthreadmalloc );
	if( !set ) OGSetTLS( casthreadmalloc, set = cnrbtree_rbset_trbset_null_t_create() );
	cnptrset_remove( set, initial );
	cnptrset_insert( set, reret );
	return reret;
}

void CNOVRThreadFree( void * tofree )
{
	if( !casthreadmalloc ) casthreadmalloc = OGCreateTLS();
	cnptrset * set = OGGetTLS( casthreadmalloc );
	if( !set ) OGSetTLS( casthreadmalloc, set = cnrbtree_rbset_trbset_null_t_create() );
	cnptrset_remove( set, tofree );
	//XXX TODO: Should we make sure that we actually deleted?
	CNOVRFreeLater( tofree );
}

void InternalThreadMallocClose()
{
	if( !casthreadmalloc ) return;
	cnptrset * set = OGGetTLS( casthreadmalloc );
	if( !set ) return;
	void * i;
	cnptrset_foreach( set, i )
	{
		CNOVRFreeLater( i );
	}
	cnptrset_destroy( set );
	OGSetTLS( casthreadmalloc, 0 ); //Just in case.
	CNOVRInternalClosePrintfBuffer();
}


//0/6: RED
//1/6: YELLOW
//2/6: GREEN
//3/6: CYAN
//4/6: BLUE
//5/6: PURPLE
//6/6: RED
uint32_t CNOVRHSVtoHEX( float hue, float sat, float value )
{

	float pr = 0.0;
	float pg = 0.0;
	float pb = 0.0;

	short ora = 0.0;
	short og = 0.0;
	short ob = 0.0;

	float ro = fmod( hue * 6.0, 6.0 );

	float avg = 0.0;

	ro = fmod( ro + 6.0 + 1.0, 6.0 ); //Hue was 60* off...

	if( ro < 1.0 ) //yellow->red
	{
		pr = 1.0;
		pg = 1.0 - ro;
	} else if( ro < 2.0 )
	{
		pr = 1.0;
		pb = ro - 1.0;
	} else if( ro < 3.0 )
	{
		pr = 3.0 - ro;
		pb = 1.0;
	} else if( ro < 4.0 )
	{
		pb = 1.0;
		pg = ro - 3.0;
	} else if( ro < 5.0 )
	{
		pb = 5.0 - ro;
		pg = 1.0;
	} else
	{
		pg = 1.0;
		pr = ro - 5.0;
	}

	//Actually, above math is backwards, oops!
	pr *= value;
	pg *= value;
	pb *= value;

	avg += pr;
	avg += pg;
	avg += pb;

	pr = pr * sat + avg * (1.0-sat);
	pg = pg * sat + avg * (1.0-sat);
	pb = pb * sat + avg * (1.0-sat);

	ora = pr * 255;
	og = pb * 255;
	ob = pg * 255;

	if( ora < 0 ) ora = 0;
	if( ora > 255 ) ora = 255;
	if( og < 0 ) og = 0;
	if( og > 255 ) og = 255;
	if( ob < 0 ) ob = 0;
	if( ob > 255 ) ob = 255;

	return (ob<<16) | (og<<8) | ora;
}



