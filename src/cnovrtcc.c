#include "cnovrtcc.h"
#include <jsmn.h>
#include "../lib/tinycc/libtcc.h"
#include <cnovrtccinterface.h>
#include <cnovrutil.h>
#include <os_generic.h>
#include <stdint.h>
#include "../cntools/tccengine/tcccrash.h"
#include <cnovr.h>
#include <string.h>

void InternalPopulateTCC( TCCState * tcc );

//TCC Makes use of a lot of global variables which are not TLS.
//We have to lock around any use of the compiler, itself.
static og_mutex_t tccmutex;

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
	uint8_t bActive;
	uint8_t bDynamicGen;
	uint8_t bFirst;
	uint8_t bDontCompile;
} TCCInstance;

static void StopTCCInstance( TCCInstance * tcc );

static void ReloadTCCInstance( void * tag, void * opaquev )
{
	int r;
	TCCInstance * tce = (TCCInstance *)tag;
	int retryno = (intptr_t)opaquev;

	OGLockMutex( tccmutex );
	if( tce->bDontCompile )
	{
		OGUnlockMutex( tccmutex );
		return;
	}
	tce->bDontCompile = 1;

	TCCState * backup_state = tce->state;

	tce->state = tcc_new();
	tcc_set_output_type(tce->state, TCC_OUTPUT_MEMORY);

	InternalPopulateTCC( tce->state );

	char * cts;
	tasprintf( &cts, "0x%p", tce );
	tcc_define_symbol( tce->state, "cidval", cts );

	tcc_add_library( tce->state, "m" );
	tcc_add_include_path( tce->state, "include" );
	tcc_add_include_path( tce->state, "." );
	tcc_define_symbol( tce->state, "TCC", (void*)1 );
	tcc_set_options( tce->state, "-nostdlib -rdynamic" );

#ifdef WIN32
	tcc_define_symbol( tce->state, "WIN32", "1" );
	tcc_define_symbol( tce->state, "WINDOWS", "1" );
#endif

	r = tcc_add_file( tce->state, tce->tccfilename );
	if( r )
	{
		CNOVRAlert( 0, 2, "TCC Comple Status: %d\n", r );
		goto failure;
	}

	r = tcc_relocate(tce->state, NULL);
	if (r == -1)
	{
		CNOVRAlert( 0, 2, "TCC Reallocation error." );
		goto failure;
	}

	//At this point, we're committed.

	OGUnlockMutex( tccmutex );
	StopTCCInstance( tce );
	OGLockMutex( tccmutex );

	void * backupimage = tce->image;
	tce->image = malloc(r);
	tcc_relocate( tce->state, tce->image );
	if( backup_state ) tcc_delete( backup_state );
	if( backupimage ) free( backupimage );
	backupimage = 0;

	tcccrash_symtcc( tce->tccfilename, tce->state );

	tce->init =  (tcccbfn)tcc_get_symbol( tce->state, "init" );
	tce->start = (tcccbfn)tcc_get_symbol( tce->state, "start" );
	tce->stop =  (tcccbfn)tcc_get_symbol( tce->state, "stop" );

	if( tce->bFirst && tce->init)
	{
		tce->init( tce->identifier );
		tce->bFirst = 0;
	}

	OGUnlockMutex( tccmutex );

	if( !tce->start )
	{
		CNOVRAlert( 0, 5, "Warning: Cannot find start function in %s.\n", tce->tccfilename );
	}
	else
	{
		tce->start( tce->identifier );
	}

	tce->bDontCompile = 0;

	return;

failure:
	tcc_delete( tce->state );
	tce->state = backup_state;
	OGUnlockMutex( tccmutex );
}

static void StopTCCInstance( TCCInstance * tcc )
{
	tcc->bDontCompile = 1;
	if( tcc->stop )
	{
		tcc->stop( tcc->identifier );
	}
	InternalShutdownTCC( tcc->state );
	tcccrash_deltag( (intptr_t)(void*)tcc->state );
}

TCCInstance * CreateTCCInstance( const char * tccfilename, const char * identifier, int bDynamicGen )
{
	TCCInstance * ret = malloc( sizeof( TCCInstance ) );
	memset( ret, 0, sizeof( TCCInstance ) );
	ret->tccfilename = strdup( tccfilename );
	ret->identifier = strdup( identifier );
	ret->bDynamicGen = bDynamicGen;
	ret->bFirst = 1;
	CNOVRJobTack( cnovrQLow, ReloadTCCInstance, ret, 0, 0 );
	CNOVRFileTimeAddWatch( ret->tccfilename, ReloadTCCInstance, ret, 0 );
	return 0;
}

void DestroyTCCInstance( TCCInstance * tcc )
{
	CNOVRFileTimeRemoveTagged( tcc, 1 );
	StopTCCInstance( tcc );

	OGLockMutex( tccmutex );
	if( tcc->tccfilename ) free( tcc->tccfilename );
	if( tcc->state ) tcc_delete( tcc->state );
	free( tcc );
	OGUnlockMutex( tccmutex );
}

//Ok, what are we thinking?

typedef struct TCCSystem_t
{
	//?!?!!????
	TCCInstance * instances;
	int num_engines;
} TCCSystem;

TCCSystem cnovrtccsystem;

void CNOVRStartTCCSystem( const char * tccsuitefile )
{
	//Expect a JSON file for the format
}

void CNOVRStopTCCSystem()
{
	
}


