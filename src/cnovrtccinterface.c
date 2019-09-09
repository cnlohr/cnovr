#include <cnovrtccinterface.h>
#include <cnovrutil.h>
#include <libtcc.h>
#include <stdarg.h>
#include <cnovr.h>
#include <stdio.h>
#include <cnhash.h>
#include <stretchy_buffer.h>
#include <cnovrparts.h>
#include <string.h>

og_tls_t tcctlstag;
og_mutex_t tccinterfacemutex;
#define TCCTag( v ) OGSetTLS( tcctlstag, v )

#define CNHASH_POINTERS 0, cnhash_ptrhf, cnhash_ptrcf, 0

typedef struct object_cleanup_t
{
	void ** mallocedram;
	cnovr_base ** tccobjects;
} object_cleanup;

cnhashtable * objects_to_delete;

void InternalSetupTCCInterface()
{
	tccinterfacemutex = OGCreateMutex();
	tcctlstag = OGCreateTLS();
	objects_to_delete = CNHashGenerate( 0, 0, CNHASH_POINTERS);
}

void InternalShutdownTCC( TCCState * tcc )
{
	OGLockMutex( tccinterfacemutex );

	CNOVRJobCancelAllTag( tcc, 1 );
	CNOVRListDeleteTag( tcc );

	object_cleanup * o = CNHashGetValue( objects_to_delete, tcc );
	if( o )
	{
		int i;
		int count;

		count = sb_count( o->mallocedram );
		for( i = 0; i < count; i++ )
		{
			free( o->mallocedram[i] );
		}
		sb_free( o->mallocedram );

		count = sb_count( o->tccobjects );
		for( i = 0; i < count; i++ )
		{
			CNOVRDelete( o->tccobjects[i] );
		}
		sb_free( o->tccobjects );
		free( o );
		CNHashDelete( objects_to_delete, tcc );
	}
	OGUnlockMutex( tccinterfacemutex );
}


///////////////////////////////////////////////////////////////////////////










static int tccprintf( const char * format, ... )
{
	va_list args;
	va_start(args, format);
	int n = CNOVRAlertv( TCCGetTag(), 2, format, args );
	va_end( args );
	return n;
}

void InternalPopulateTCC( TCCState * tcc )
{
	OGLockMutex( tccinterfacemutex );

	object_cleanup * o = malloc( sizeof( object_cleanup ) );
	memset( o, 0, sizeof( *o ) );
	CNHashInsert( objects_to_delete, tcc, o );

	tcc_add_symbol( tcc, "printf", tccprintf );

	OGUnlockMutex( tccinterfacemutex );
}

