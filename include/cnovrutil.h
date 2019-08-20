#ifndef _CNOVRUTIL_H
#define _CNOVRUTIL_H

//Not sure if we need this feature.
//void * GetNamedPtr( const char * namedptr, const char * type );
//void * NamedPtr( const char * namedptr, const char * type, int size );

/*
//Make a task manager?  Not sure if we need this, either.
typedef struct cnovrtask_t
{
	void (*taskcb)( void * opaque, int ret );
	int (*task)( void * opaque );
	void * opaque;
	int priority;
} cnovrtask;

int (*taskcb)( void * opaque );
int (*task)( void * opaque );
//int WaitOnTask( 
//int AddTask( void * taskid, 
*/

//Various other tools.

//These must be threadsafe.  Also, need a way to wholesale clear out a class of these guys.
void FileTimeAddWatch( const char * fname, uint8_t * flag, void * tag );
void FileTimeRemoveWatch( const char * fname, uint8_t * flag, void * tag );
void FileTimeRemoveTagged( void * tag );
double FileTimeCached( const char * fname );
char * FileToString( const char * fname, int * length );
char ** SplitStrings( const char * line, char * split, char * white, int merge_fields ); //You can just free(...) the return. it's safe.

void CNOVRInternalStartCacheSystem();
void CNOVRInternalStopCacheSystem();

#endif

