#include <cnovrtccinterface.h>
#include <cnovrutil.h>
#include <cnovrfocus.h>
#include <tinycc/libtcc.h>
#include <stdarg.h>
#include <cnovr.h>
#include <stdio.h>
#include <cnhash.h>
#include <cnovrparts.h>
#include <string.h>
#include <cnrbtree.h>
#include <chew.h>

////////////////////////////////////////////////////////////////////////////////
//
// XXX TODO: Make a better mapping from TCCGetTag() to the cleanup object.
//

og_tls_t ogsafelocktls;

CNRBTREETEMPLATE( og_mutex_t, int, RBptrcmp, RBptrcpy, RBnullop );

void OGTSLockMutex( og_mutex_t m )
{
	OGLockMutex( m );
	cnrbtree_og_mutex_tint * tree = (cnrbtree_og_mutex_tint *)OGGetTLS( ogsafelocktls );
	if( !tree ) return;
	RBA( tree, m )++;
}

void OGTSUnlockMutex( og_mutex_t m )
{
	cnrbtree_og_mutex_tint * tree = (cnrbtree_og_mutex_tint *)OGGetTLS( ogsafelocktls );
	if( tree )
		RBA( tree, m )--;
	OGUnlockMutex( m );
}

//Called when we enter a thread.
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
		OGSetTLS( ogsafelocktls, cnrbtree_og_mutex_tint_create() );
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
	void * tcccrashdata;
} object_cleanup;

cnhashtable * objects_to_delete;

void InternalSetupTCCInterface()
{
	tccinterfacemutex = OGCreateMutex();
	tcctlstag = OGCreateTLS();
	objects_to_delete = CNHashGenerate( 0, 0, CNHASH_POINTERS);
}

void * internal_tcc_crash_malloc( int size )
{
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag() );
	if( c )
		return c->tcccrashdata = malloc( size );
	else
		return malloc( size );
}

void internal_tcc_crash_free( void * data )
{
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag() );
	free( data );
	if( c ) 
	{
		c->tcccrashdata = 0;
	}
}

//This is a FINAL SHUTDOWN when the system is going down.  DO NOT call this unless you are totally ready for a shutdown.
void InternalBreakdownRestOfTCCInterface()
{
	OGLockMutex( tccinterfacemutex );
	CNOVRJobCancelAllTag( 0, 0 );
	CNOVRListDeleteTCCTag( 0 );
	printf( "BREAKING DOWN REST OF TCC Interface\n" );
	int k;
	for( k = 0; k < objects_to_delete->array_size; k++ )
	{
		object_cleanup * o = (object_cleanup *)objects_to_delete->elements[k].data;
		TCCInstance * tce = (TCCInstance *)objects_to_delete->elements[k].key;
		if( tce ) tce->bClosing = 1;
		if( o )
		{
			void * i;
			cnptrset_foreach( o->threads, i )
			{
				OGCancelThread( (og_thread_t)i );
			}
			cnptrset_destroy( o->threads );

			CNOVRFocusRemoveTag( tce );
			CNOVRJobCancelAllTag( tce, 1 ); //XXX TODO XXX We need a way of cancelling the currently running operation so we CAN block.
			CNOVRListDeleteTCCTag( tce );


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
			if( o->tcccrashdata ) free( o->tcccrashdata ); //XXX TODO: This is temporarily a shim.  It can be simplified.
			free( o );
			CNHashDelete( objects_to_delete, tce );
		}
	}
	OGDeleteMutex( tccinterfacemutex );
	CNHashDestroy( objects_to_delete );
	objects_to_delete = 0;
}


void InternalShutdownTCC( TCCInstance * tce )
{
	printf( "Shutting down TCC Instance %p\n", tce );
	tce->bClosing = 1;
	OGLockMutex( tccinterfacemutex );
	object_cleanup * o = CNHashGetValue( objects_to_delete, tce );
	if( o )
	{
		void * i;
		cnptrset_foreach( o->threads, i )
		{
			OGCancelThread( (og_thread_t)i );
		}
		cnptrset_destroy( o->threads );
		CNOVRFocusRemoveTag( tce );
		CNOVRJobCancelAllTag( tce, 1 ); //XXX TODO XXX We need a way of cancelling the currently running operation so we CAN block.
		CNOVRListDeleteTCCTag( tce );
		cnptrset_foreach( o->tccobjects, i ) { CNOVRDeleteBase( ((cnovr_base*)i) ); }
		cnptrset_destroy( o->tccobjects );
		cnptrset_foreach( o->mutices, i ) OGDeleteMutex( (og_mutex_t)i );
		cnptrset_destroy( o->mutices );
		cnptrset_foreach( o->tlses, i ) OGDeleteTLS( (og_tls_t)i );
		cnptrset_destroy( o->tlses );
		cnptrset_foreach( o->semaphores, i ) OGDeleteSema( (og_sema_t)i );
		cnptrset_destroy( o->semaphores );
		cnptrset_foreach( o->mallocedram, i ) free( i );
		cnptrset_destroy( o->mallocedram );
		if( o->tcccrashdata ) free( o->tcccrashdata );
		free( o );
		CNHashDelete( objects_to_delete, tce );
	}
	else
	{
		CNOVRFocusRemoveTag( tce );
		CNOVRJobCancelAllTag( tce, 1 ); //XXX TODO XXX We need a way of cancelling the currently running operation so we CAN block.
		CNOVRListDeleteTCCTag( tce );
	}
	OGUnlockMutex( tccinterfacemutex );
	printf( "Shutdown complete.\n" );
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

static void TCCOGDeleteTLS( og_tls_t key )
{
	OGDeleteTLS( key );
	OGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_remove( c->tlses, key );
	OGUnlockMutex( tccinterfacemutex );
}

static cnovr_shader * TCCCNOVRShaderCreate( const char * shaderfilebase )
{
	cnovr_shader * ret = CNOVRShaderCreate( shaderfilebase );
	OGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_insert( c->tccobjects, ret );
	OGUnlockMutex( tccinterfacemutex );
	return ret;
}

static cnovr_texture * TCCCNOVRTextureCreate( int w, int h, int chan )
{
	cnovr_texture * ret = CNOVRTextureCreate( w, h, chan );
	OGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_insert( c->tccobjects, ret );
	OGUnlockMutex( tccinterfacemutex );
	return ret;
}

static cnovr_simple_node * TCCCNOVRNodeCreateSimple( int reserved_size )
{
	cnovr_simple_node * ret = CNOVRNodeCreateSimple( reserved_size );
	OGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_insert( c->tccobjects, ret );
	OGUnlockMutex( tccinterfacemutex );
	return ret;
}

static cnovr_model * TCCCNOVRModelCreate( int initial_indices, int num_vbos, int rendertype )
{
	cnovr_model * ret = CNOVRModelCreate( initial_indices, num_vbos, rendertype );
	OGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_insert( c->tccobjects, ret );
	OGUnlockMutex( tccinterfacemutex );
	return ret;
}

static void TCCCNOVRDeleteBase( cnovr_base * b )
{
	CNOVRDeleteBase( b );
	OGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_remove( c->tccobjects, b );
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


static void TCCCNOVRJobTack( cnovrQueueType q, cnovr_cb_fn fn, void * tag, void * opaquev, bool insert_even_if_pending )
{
	CNOVRJobTack( q, fn, TCCGetTag(), opaquev, insert_even_if_pending );
}

static void TCCCNOVRListAdd( cnovrRunList l, void * base_object, cnovr_cb_fn * fn )
{
	CNOVRListAdd( l, TCCGetTag(), fn );
}

void TCCCNOVRListDeleteTCCTag( void * tcctag )
{
	CNOVRListDeleteTCCTag( TCCGetTag() );
}



void *TCCmalloc(size_t size)
{
	void * ret = malloc( size );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag() );
	if( c )
	{
		cnptrset_insert( c->mallocedram, ret );
	}
	else
	{
		InternalInterfaceCreationDone( TCCGetTag() );
		object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag() );
		cnptrset_insert( c->mallocedram, ret );
	}
	return ret;
}

void TCCfree(void *ptr)
{	
	free( ptr );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag() );
	if( c ) cnptrset_remove( c->mallocedram, ptr );
}

void * TCCcalloc(size_t nmemb, size_t size)
{
	void * ret = calloc( nmemb, size );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag() );
	if( c ) cnptrset_insert( c->mallocedram, ret );
	return ret;
}

void * TCCrealloc(void *ptr, size_t size)
{
	void * ret = realloc( ptr, size );
	if( ptr != ret )
	{
		object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag() );
		if( c )
		{
			cnptrset_remove( c->mallocedram, ptr );
			cnptrset_insert( c->mallocedram, ret );
		}
	}
	return ret;
}

char * TCCstrndup(const char * str, size_t size)
{
	if( str == 0 ) return 0;
	int asize;
	for( asize = 0; asize < size; asize++ )
	{
		if( str[asize] == 0 ) break;
	}
	char * ret = malloc( asize );
	memcpy( ret, str, asize+1 );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag() );
	if( c ) cnptrset_insert( c->mallocedram, ret );
	return ret;
}

char * TCCstrdup(const char * str )
{
	char * ret = strdup( str );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag() );
	if( c ) cnptrset_insert( c->mallocedram, ret );
	return ret;
}

void TCCCNOVRFocusRespond( cnovrfocus_capture * ce, float realdistance )
{
	ce->tag = TCCGetTag();
	CNOVRFocusRespond( ce, realdistance );
}

void TCCCNOVRFocusAcquire( cnovrfocus_capture * ce, int wantfocus )
{
	ce->tag = TCCGetTag();
	CNOVRFocusAcquire( ce, wantfocus );
}

void TCCCNOVRFocusRemoveTag( void * tag )
{
	CNOVRFocusRemoveTag( TCCGetTag() );
}

#if defined( WINDOWS  ) || defined ( WIN32 ) || defined( WIN64 )
//XXX TODO: I think we'll need these maybe?
static void TCC_InterlockedExchangeAdd( ) { ovrprintf( "Unsupported function\n" );  }
static void TCC_InterlockedExchangeAdd64( ) { ovrprintf( "Unsupported function\n" ); }
static void TCC_mul128( ) { ovrprintf( "Unsupported function\n" ); }
static void TCC__shiftright128( ) { ovrprintf( "Unsupported function\n" ); }
static void TCC_umul128( ) { ovrprintf( "Unsupported function\n" ); }
static void TCC__stosb( ) { ovrprintf( "Unsupported function\n" ); }

static float TCCffloor( float x ) { return FLT_FLOOR(x); }

#endif

#define TCCExport( x ) tcc_add_symbol( tce->state, #x, &TCC##x );
#define TCCExportS( x ) tcc_add_symbol( tce->state, #x, &x );

void InternalPopulateTCC( TCCInstance * tce )
{
	printf( "InternalPopulateTCC { %p }\n", tce );
	OGLockMutex( tccinterfacemutex );

	TCCExport( realloc );
	TCCExport( malloc );
	TCCExport( calloc );
	TCCExport( free );
	TCCExport( strdup );
	TCCExport( strndup );

	TCCExportS( CNOVRAlertv );
	TCCExportS( CNOVRAlert );
	
	TCCExportS( puts );
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

//Oddball things
#if defined(WINDOWS) || defined( WIN32 ) || defined ( WIN64 )
#else
	extern void * CNFGDisplay;		TCCExportS( CNFGDisplay );
	extern void * CNFGWindow;		TCCExportS( CNFGWindow );
	extern void * CNFGPixmap;		TCCExportS( CNFGPixmap );
	extern void * CNFGGC;			TCCExportS( CNFGGC );
	extern void * CNFGWindowGC;		TCCExportS( CNFGWindowGC );
	extern void * CNFGVisual;		TCCExportS( CNFGVisual );

//X11
 	void XShmCreateImage(); 
	void XShmAttach();
	void XShmGetImage();
	TCCExportS( XShmCreateImage );
	TCCExportS( XShmAttach );
	TCCExportS( XShmGetImage );
//End X11
#endif


	TCCExportS( cnovrstate );

	TCCExport( CNOVRNodeCreateSimple );
	TCCExportS( CNOVRVBOTaint );
	TCCExport( CNOVRModelCreate );
	TCCExportS( CNOVRModelRenderWithPose );
	TCCExportS( CNOVRModelApplyTextureFromFileAsync );
	TCCExportS( CNOVRModelAppendMesh );
	TCCExport( CNOVRTextureCreate );
	TCCExport( CNOVRShaderCreate );
	TCCExport( CNOVRDeleteBase );
	TCCExportS( CNOVRTextureLoadDataNow );
	TCCExportS( CNOVRTextureLoadFileAsync );
	TCCExportS( CNOVRModelAppendCube );
	TCCExportS( CNOVRModelCollide );
	TCCExportS( CNOVRModelHandleFocusEvent );
	TCCExportS( CNOVRModelSetInteractable );
 
	TCCExportS( CNOVRNodeAddObject );
	TCCExportS( CNOVRNodeRemoveObject );

	TCCExportS( CNOVRNamedPtrData );
	TCCExportS( CNOVRNamedPtrSave );
	TCCExportS( CNOVRNamedPtrGet );

	TCCExport( CNOVRJobTack );
	TCCExport( CNOVRListAdd );
	TCCExport( CNOVRListDeleteTCCTag );

	TCCExportS( CNOVRListDeleteTag );

	TCCExport( CNOVRFocusRespond );
	TCCExport( CNOVRFocusAcquire );
	TCCExport( CNOVRFocusRemoveTag );
	TCCExportS( CNOVRFocusGetTipPose );

	TCCExportS( cnovr_interpolate );
	TCCExportS( cross3d );
	TCCExportS( sub3d );
	TCCExportS( add3d );
	TCCExportS( scale3d );
	TCCExportS( invert3d );
	TCCExportS( mag3d );
	TCCExportS( normalize3d );
	TCCExportS( center3d );
	TCCExportS( mean3d );
	TCCExportS( dot3d );
	TCCExportS( compare3d );
	TCCExportS( copy3d );
	TCCExportS( magnitude3d );
	TCCExportS( dist3d );
	TCCExportS( anglebetween3d );
	TCCExportS( rotatearoundaxis );
	TCCExportS( angleaxisfrom2vect );
	TCCExportS( axisanglefromquat );
	TCCExportS( quatdist );
	TCCExportS( quatdifference );
	TCCExportS( quatset );
	TCCExportS( quatiszero );
	TCCExportS( quatsetnone );
	TCCExportS( quatcopy );
	TCCExportS( quatfromeuler );
	TCCExportS( quattoeuler );
	TCCExportS( quatfromaxisangle );
	TCCExportS( quatfromaxisanglemag );
	TCCExportS( quattoaxisanglemag );
	TCCExportS( quatmagnitude );
	TCCExportS( quatinvsqmagnitude );
	TCCExportS( quatnormalize );
	TCCExportS( quattomatrix );
	TCCExportS( quatfrommatrix );
	TCCExportS( quatgetconjugate );
	TCCExportS( findnearestaxisanglemag );
	TCCExportS( quatconjugateby );
	TCCExportS( quatgetreciprocal );
	TCCExportS( quatfind );
	TCCExportS( quatrotateabout );
	TCCExportS( quatmultiplyrotation );
	TCCExportS( quatscale );
	TCCExportS( quatdivs );
	TCCExportS( quatsub );
	TCCExportS( quatadd );
	TCCExportS( quatinnerproduct );
	TCCExportS( quatouterproduct );
	TCCExportS( quatevenproduct );
	TCCExportS( quatoddproduct );
	TCCExportS( quatslerp );
	TCCExportS( quatrotatevector );
	TCCExportS( eulerrotatevector );
	TCCExportS( quatfrom2vectors );
	TCCExportS( eulerfrom2vectors );
	TCCExportS( apply_pose_to_point );
	TCCExportS( apply_pose_to_pose );
	TCCExportS( unapply_pose_from_pose );
	TCCExportS( pose_invert );
	TCCExportS( pose_to_matrix44 );
	TCCExportS( matrix44_to_pose );
	TCCExportS( matrix44copy );
	TCCExportS( matrix44transpose );
	TCCExportS( matrix44identity );
	TCCExportS( matrix44zero );
	TCCExportS( matrix44translate );
	TCCExportS( matrix44scale );
	TCCExportS( matrix44rotateaa );
	TCCExportS( matrix44rotatequat );
	TCCExportS( matrix44rotateea );
	TCCExportS( matrix44multiply );
	TCCExportS( matrix44print );
	TCCExportS( matrix44perspective );
	TCCExportS( matrix44lookat );
	TCCExportS( matrix44ptransform );
	TCCExportS( matrix44vtransform );
	TCCExportS( matrix444transform );

	TCCExportS( glGenBuffers );
	TCCExportS( glBindBuffer );
	TCCExportS( glMapBufferRange );
	TCCExportS( glUnmapBuffer );
	TCCExportS( glBufferData );
	TCCExportS( glMapBuffer );
	TCCExportS( glUniform4fv );
	TCCExportS( glBindTexture );
	TCCExportS( glTexImage2D );


#if defined(WINDOWS) || defined( WIN32 ) || defined ( WIN64 )

	TCCExportS( _stricmp );
	TCCExportS( _strnicmp );
	
	TCCExport( _InterlockedExchangeAdd );
	TCCExport( _InterlockedExchangeAdd64 );
	TCCExport( _mul128 );
	TCCExport( __shiftright128 );
	TCCExport( _umul128 );
	TCCExport( __stosb );
	
	TCCExportS( _exit );
	TCCExportS( _atoi64 );
	TCCExportS( _ui64toa );
	TCCExportS( _i64toa );
	TCCExportS( _wtoi64 );
	TCCExportS( _i64tow );
	TCCExportS( _ui64tow );

	TCCExportS( _chgsign );
	TCCExportS( _copysign );

	TCCExportS( frexp );
	TCCExportS( ldexp );
	TCCExportS( hypot );
	TCCExportS( rand );

	TCCExportS( sin );
	TCCExportS( cos );
	TCCExport( ffloor );
	TCCExportS( floor );
	TCCExportS( floorf );
	TCCExportS( tan );
	TCCExportS( atan2 );

#endif
	OGUnlockMutex( tccinterfacemutex );
	printf( "Populate done\n" );
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

