#include <cnovrtccinterface.h>
#include <cnovrutil.h>
#include <tinycc/libtcc.h>
#include <stdarg.h>
#include <cnovr.h>
#include <stdio.h>
#include <cnhash.h>
#include <cnovrparts.h>
#include <string.h>
#include <cnrbtree.h>

////////////////////////////////////////////////////////////////////////////////

og_tls_t ogsafelocktls;

CNRBTREETEMPLATE( og_mutex_t, int, RBptrcmp, RBptrcpy, RBnullop );

void OGTSLockMutex( og_mutex_t m )
{
	cnrbtree_og_mutex_tint * tree = (cnrbtree_og_mutex_tint *)OGGetTLS( ogsafelocktls );
	if( !tree ) return;
	RBA( tree, m )++;
}

void OGTSUnlockMutex( og_mutex_t m )
{
	cnrbtree_og_mutex_tint * tree = (cnrbtree_og_mutex_tint *)OGGetTLS( ogsafelocktls );
	if( !tree ) return;
	RBA( tree, m )--;
}

void OGResetSafeMutices()
{
	cnrbtree_og_mutex_tint * tree;
	if( !ogsafelocktls )
	{
		ogsafelocktls = OGCreateTLS();
	}
	tree = (cnrbtree_og_mutex_tint *)OGGetTLS( ogsafelocktls );
	if( !tree )
	{
		tree = cnrbtree_og_mutex_tint_create();
	}
}

void OGUnlockSafeMutices()
{
	cnrbtree_og_mutex_tint * tree = (cnrbtree_og_mutex_tint *)OGGetTLS( ogsafelocktls );
	RBFOREACH( og_mutex_tint, tree, i )
	{
		if( i->data < 0 )
		{
			fprintf( stderr, "Serious fault: Safe lock has been unlocked more times than it should have.\n" );
			continue;
		}
		while( i->data )
		{
			OGUnlockMutex( i->key );
			i->data--;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

og_tls_t tcctlstag;
og_mutex_t tccinterfacemutex;
#define TCCTag( v ) OGSetTLS( tcctlstag, v )

#define CNHASH_POINTERS 0, cnhash_ptrhf, cnhash_ptrcf, 0

typedef struct object_cleanup_t
{
	cnptrset * mallocedram;
	cnptrset * tccobjects;
	cnptrset * mutices;
	cnptrset * threads;
	cnptrset * semaphores;
	cnptrset * tlses;
} object_cleanup;

cnhashtable * objects_to_delete;

void InternalSetupTCCInterface()
{
	tccinterfacemutex = OGCreateMutex();
	printf( "CRETE\n" );
	tcctlstag = OGCreateTLS();
	objects_to_delete = CNHashGenerate( 0, 0, CNHASH_POINTERS);
}


void InternalShutdownTCC( TCCInstance * tce )
{
	OGLockMutex( tccinterfacemutex );
	CNOVRJobCancelAllTag( tce, 1 ); //XXX TODO XXX We need a way of cancelling the currently running operation so we CAN block.
	CNOVRListDeleteTag( tce );
	object_cleanup * o = CNHashGetValue( objects_to_delete, tce );
	printf( "Cleanup: %p\n", o );
	if( o )
	{
		void * i;
		cnptrset_foreach( o->threads, i )
		{
			printf( "Cancelling thread: %p in %p\n", i, o->threads );
			OGCancelThread( (og_thread_t)i );
		}
		cnptrset_destroy( o->threads );
		cnptrset_foreach( o->tccobjects, i ) CNOVRDeleteBase( ((cnovr_base*)i) );
		cnptrset_destroy( o->tccobjects );
		cnptrset_foreach( o->mutices, i ) OGDeleteMutex( (og_mutex_t)i );
		cnptrset_destroy( o->mutices );
		cnptrset_foreach( o->tlses, i ) OGDeleteTLS( (og_tls_t)i );
		cnptrset_destroy( o->tlses );
		cnptrset_foreach( o->semaphores, i ) OGDeleteSema( (og_sema_t)i );
		cnptrset_destroy( o->semaphores );
		cnptrset_foreach( o->mallocedram, i ) free( i );
		cnptrset_destroy( o->mallocedram );
		free( o );
		printf( "ERASING {%p}\n", tce );
		CNHashDelete( objects_to_delete, tce );
	}
	OGUnlockMutex( tccinterfacemutex );
}


///////////////////////////////////////////////////////////////////////////

struct cnovr_internal_thread_starter_t
{
	void * (*routine)( void * );
	void * parameter;
	void * tag;
};

static void * cnovr_internal_thread_starter( void * v )
{
	struct cnovr_internal_thread_starter_t tp;
	memcpy( &tp, v, sizeof( tp ) );
	free( v );

	TCCInvocation( tp.tag, 
	{
		if( tcccrash_checkpoint() )
		{
			printf( "Warning: thread failed.\n" );
			//Don't worry threads will be cleaned up later.
			return 0;
		}
		else
		{
			return tp.routine( tp.parameter );
		}
	} )
	return 0;
}

og_thread_t TCCOGCreateThread( void * (routine)( void * ), void * parameter )
{
	struct cnovr_internal_thread_starter_t * m = malloc( sizeof( struct cnovr_internal_thread_starter_t ) );
	m->tag = TCCGetTag();
	m->parameter = parameter;
	m->routine = routine;

	og_thread_t ret = OGCreateThread( cnovr_internal_thread_starter, m );
	OGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag() );
	if( c ) cnptrset_insert( c->threads, ret );
	OGUnlockMutex( tccinterfacemutex );
	return ret;
}

void * TCCOGJoinThread( og_thread_t ot )
{
	og_thread_t ret = OGJoinThread( ot );
	OGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_remove( c->threads, ot );
	OGUnlockMutex( tccinterfacemutex );
	return ret;
}

void TCCOGCancelThread( og_thread_t ot )
{
	OGLockMutex( tccinterfacemutex );
	OGCancelThread( ot );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_remove( c->threads, ot );
	OGUnlockMutex( tccinterfacemutex );
}

og_mutex_t TCCOGCreateMutex()
{
	og_mutex_t ret = OGCreateMutex();
	OGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_insert( c->mutices, ret );
	OGUnlockMutex( tccinterfacemutex );
	return ret;
}

void TCCOGDeleteMutex( og_mutex_t om )
{
	OGDeleteMutex( om );
	OGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_remove( c->mutices, om );
	OGUnlockMutex( tccinterfacemutex );
}

og_sema_t TCCOGCreateSema()
{
	og_sema_t ret = OGCreateSema();
	OGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_insert( c->semaphores, ret );
	OGUnlockMutex( tccinterfacemutex );
	return ret;
}

void TCCOGDeleteSema( og_sema_t os )
{
	OGDeleteSema( os );
	OGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_remove( c->semaphores, os );
	OGUnlockMutex( tccinterfacemutex );
}

og_tls_t TCCOGCreateTLS()
{
	og_sema_t ret = OGCreateTLS();
	OGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_insert( c->tlses, ret );
	OGUnlockMutex( tccinterfacemutex );
	return ret;
}

void TCCOGDeleteTLS( og_tls_t key )
{
	OGDeleteTLS( key );
	OGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_remove( c->tlses, key );
	OGUnlockMutex( tccinterfacemutex );
}

static int TCCprintf( const char * format, ... )
{
	va_list args;
	va_start(args, format);
	int n = CNOVRAlertv( TCCGetTag(), 2, format, args );
	va_end( args );
	return n;
}

#define TCCExport( x ) tcc_add_symbol( tce->state, #x, &TCC##x );
#define TCCExportS( x ) tcc_add_symbol( tce->state, #x, &x );

void InternalPopulateTCC( TCCInstance * tce )
{
	printf( "InternalPopulateTCC { %p }\n", tce );
	OGLockMutex( tccinterfacemutex );

	TCCExport( printf );
	TCCExportS( OGSleep );
	TCCExportS( OGUSleep );
	TCCExportS( OGGetAbsoluteTime );
	TCCExportS( OGGetFileTime );
	TCCExport( OGCreateThread );
	TCCExport( OGJoinThread );
	TCCExport( OGCancelThread );
	TCCExport( OGCreateMutex );
	TCCExportS( OGLockMutex );
	TCCExportS( OGUnlockMutex );
	TCCExport( OGDeleteMutex );
	TCCExport( OGCreateSema );
	TCCExportS( OGGetSema );
	TCCExportS( OGLockSema );
	TCCExportS( OGUnlockSema );
	TCCExport( OGDeleteSema );
	TCCExport( OGCreateTLS );
	TCCExport( OGDeleteTLS );
	TCCExportS( OGGetTLS );
	TCCExportS( OGSetTLS );

	OGUnlockMutex( tccinterfacemutex );
}

void InternalInterfaceCreationDone( TCCInstance * tce )
{
	object_cleanup * o = malloc( sizeof( object_cleanup ) );
	memset( o, 0, sizeof( *o ) );
	CNHashInsert( objects_to_delete, tce, o );
	o->mallocedram  = cnptrset_create();
	o->tccobjects = cnptrset_create();
	o->mutices = cnptrset_create();
	o->threads = cnptrset_create();
	o->semaphores = cnptrset_create();
	o->tlses = cnptrset_create();
}

