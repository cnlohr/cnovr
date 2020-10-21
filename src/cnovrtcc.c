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
	TCCInstance * tce = (TCCInstance *)opaquev;
//	printf( "Reloading: %p %p\n", tag, tce );
	printf( "Reloading: %s [%p %p]\n", tce->tccfilename, tag, tce );

	double StartRecompileTime = OGGetAbsoluteTime();

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

	//In case we're a subproject.
	tcc_add_include_path( tce->state, "cnovr/include" );
	tcc_add_include_path( tce->state, "cnovr/lib/tinycc/include" );
	tcc_add_include_path( tce->state, "cnovr/lib/systemheaders" );
	tcc_add_include_path( tce->state, "cnovr/rawdraw" );
	tcc_add_include_path( tce->state, "cnovr/openvr/headers" );
	tcc_add_include_path( tce->state, "cnovr" );
	tcc_add_include_path( tce->state, "cnovr/cntools/vlinterm" );

	tcc_add_include_path( tce->state, "include" );
	tcc_add_include_path( tce->state, "cntools/vlinterm" );
	tcc_add_include_path( tce->state, "lib/tinycc/include" );
	tcc_add_include_path( tce->state, "lib/systemheaders" );
	tcc_add_include_path( tce->state, "rawdraw" );
	tcc_add_include_path( tce->state, "openvr/headers" );
	tcc_add_include_path( tce->state, "." );


#ifdef __aarch64__
	tcc_add_include_path( tce->state, "/usr/include/aarch64-linux-gnu" );
#endif

#if defined(WINDOWS) || defined( WIN32 ) || defined( WIN64 )
//	tcc_add_include_path( tce->state, "C:/tcc/include/winapi" );
//	tcc_add_include_path( tce->state, "C:/tcc/include" );
	tcc_add_include_path( tce->state, "lib/tinycc/win32/include" );
	tcc_add_include_path( tce->state, "lib/tinycc/win32/include/winapi" );
	tcc_add_include_path( tce->state, "cnovr/lib/tinycc/win32/include" );
	tcc_add_include_path( tce->state, "cnovr/lib/tinycc/win32/include/winapi" );
	tcc_define_symbol( tce->state, "_MATH_H_", "1" );
	tcc_define_symbol( tce->state, " __STDC_VERSION__", "199901L" ); //Ugh... Long story.
#ifdef WIN32
	tcc_define_symbol( tce->state, "WIN32", "1" );
#endif
#ifdef WIN64
	tcc_define_symbol( tce->state, "WIN64", "1" );
#endif
	tcc_define_symbol( tce->state, "WINDOWS", "1" );
	tcc_define_symbol( tce->state, "chew_FUN_EXPORT", "extern __declspec(dllimport)" );
#else
	tcc_define_symbol( tce->state, "LINUX", "1" );	
#endif
	tcc_define_symbol( tce->state, "OSG_NOSTATIC", "1" );
	tcc_define_symbol( tce->state, "TCC", "1" );

	tcc_set_options( tce->state, "-nostdlib -rdynamic" );

	tcc_define_symbol( tce->state, "TCCINSTANCE", "1" );
	tcc_define_symbol( tce->state, "OSG_NO_IMPLEMENTATION", "1" );
	printf( "Adding: %s\n", tce->tccfilename );
	r = tcc_add_file( tce->state, tce->tccfilename );
	printf( "Add Done: %d\n", r );
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

	void * backupimage = tce->image;
	tce->image = malloc(r);
	tcc_relocate( tce->state, tce->image );
	tcccrash_symtcc( tce->tccfilename, tce->state );

	tce->init =  (tcccbfn)tcc_get_symbol( tce->state, "init" );
	if( !tce->init && tce->basefilename )
		tce->init = (tcccbfn)tcc_get_symbol( tce->state, trprintf( "%sinit", tce->basefilename ) );
	tce->start = (tcccbfn)tcc_get_symbol( tce->state, "start" );
	if( !tce->start && tce->basefilename )
		tce->start = (tcccbfn)tcc_get_symbol( tce->state, trprintf( "%sstart", tce->basefilename ) );

	double EndRecompileTime = OGGetAbsoluteTime();
	printf( "INIT / START: %p %p (Took %4.3f ms)\n", tce->init, tce->start, (EndRecompileTime - StartRecompileTime)*1000. );

	OGUnlockMutex( tccmutex );

	if( tce->bFirst && tce->init)
	{
		InternalInterfaceCreationDone( tce );
		//printf( "%p->%s %p\n", tce->init, tce->identifier, tce->identifier );
		TCCInvocation( tce, tce->init( tce->identifier ) );
		tce->bFirst = 0;
	}
	else
	{
		StopTCCInstance( tce );
		InternalInterfaceCreationDone( tce );
	}

	tce->stop =  (tcccbfn)tcc_get_symbol( tce->state, "stop" );
	if( !tce->stop && tce->basefilename ) 
		tce->stop =  (tcccbfn)tcc_get_symbol( tce->state, trprintf( "%sstop", tce->basefilename ) );

	if( backup_state )
	{
		tcccrash_deltag( (intptr_t)backup_state );
		tcc_delete( backup_state );
	}
	if( backupimage ) CNOVRFreeLater( backupimage ); //In case there are any hanging references.
	backupimage = 0;

	tce->bClosing = 0;

	if( !tce->start )
	{
		CNOVRAlert( 0, 5, "Warning: Cannot find start function in %s.\n", tce->tccfilename );
	}
	else
	{
		printf( "Invoke %p\n", tce );
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
	printf( "Instance stopped\n" );
//	tcccrash_deltag( (intptr_t)(void*)tcc->state );
}
#if 0
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
#endif

//Expects pre-dupped 
TCCInstance * CreateOrRefreshTCCInstance( TCCInstance * tccold, char * tccfilename, char ** additionalfiles, char * identifier, cnstrstrmap * otherproperties, int bDynamicGen )
{
	TCCInstance * ret = malloc( sizeof( TCCInstance ) );
	memset( ret, 0, sizeof( TCCInstance ) );
	ret->tccfilename = tccfilename;
	ret->identifier = identifier;
	ret->otherproperties = otherproperties;
	ret->additionalfiles = additionalfiles;
	ret->bDynamicGen = bDynamicGen;
	ret->bFirst = 1;
	ret->bClosing = 0;

	ret->basefilename = strdup( CNOVRGetBaseFileName( tccfilename ) );

	//COMPLEX!!! We don't use the tag here, otherwise, the TCC system will try to close this event if it's trying to close out causing a deadlock.
	CNOVRFileTimeAddWatch( ret->tccfilename, ReloadTCCInstance, 0, ret );
	int count = sb_count( additionalfiles);
	if( additionalfiles )
	{
		int i;
		for (i = 0 ; i < count; i++ )
		{
			CNOVRFileTimeAddWatch( additionalfiles[i], ReloadTCCInstance, 0, ret );
		}
	}

	CNOVRJobTack( cnovrQAsync, ReloadTCCInstance, 0, ret, 0 );
	return ret;
}

void TCCInstanceDestroy( TCCInstance * tcc )
{
	if( !tccmutex ) tccmutex = OGCreateMutex();
	CNOVRFileTimeRemoveTagged( tcc, 1 );
	StopTCCInstance( tcc );

	OGLockMutex( tccmutex );
	if( tcc->tccfilename ) free( tcc->tccfilename );
	if( tcc->image ) { CNOVRFreeLater( tcc->image ); }
	if( tcc->identifier ) { free( tcc->identifier );  }
	if( tcc->otherproperties ) { cnstrstrmap_destroy( tcc->otherproperties ); }
	if( tcc->basefilename ) { free( tcc->basefilename ); }
	//if( tcc->additionalfiles ) { sb_free( tcc->additionalfiles );  }	//????
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
	//printf( "TODO LOG: %s\n", tolog );
	//XXX TODO
}

void CNOVRTCCSystemFileChange( void * filename, void * opaquev )
{
	char tmporig[CNOVR_MAX_PATH];

	const char * tccsuitefile = cnovrtccsystem.suitefile;

	int filelen;
	char * filestr = CNOVRFileToString( tccsuitefile, &filelen );

	if( filestr == 0 )
	{
		ovrprintf( "Error opening file %s.  Returned null.\n", tccsuitefile );
		return;
	}

	ovrprintf( "File change event (%s)\n", cnovrtccsystem.suitefile );

	CNOVRStopTCCSystem();
	CNOVRFileTimeAddWatch( tccsuitefile, CNOVRTCCSystemFileChange, &cnovrtccsystem, 0 );

	jsmn_parser jp;
	jsmntok_t tokens[2048];
	jsmn_init( &jp );

	int l = jsmn_parse( &jp, filestr, filelen, tokens, sizeof( tokens ) / sizeof( tokens[0] ) );

	int i = 0;
	jsmntok_t * t = 0;

	while( i < l )
	{
		t = tokens + i++;
		if( t->type != JSMN_OBJECT ) goto failout;
		t = tokens + i++;
		while( i < l )
		{
			if( t->type != JSMN_STRING ) goto failout;
			//printf( "%s\n", jsmnstrtr( filestr, t->start, t->end ) );
			if( strncmp( filestr + t->start, "searchfolders", t->end - t->start ) == 0 )
			{
				t = tokens + i++;
				if( t->type != JSMN_ARRAY ) goto failout;
				int arraylen = t->size;
				int j;
				for( j = 0; j < arraylen; j++ )
				{
					t = tokens + i++;
					jsmnstrsn( tmporig, CNOVR_MAX_PATH, filestr, t->start, t->end );
					CNOVRFileSearchAddPath( tmporig );
					sb_push( cnovrtccsystem.searchfolders, strdup( tmporig ) );
				}
			}
			else if( strncmp( filestr + t->start, "cfiles", t->end - t->start ) == 0 )
			{
				t = tokens + i++;
				if( t->type != JSMN_ARRAY ) goto failout;

				int arraylen = t->size;
				t = tokens + i++;
				int j;
				for( j = 0; j < arraylen; j++ )
				{
					char * cfile;
					char * identifier;
					cnstrstrmap * otherproperties;
					int disabled = 0;
					char ** additionalfiles = 0;
					cfile = 0;
					otherproperties = 0;
					identifier = 0;
					while( i < l )
					{
						t = tokens + i++;
                        //char checktag[32];
                        //memcpy( checktag, filestr + t->start, 31 );
                        //checktag[31] = 0;
                        //printf( "CHECKTAG: %s\n", checktag );
						if( t->type == JSMN_STRING && strncmp( filestr + t->start, "cfile", t->end - t->start ) == 0 )
						{
							t = tokens + i++;
							if( t->type == JSMN_STRING && cfile == 0 )
							{
								jsmnstrsn( tmporig, CNOVR_MAX_PATH, filestr, t->start, t->end );
								char * tmp = CNOVRFileSearch( tmporig );
								if( !tmp )
								{
									ovrprintf( "Can't find file: %s\n", tmporig );
								}
								else
								{
									cfile = strdup( tmp );
								}
							}
							else goto failout;
						}
						else if( t->type == JSMN_STRING && strncmp( filestr + t->start, "disabled", t->end - t->start ) == 0 )
						{
							t = tokens + i++;
							if( t->type == JSMN_PRIMITIVE )
							{
								//char * tmp = jsmnstrtr( filestr, t->start, t->end );
								//printf( "~~~~~~~~~~~~TMP: %s\n", tmp );
								disabled = jsmnintparse( filestr, t->start, t->end );
							}
							else goto failout;
						}
						else if( t->type == JSMN_STRING && strncmp( filestr + t->start, "identifier", t->end - t->start ) == 0 )
						{
							t = tokens + i++;
							if( t->type == JSMN_STRING && identifier == 0 )
							{
								jsmnstrsn( tmporig, CNOVR_MAX_PATH, filestr, t->start, t->end );
								identifier = strdup( tmporig );
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
								jsmnstrsn( tmporig, CNOVR_MAX_PATH, filestr, t->start, t->end );
								char * tmp = CNOVRFileSearch( tmporig );
								if( !tmp )
								{
									ovrprintf( "Can't find file: %s", tmporig ); 
								}
								else
								{
									ovrprintf( "Additional file: %s\n", tmp );
									sb_push( additionalfiles, strdup( tmp ) );
								}
							}
						}
						else if( t->type == JSMN_STRING )
						{
							int indexlen = t->end-t->start;
							char oiindex[indexlen+1];
							memcpy( oiindex, filestr + t->start, indexlen );
							oiindex[indexlen] = 0;
							//Other parameters go into  	cnstrstrmap * otherproperties;
							t = tokens + i++;
							if( t->type == JSMN_STRING || t->type == JSMN_PRIMITIVE ) //TODO: Consider removing this check.
							{
								jsmnstrsn( tmporig, CNOVR_MAX_PATH, filestr, t->start, t->end );
								ovrprintf( "Assigning tag: %s = %s\n", oiindex, tmporig );
								if( !otherproperties ) otherproperties = cnstrstrmap_create();
								char ** v = &(cnstrstrmap_insert( otherproperties, oiindex )->data);
								if( *v ) free( *v );
								*v = strdup( tmporig );
							}
							else goto failout;
						}
						else //End of 
						{
							break;
						}
					}

					if( disabled ) continue;

					if( !cfile || !identifier )
					{
						ovrprintf( "Invalid JSON: Either cfile or identifier invalid. You need both.\n" );
						goto failout;
					}

					TCCInterfaceAddCFileInstance( cfile, additionalfiles, identifier, otherproperties );
				}
			}
			t = tokens + i++;

		}
	}
failout:
	if( i < l-1 ) //Ignore last token.
	{
		if( t )
		{
			jsmnstrsn( tmporig, CNOVR_MAX_PATH, filestr, t->start, t->end );
			ovrprintf( "Error parsing JSON around %s [%d != %d]\n", tmporig, i, l );
		}
		else
			ovrprintf( "Couldn't begin JSON parsing\n" );
	}
	free( filestr );
}


TCCInstance * TCCInterfaceAddCFileInstance( char * cfile, char ** additionalfiles, char * identifier, cnstrstrmap * otherproperties )
{
	if( !cfile ) return 0;

	TCCInstance * ret = CreateOrRefreshTCCInstance( 0, cfile, additionalfiles, identifier, otherproperties, 0 );

	if( !ret ) return 0;

	sb_push( cnovrtccsystem.instances, ret );

	if( additionalfiles )
	{
		int i;
		for( i = 0; i < sb_count( additionalfiles ); i++ )
		{
			free( additionalfiles[i] );
		}
		sb_free( additionalfiles );
	}
	additionalfiles = 0;

	//Identifier and cfile are consumed by CreateOrRefreshTCCInstance
	return ret;
}

void CNOVRStartTCCSystem( const char * tccsuitefile )
{
	CNOVRStopTCCSystem();
	if( cnovrtccsystem.suitefile ) free( cnovrtccsystem.suitefile );
	
	char * gfile = CNOVRFileSearch( tccsuitefile );

	printf( "COMP FILE %s/%s\n", gfile, tccsuitefile );

	//If we can't find the file, try prefixing with projects/
	if( !gfile || CNOVRFileGetLength( gfile ) <= 0 )
	{
		char cts[1024];
		sprintf( cts, "projects/%s/%s.json", tccsuitefile, tccsuitefile );
		printf( "Project not found.  Maybe it was a projec name?  Trying: %s\n", cts );
		gfile = CNOVRFileSearch( cts );
	}

	if( !gfile || CNOVRFileGetLength( gfile ) <= 0 )
	{
		char cts[1024];
		sprintf( cts, "modules/%s/%s.json", tccsuitefile, tccsuitefile );
		printf( "Project not found.  Maybe it was a module name?  Trying: %s\n", cts );
		gfile = CNOVRFileSearch( cts );
	}

	if( !gfile || CNOVRFileGetLength( gfile ) <= 0 )
	{
		ovrprintf( "Error: Could not find project by name of %s\n", tccsuitefile );
		return;
	}
	
	cnovrtccsystem.suitefile = strdup( gfile );
	ovrprintf( "CNOVRStartTCCSystem( %s %p )\n", cnovrtccsystem.suitefile, CNOVRTCCSystemFileChange );
	CNOVRFileTimeAddWatch( cnovrtccsystem.suitefile, CNOVRTCCSystemFileChange, &cnovrtccsystem, 0 );
	CNOVRJobTack( cnovrQAsync, CNOVRTCCSystemFileChange, &cnovrtccsystem, 0, 0 );
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
				TCCInstanceDestroy( cnovrtccsystem.instances[i] );
		}
	
		sb_free( cnovrtccsystem.instances );
		cnovrtccsystem.instances = 0;
	}

	if( cnovrtccsystem.searchfolders )
	{
		int count = sb_count( cnovrtccsystem.searchfolders );
		for( i = 0; i < count; i++ )
		{
			CNOVRFileSearchRemovePath( cnovrtccsystem.searchfolders[i] );
			free( cnovrtccsystem.searchfolders[i] );
		}
		sb_free( cnovrtccsystem.searchfolders );
		cnovrtccsystem.searchfolders = 0;
	}
}

