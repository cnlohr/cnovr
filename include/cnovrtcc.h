#ifndef _CNOVRTCC_H
#define _CNOVRTCC_H

#ifdef SINGLEBUILD
#define GetTCCTag() 0
#endif

#ifdef TCCINSTANCE


//You should implement these
void init( const char * id );	//called only once
void start( const char * id );	//called on start and after reload
void stop( const char * id );	//called on reload and stop

//Alternatively, you can prefix the init/start/stop with your module's name, i.e. ovrballstart

//These are provided.
int printf( const char * format, ... );

//Math functions
double floor( double f );
double sin( double f );
double cos( double f );
float floorf(float x);
double acos( double f );
double atan( double f );
double tan( double f );

void CNFGSwapBuffers();

#ifdef WINDOWS
#include <stdio.h>
#endif

//from os_generic
typedef void* og_thread_t;
typedef void* og_mutex_t;
typedef void* og_sema_t;
typedef void* og_tls_t;


void OGSleep( int is );
void OGUSleep( int ius );
double OGGetAbsoluteTime();
double OGGetFileTime( const char * file );
og_thread_t OGCreateThread( void * (routine)( void * ), void * parameter );
void * OGJoinThread( og_thread_t ot );
void OGCancelThread( og_thread_t ot );
og_mutex_t OGCreateMutex();
void OGLockMutex( og_mutex_t om );
void OGUnlockMutex( og_mutex_t om );
void OGDeleteMutex( og_mutex_t om );
og_sema_t OGCreateSema();
int OGGetSema( og_sema_t os );
void OGLockSema( og_sema_t os );
void OGUnlockSema( og_sema_t os );
void OGDeleteSema( og_sema_t os );
og_tls_t OGCreateTLS();
void OGDeleteTLS( og_tls_t key );
void * OGGetTLS( og_tls_t key );
void OGSetTLS( og_tls_t key, void * data );


//Force a pointer to be disconnected from the TCC deletion tree.
void RemoveTCCDeleteTag( void * v );

void * GetTCCTag();

//Actually a cnstrstrmap * but, we don't cast it here to make it so users don't need cnhash.
void * GetOtherTCCProperties(); 

#else

#include <stdio.h>
#include <stdint.h>
#include <cnrbtree.h>

struct TCCState;
typedef struct TCCState TCCState;
typedef void (*tcccbfn)( const char * id );

typedef struct TCCInstance_t
{
	tcccbfn init;
	tcccbfn start;
	tcccbfn stop;
	TCCState * state;
	char * tccfilename;
	void * image;
	char * identifier;
	char * basefilename; //i.e. 'ovrball' 
	char ** additionalfiles; //sb_buffer.
	cnstrstrmap * otherproperties;
	uint8_t bActive;
	uint8_t bDynamicGen;
	uint8_t bFirst;
	uint8_t bDontCompile;
	uint8_t bClosing;
} TCCInstance;



void CNOVRTCCLog( void * data, const char * tolog );
void CNOVRStartTCCSystem( const char * systemname );

//Note: This "consumes" cfile, identifier and additionalfiles.  It will free them.
TCCInstance * TCCInterfaceAddCFileInstance( char * cfile, char ** additionalfiles,
	char * identifier, cnstrstrmap * otherproperties );

void TCCInstanceDestroy( TCCInstance * tcc );


#endif

#endif

