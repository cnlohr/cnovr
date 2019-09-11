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

//XXX TODO:
// Consider: What if we've called back into some core system which is currently locked.
//  if we cancel the thread, it will remain locked.
//
//TODO: We need to make this thing a set.


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
	printf( "LOCK\n" );
	printf( "LOCKIN\n" );
	OGLockMutex( tccinterfacemutex );
	CNOVRJobCancelAllTag( tce, 0 );
	//XXX TODO XXX We need a way of cancelling the currently running operation so we CAN block.
	printf( "LOCKINGOINGA\n" );
	CNOVRListDeleteTag( tce );
	printf( "LOCKINGOING\n" );
	object_cleanup * o = CNHashGetValue( objects_to_delete, tce );
	if( o )
	{
		void * i;
		cnptrset_foreach( o->threads, i ) OGCancelThread( (og_thread_t)i );
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
		CNHashDelete( objects_to_delete, tce );
	}
	printf( "LOCKEND\n" );
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
	printf( "++1\n" );
	OGLockMutex( tccinterfacemutex );
	printf( "++1B\n" );

	object_cleanup * o = malloc( sizeof( object_cleanup ) );
	memset( o, 0, sizeof( *o ) );
	CNHashInsert( objects_to_delete, tce, o );

	printf( "++2\n" );
	o->mallocedram  = cnptrset_create();
	o->tccobjects = cnptrset_create();
	o->mutices = cnptrset_create();
	o->threads = cnptrset_create();
	o->semaphores = cnptrset_create();
	o->tlses = cnptrset_create();


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
	printf( "++3\n" );

	OGUnlockMutex( tccinterfacemutex );
	printf( "++4\n" );
}

