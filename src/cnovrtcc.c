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
#include <stdio.h>
#include <stretchy_buffer.h>

void CNOVRStopTCCSystem();

//TCC Makes use of a lot of global variables which are not TLS.
//We have to lock around any use of the compiler, itself.
static og_mutex_t tccmutex;

static void StopTCCInstance( TCCInstance * tcc );

static void ReloadTCCInstance( void * tag, void * opaquev )
{
	if( !tccmutex ) tccmutex = OGCreateMutex();
	int r;
	TCCInstance * tce = (TCCInstance *)tag;
	int retryno = (intptr_t)opaquev;
	printf( "Reloading: %p %p\n", tag, tce );
	printf( "Reloading;%s\n", tce->tccfilename );
	OGLockMutex( tccmutex );
	if( tce->bDontCompile )
	{
		printf( "Failed; Marked as don't compile.\n" );
		OGUnlockMutex( tccmutex );
		return;
	}
	tce->bDontCompile = 1;

	TCCState * backup_state = tce->state;

	tce->state = tcc_new();
	tcc_set_output_type(tce->state, TCC_OUTPUT_MEMORY);

	InternalPopulateTCC( tce );

	char * cts;
	tasprintf( &cts, "0x%p", tce );
	tcc_define_symbol( tce->state, "cidval", cts );

	tcc_add_library( tce->state, "m" );
	tcc_add_include_path( tce->state, "include" );
	tcc_add_include_path( tce->state, "." );
	tcc_define_symbol( tce->state, "TCC", "1" );
	tcc_set_options( tce->state, "-nostdlib -rdynamic" );

#ifdef WIN32
	tcc_define_symbol( tce->state, "WIN32", "1" );
	tcc_define_symbol( tce->state, "WINDOWS", "1" );
#endif
	printf( "CMidpoint\n" );
	tcc_define_symbol( tce->state, "TCCINSTANCE", "1" );
	printf( "Adding\n" );
	r = tcc_add_file( tce->state, tce->tccfilename );
	if( r )
	{
		CNOVRAlert( 0, 2, "TCC Comple Status: %d\n", r );
		goto failure;
	}

	printf( "Relocating\n" );
	r = tcc_relocate(tce->state, NULL);
	if (r == -1)
	{
		CNOVRAlert( 0, 2, "TCC Reallocation error." );
		goto failure;
	}

	//At this point, we're committed.

	printf( "Finishing up\n" );
	void * backupimage = tce->image;
	tce->image = malloc(r);
	tcc_relocate( tce->state, tce->image );
	printf( "Symming in\n" );
	tcccrash_symtcc( tce->tccfilename, tce->state );

	tce->init =  (tcccbfn)tcc_get_symbol( tce->state, "init" );
	tce->start = (tcccbfn)tcc_get_symbol( tce->state, "start" );

	printf( "Ok... Unlocking\n" );




	OGUnlockMutex( tccmutex );

	if( tce->bFirst && tce->init)
	{
		InternalInterfaceCreationDone( tce );
		TCCInvocation( tce, tce->init( tce->identifier ) );
		tce->bFirst = 0;
	}
	else
	{
		StopTCCInstance( tce );
		InternalInterfaceCreationDone( tce );
	}

	tce->stop =  (tcccbfn)tcc_get_symbol( tce->state, "stop" );

	if( backup_state ) tcc_delete( backup_state );
	if( backupimage ) free( backupimage );
	backupimage = 0;

	if( !tce->start )
	{
		CNOVRAlert( 0, 5, "Warning: Cannot find start function in %s.\n", tce->tccfilename );
	}
	else
	{
		TCCInvocation( tce, tce->start( tce->identifier ) );
	}

	tce->bDontCompile = 0;
	printf( "ReloadingOK %s\n", tce->tccfilename );


	return;

failure:
	tce->bDontCompile = 0;
	tcc_delete( tce->state );
	tce->state = backup_state;
	OGUnlockMutex( tccmutex );
	printf( "ReloadingFAILED %s\n", tce->tccfilename );
}

static void StopTCCInstance( TCCInstance * tcc )
{
	printf( "STOP INSTANCE!\n" );
	if( !tcc ) return;
	tcc->bDontCompile = 1;
	if( tcc->stop )
	{
		TCCInvocation( tcc, tcc->stop( tcc->identifier ) );
	}
	InternalShutdownTCC( tcc );
	tcccrash_deltag( (intptr_t)(void*)tcc->state );
}

static void DestroyTCC( TCCInstance * tcc )
{
	if( !tcc ) return;
	StopTCCInstance( tcc );
	void * v;
	if( tcc->tccfilename ) { free( tcc->tccfilename ); }
	if( tcc->image ) { free( tcc->image ); }
	if( tcc->identifier ) { free( tcc->identifier );  }
	if( tcc->additionalfiles ) { sb_free( tcc->additionalfiles );  }
	if( tcc->state ) { tcc_delete( tcc->state ); }
	free( tcc );
}

//Expects pre-dupped 
TCCInstance * CreateOrRefreshTCCInstance( TCCInstance * tccold, char * tccfilename, char ** additionalfiles, char * identifier, int bDynamicGen )
{
	TCCInstance * ret = malloc( sizeof( TCCInstance ) );
	memset( ret, 0, sizeof( TCCInstance ) );
	ret->tccfilename = tccfilename;
	ret->identifier = identifier;
	ret->additionalfiles = additionalfiles;
	ret->bDynamicGen = bDynamicGen;
	ret->bFirst = 1;
	//CNOVRListAdd( cnovrLFTCheck, ret, ReloadTCCInstance  );
	CNOVRFileTimeAddWatch( ret->tccfilename, ReloadTCCInstance, ret, 0 );
	printf( "!!!!!!!!!!!!! Marking on %s\n", tccfilename );
	return 0;
}

void DestroyTCCInstance( TCCInstance * tcc )
{
	if( !tccmutex ) tccmutex = OGCreateMutex();

	CNOVRFileTimeRemoveTagged( tcc, 1 );
	StopTCCInstance( tcc );

	OGLockMutex( tccmutex );
	if( tcc->tccfilename ) free( tcc->tccfilename );
	if( tcc->state ) tcc_delete( tcc->state );
	free( tcc );
	OGUnlockMutex( tccmutex );
}

///////////////////////////////////////////////////////////////////////////////////

//Ok, what are we thinking?

typedef struct TCCSystem_t
{
	char *  suitefile;
	char ** searchfolders;
	TCCInstance ** instances; //sb_'d
} TCCSystem;

TCCSystem cnovrtccsystem;

//XXX TODO: Consider allowing partial reloads of the .json file.

void CNOVRTCCLog( void * data, const char * tolog )
{
	//XXX TODO
}

static void CNOVRTCCSystemFileChange( void * filename, void * opaquev )
{
	printf( "File change event\n" );
	const char * tccsuitefile = cnovrtccsystem.suitefile;

	CNOVRStopTCCSystem();
	CNOVRFileTimeAddWatch( tccsuitefile, CNOVRTCCSystemFileChange, &cnovrtccsystem, 0 );

	int filelen;
	char * filestr = FileToString( tccsuitefile, &filelen );
	jsmn_parser jp;
	jsmntok_t tokens[1024];
	jsmn_init( &jp );
	int l = jsmn_parse( &jp, filestr, filelen, tokens, sizeof( tokens ) / sizeof( tokens[0] ) );

	int i = 0;
	jsmntok_t * t = 0;
	printf( "CNOVRTCCSystemFileChange\n" );
	while( i < l )
	{
		t = tokens + i++;
		if( t->type != JSMN_OBJECT ) goto failout;
		t = tokens + i++;
		while( i < l )
		{
			if( t->type != JSMN_STRING ) goto failout;
			printf( "%s\n", jsmnstrdup( filestr, t->start, t->end ) );
			if( strncmp( filestr + t->start, "searchfolders", t->end - t->start ) == 0 )
			{
				t = tokens + i++;
				if( t->type != JSMN_ARRAY ) goto failout;
				int arraylen = t->size;
				int j;
				for( j = 0; j < arraylen; j++ )
				{
					t = tokens + i++;
					char * st = jsmnstrdup( filestr, t->start, t->end );
					FileSearchAddPath( st );
					printf( "%p %p\n", cnovrtccsystem.searchfolders, st );
					sb_push( cnovrtccsystem.searchfolders, strdup( st ) );
				}
				printf( "%d %d\n", i, l );
			}
			else if( strncmp( filestr + t->start, "cfiles", t->end - t->start ) == 0 )
			{
				t = tokens + i++;
				if( t->type != JSMN_ARRAY ) goto failout;

				int arraylen = t->size;
				t = tokens + i++;
				int j;
				printf( "CFILES %d\n", arraylen );
				for( j = 0; j < arraylen; j++ )
				{
					char * cfile;
					char * identifier;
					char ** additionalfiles = 0;
					cfile = 0;
					identifier = 0;
					while( i < l )
					{
						t = tokens + i++;
						if( t->type == JSMN_STRING && strncmp( filestr + t->start, "cfile", t->end - t->start ) == 0 )
						{
							t = tokens + i++;
							if( t->type == JSMN_STRING && cfile == 0 )
							{
								char * tmp = jsmnstrdup( filestr, t->start, t->end );
								tmp = FileSearch( tmp );
								if( !tmp ) { printf( "Can't find file: %s\n", jsmnstrdup( filestr, t->start, t->end ) ); }
								cfile = strdup( tmp );
							}
							else goto failout;
						}
						else if( t->type == JSMN_STRING && strncmp( filestr + t->start, "identifier", t->end - t->start ) == 0 )
						{
							t = tokens + i++;
							if( t->type == JSMN_STRING && identifier == 0 )
							{
								char * tmp = jsmnstrdup( filestr, t->start, t->end );
								identifier = strdup( tmp );
							}
							else goto failout;
						}
						else if( t->type == JSMN_STRING && strncmp( filestr + t->start, "additionalfiles", t->end - t->start ) == 0 )
						{
							t = tokens + i++;
							if( t->type != JSMN_ARRAY ) goto failout;
							int nra = t->size;
							int k;
							for( k = 0; k < nra; k++ )	
							{
								t = tokens + i++;
								if( t->type != JSMN_STRING ) goto failout;
								char * tmp = jsmnstrdup( filestr, t->start, t->end );
								tmp = FileSearch( tmp );
								if( !tmp ) { printf( "Can't find file: %s\n", jsmnstrdup( filestr, t->start, t->end ) ); }
								sb_push( additionalfiles, strdup( tmp ) );
							}
						}
						else if( t->type == JSMN_STRING )
						{
							t = tokens + i++;
							if( t->type != JSMN_STRING ) goto failout;
							//Any other data type here...
						}
						else //End of 
						{
							break;
						}
					}

					if( !cfile || !identifier )
					{
						printf( "Invalid JSON: cfile + identifier not available for recent identifier.\n" );
						goto failout;
					}

					printf( "Pushing: %s %s\n", cfile, identifier );
					sb_push( cnovrtccsystem.instances, 
						CreateOrRefreshTCCInstance( 0, cfile, additionalfiles, identifier, 0 ) );

					if( additionalfiles ) sb_free( additionalfiles );
					additionalfiles = 0;
				}
			}
			t = tokens + i++;

		}
	}
failout:
	if( i < l-1 ) //Ignore last token.
	{
		if( t )
			printf( "Error parsing JSON around %s [%d != %d]\n", jsmnstrdup( filestr, t->start, t->end ), i, l );
		else
			printf( "Couldn't begin JSON parsing\n" );
	}
	free( filestr );
}

void CNOVRStartTCCSystem( const char * tccsuitefile )
{
	tcccrash_install();
	CNOVRStopTCCSystem();
	if( cnovrtccsystem.suitefile ) free( cnovrtccsystem.suitefile );
	cnovrtccsystem.suitefile = strdup( tccsuitefile );
	printf( "STARTING (and adding list) %s %p\n", cnovrtccsystem.suitefile, CNOVRTCCSystemFileChange );
	//CNOVRListAdd( cnovrLFTCheck, &cnovrtccsystem, CNOVRTCCSystemFileChange );
	CNOVRFileTimeAddWatch( tccsuitefile, CNOVRTCCSystemFileChange, &cnovrtccsystem, 0 );
}

void CNOVRStopTCCSystem()
{
	int i;
	CNOVRFileTimeRemoveTagged( &cnovrtccsystem, 0 );	
	if( cnovrtccsystem.instances )
	{
		int count = sb_count( cnovrtccsystem.instances );
		for( i = 0; i < count; i++ )
		{
			if( cnovrtccsystem.instances[i] )
				DestroyTCCInstance( cnovrtccsystem.instances[i] );
		}
	
		sb_free( cnovrtccsystem.instances );
		cnovrtccsystem.instances = 0;
	}

	if( cnovrtccsystem.searchfolders )
	{
		int count = sb_count( cnovrtccsystem.searchfolders );
		for( i = 0; i < count; i++ )
		{
			FileSearchRemovePath( cnovrtccsystem.searchfolders[i] );
			free( cnovrtccsystem.searchfolders[i] );
		}
		sb_free( cnovrtccsystem.searchfolders );
		cnovrtccsystem.searchfolders = 0;
	}

}

