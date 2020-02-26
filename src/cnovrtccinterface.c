#include <cnovrtccinterface.h>
#include <cnovrutil.h>
#include <cnovrcanvas.h>
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


#define MARKOGLockMutex( x )  OGLockMutex( x );
#define MARKOGUnlockMutex( x ) OGUnlockMutex( x );
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
	//void * tcccrashdata;
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
//	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag() );
//	if( c )
//		return c->tcccrashdata = malloc( size );
//	else
//		return malloc( size );
	return malloc( size );
}

void internal_tcc_crash_free( void * data )
{
//	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag() );
//	free( data );
//	if( c ) 
//	{
//		c->tcccrashdata = 0;
//	}
	free( data ); 
}

//This is a FINAL SHUTDOWN when the system is going down.  DO NOT call this unless you are totally ready for a shutdown.
void InternalBreakdownRestOfTCCInterface()
{
	MARKOGLockMutex( tccinterfacemutex );
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
			//if( o->tcccrashdata ) free( o->tcccrashdata ); //XXX TODO: This is temporarily a shim.  It can be simplified.
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
	tce->bClosing = 1;
	MARKOGLockMutex( tccinterfacemutex );
	object_cleanup * o = CNHashGetValue( objects_to_delete, tce );
	printf( "Shutting down TCC Instance %p (o=%p)\n", tce, o );
	if( o )
	{
		void * i;
		cnptrset_foreach( o->threads, i )
		{
			OGCancelThread( (og_thread_t)i );
		}
		cnptrset_destroy( o->threads );
		CNOVRFocusRemoveTag( tce );
		MARKOGUnlockMutex( tccinterfacemutex );
		CNOVRJobCancelAllTag( tce, 1 ); //XXX TODO XXX We need a way of cancelling the currently running operation so we CAN block.
		MARKOGLockMutex( tccinterfacemutex );
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
		//if( o->tcccrashdata ) free( o->tcccrashdata );
		free( o );
		CNHashDelete( objects_to_delete, tce );
	}
	else
	{
		CNOVRFocusRemoveTag( tce );
		MARKOGUnlockMutex( tccinterfacemutex );
		CNOVRJobCancelAllTag( tce, 1 ); //XXX TODO XXX We need a way of cancelling the currently running operation so we CAN block.
		MARKOGLockMutex( tccinterfacemutex );
		CNOVRListDeleteTCCTag( tce );
	}
	MARKOGUnlockMutex( tccinterfacemutex );
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

	printf( "Invocation check: %p\n", tp.tag );
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
	MARKOGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag() );
	if( c ) cnptrset_insert( c->threads, ret );
	MARKOGUnlockMutex( tccinterfacemutex );


	return ret;
}

void * TCCOGJoinThread( og_thread_t ot )
{
	og_thread_t ret = OGJoinThread( ot );
	MARKOGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_remove( c->threads, ot );
	MARKOGUnlockMutex( tccinterfacemutex );
	return ret;
}

void TCCOGCancelThread( og_thread_t ot )
{
	MARKOGLockMutex( tccinterfacemutex );
	OGCancelThread( ot );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_remove( c->threads, ot );
	MARKOGUnlockMutex( tccinterfacemutex );
}

og_mutex_t TCCOGCreateMutex()
{
	og_mutex_t ret = OGCreateMutex();
	MARKOGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_insert( c->mutices, ret );
	MARKOGUnlockMutex( tccinterfacemutex );
	return ret;
}

void TCCOGDeleteMutex( og_mutex_t om )
{
	OGDeleteMutex( om );
	MARKOGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_remove( c->mutices, om );
	MARKOGUnlockMutex( tccinterfacemutex );
}

og_sema_t TCCOGCreateSema()
{
	og_sema_t ret = OGCreateSema();
	MARKOGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_insert( c->semaphores, ret );
	MARKOGUnlockMutex( tccinterfacemutex );
	return ret;
}

void TCCOGDeleteSema( og_sema_t os )
{
	OGDeleteSema( os );
	MARKOGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_remove( c->semaphores, os );
	MARKOGUnlockMutex( tccinterfacemutex );
}

og_tls_t TCCOGCreateTLS()
{
	og_sema_t ret = OGCreateTLS();
	MARKOGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_insert( c->tlses, ret );
	MARKOGUnlockMutex( tccinterfacemutex );
	return ret;
}

static void TCCOGDeleteTLS( og_tls_t key )
{
	OGDeleteTLS( key );
	MARKOGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_remove( c->tlses, key );
	MARKOGUnlockMutex( tccinterfacemutex );
}

static cnovr_rf_buffer * TCCCNOVRRFBufferCreate( int w, int h, int multisample )
{
	cnovr_rf_buffer * ret = CNOVRRFBufferCreate( w, h, multisample );
	MARKOGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_insert( c->tccobjects, ret );
	MARKOGUnlockMutex( tccinterfacemutex );
	return ret;
}


static cnovr_shader * TCCCNOVRShaderCreateWithPrefix( const char * shaderfilebase, const char * prefix )
{
	cnovr_shader * ret = CNOVRShaderCreateWithPrefix( shaderfilebase, prefix );
	MARKOGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_insert( c->tccobjects, ret );
	MARKOGUnlockMutex( tccinterfacemutex );
	return ret;
}

static cnovr_shader * TCCCNOVRShaderCreate( const char * shaderfilebase )
{
	cnovr_shader * ret = CNOVRShaderCreate( shaderfilebase );
	MARKOGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_insert( c->tccobjects, ret );
	MARKOGUnlockMutex( tccinterfacemutex );
	return ret;
}

static cnovr_texture * TCCCNOVRTextureCreate( int w, int h, int chan )
{
	cnovr_texture * ret = CNOVRTextureCreate( w, h, chan );
	MARKOGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_insert( c->tccobjects, ret );
	MARKOGUnlockMutex( tccinterfacemutex );
	return ret;
}

static cnovr_simple_node * TCCCNOVRNodeCreateSimple( int reserved_size )
{
	cnovr_simple_node * ret = CNOVRNodeCreateSimple( reserved_size );
	MARKOGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_insert( c->tccobjects, ret );
	MARKOGUnlockMutex( tccinterfacemutex );
	return ret;
}

static cnovr_model * TCCCNOVRModelCreate( int initial_indices, int rendertype )
{
	cnovr_model * ret = CNOVRModelCreate( initial_indices, rendertype );
	MARKOGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_insert( c->tccobjects, ret );
	MARKOGUnlockMutex( tccinterfacemutex );
	return ret;
}

static cnovr_canvas * TCCCNOVRCanvasCreate( const char * name, int w, int h, int props )
{
	cnovr_canvas * ret = CNOVRCanvasCreate( name, w, h, props );
	MARKOGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_insert( c->tccobjects, ret );
	MARKOGUnlockMutex( tccinterfacemutex );
	return ret;
}

static void TCCCNOVRDeleteBase( cnovr_base * b )
{
	CNOVRDeleteBase( b );
	MARKOGLockMutex( tccinterfacemutex );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag()  );
	if( c ) cnptrset_remove( c->tccobjects, b );
	MARKOGUnlockMutex( tccinterfacemutex );
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

void TCCCNOVRFocusRespond( cnovrfocus_capture * ce, float realdistance, float * fdprops )
{
	ce->tag = TCCGetTag();
	CNOVRFocusRespond( ce, realdistance, fdprops );
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

char ** TCCCNOVRFolderListing( const char * folder, int * numentries )
{
	char ** ret = CNOVRFolderListing( folder, numentries );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag() );
	if( c ) cnptrset_insert( c->mallocedram, ret );
	return ret;
}

char ** TCCCNOVRSplitStrings( const char * line, char * split, char * white, int merge_fields, int * elementcount )
{
	char ** ret = CNOVRSplitStrings( line, split, white, merge_fields, elementcount );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag() );
	if( c ) cnptrset_insert( c->mallocedram, ret );
	return ret;
}

char * TCCCNOVRFileToString( const char * fname, int * length )
{
	char * ret = CNOVRFileToString( fname, length );
	object_cleanup * c = CNHashGetValue( objects_to_delete, TCCGetTag() );
	if( c ) cnptrset_insert( c->mallocedram, ret );
	return ret;
}

void * TCCGetTCCTag()
{
	return TCCGetTag();
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
#else
//Oddball things
extern void * CNFGDisplay;
extern void * CNFGWindow;
extern void * CNFGPixmap;
extern void * CNFGGC;	
extern void * CNFGWindowGC;
extern void * CNFGVisual;
void XShmCreateImage(); 
void XShmAttach();
void XShmGetImage();
void   CNFGSetVSync( int vson );
#endif

void CNFGSwapBuffers(void);

//#define TCCExport( x ) tcc_add_symbol( tce->state, #x, &TCC##x );
//#define TCCExportS( x ) tcc_add_symbol( tce->state, #x, &x );

#define TCCExport( x ) { #x, &TCC##x },
#define TCCExportS( x ) { #x, &x },


const struct ImportList ILSYMS[] = {
	TCCExport( realloc )
	TCCExport( malloc )
	TCCExport( calloc )
	TCCExport( free )
	TCCExport( strdup )
	TCCExportS( memcpy )
	TCCExportS( memset )
	TCCExport( strndup )

	TCCExportS( CNOVRAlertv )
	TCCExportS( CNOVRAlert )
	
	TCCExportS( puts )
	TCCExportS( printf )
	TCCExportS( sprintf )
	TCCExportS( snprintf )
	TCCExportS( tasprintf )
	TCCExportS( trprintf )

	TCCExportS( OGSleep )
	TCCExportS( OGUSleep )
	TCCExportS( OGGetAbsoluteTime )
	TCCExportS( OGGetFileTime )
	TCCExport( OGCreateThread )
	TCCExport( OGJoinThread )
	TCCExport( OGCancelThread )
	TCCExport( OGCreateMutex )
	TCCExportS( OGLockMutex )
	TCCExportS( OGUnlockMutex )
	TCCExport( OGDeleteMutex )
	TCCExport( OGCreateSema )
	TCCExportS( OGGetSema )
	TCCExportS( OGLockSema )
	TCCExportS( OGUnlockSema )
	TCCExport( OGDeleteSema )
	TCCExport( OGCreateTLS )
	TCCExport( OGDeleteTLS )
	TCCExportS( OGGetTLS )
	TCCExportS( OGSetTLS )
	TCCExport( GetTCCTag )
	TCCExportS( cnovrstate )
	TCCExportS( cnovr_current_shader )
	TCCExport( CNOVRNodeCreateSimple )
	TCCExportS( CNOVRVBOTaint )
	TCCExport( CNOVRModelCreate )
	TCCExportS( CNOVRModelSetNumVBOs )
	TCCExportS( CNOVRCreateVBO )
	TCCExportS( CNOVRModelTaintIndices )
	TCCExportS( CNOVRModelTackIndex )
	TCCExportS( CNOVRModelRenderWithPose )
	TCCExportS( CNOVRModelApplyTextureFromFileAsync )
	TCCExportS( CNOVRModelAppendMesh )
	TCCExportS( CNOVRModelLoadFromFileAsync )
	TCCExport( CNOVRTextureCreate )
	TCCExport( CNOVRShaderCreate )
	TCCExportS( CNOVRUniform )
	TCCExport( CNOVRShaderCreateWithPrefix )
	TCCExport( CNOVRDeleteBase )
	TCCExportS( CNOVRTextureLoadDataNow )
	TCCExportS( CNOVRTextureLoadFileAsync )
	TCCExportS( CNOVRModelAppendCube )
	TCCExportS( CNOVRModelCollide )
	TCCExportS( CNOVRGeneralHandleFocusEvent )
	TCCExportS( CNOVRFocusDefaultFocusEvent )
	TCCExportS( CNOVRFocusGetPropsForDev )
	TCCExportS( CNOVRModelSetInteractable )
	TCCExport( CNOVRCanvasCreate )
	TCCExportS( CNOVRCanvasResize )
	TCCExportS( CNOVRCanvasTackPixel )
	TCCExportS( CNOVRCanvasDrawText )
	TCCExportS( CNOVRCanvasTackSegment )
	TCCExportS( CNOVRCanvasDrawBox )
	TCCExportS( CNOVRCanvasTackRectangle )
	TCCExportS( CNOVRCanvasTackPoly )
	TCCExportS( CNOVRCanvasSwapBuffers )
	TCCExportS( CNOVRCanvasClearFrame )
	TCCExportS( CNOVRNodeAddObject )
	TCCExportS( CNOVRNodeRemoveObject )
	TCCExportS( CNOVRNamedPtrData )
	TCCExportS( CNOVRNamedPtrSave )
	TCCExportS( CNOVRNamedPtrRevert )
	TCCExportS( CNOVRNamedPtrGet )
	TCCExport( CNOVRFolderListing )
	TCCExport( CNOVRSplitStrings )
	TCCExport( CNOVRFileToString )
	TCCExport( CNOVRJobTack )
	TCCExport( CNOVRListAdd )
	TCCExport( CNOVRListDeleteTCCTag )
	TCCExportS( CNOVRListDeleteTag )
	TCCExport( CNOVRFocusRespond )
	TCCExport( CNOVRFocusAcquire )
	TCCExport( CNOVRFocusRemoveTag )
	TCCExportS( CNOVRFocusGetTipPose )
	TCCExport( CNOVRRFBufferCreate )
	TCCExportS( CNOVRFocusGetVRActionHandleFromConrollerAndCtrlA )
	TCCExportS( CNOVRGetTrackedDeviceString )
	TCCExportS( CNOVRListCall )
	TCCExportS( CNOVRFBufferActivate )
	TCCExportS( CNOVRFBufferBlitResolve )
	TCCExportS( CNOVRCanvasSetPhysicalSize )
	TCCExportS( CNOVRCanvasYFlip )
	TCCExportS( CNOVRCanvasApplyCannedGUI )
	TCCExportS( CNOVRCheck )
	TCCExportS( glActiveTextureCHEW )
	TCCExportS( cnovr_interpolate )
	TCCExportS( cross3d )
	TCCExportS( sub3d )
	TCCExportS( add3d )
	TCCExportS( mult3d )
	TCCExportS( scale3d )
	TCCExportS( invert3d )
	TCCExportS( mag3d )
	TCCExportS( normalize3d )
	TCCExportS( center3d )
	TCCExportS( mean3d )
	TCCExportS( dot3d )
	TCCExportS( compare3d )
	TCCExportS( copy3d )
	TCCExportS( magnitude3d )
	TCCExportS( dist3d )
	TCCExportS( anglebetween3d )
	TCCExportS( rotatearoundaxis )
	TCCExportS( angleaxisfrom2vect )
	TCCExportS( axisanglefromquat )
	TCCExportS( quatdist )
	TCCExportS( quatdifference )
	TCCExportS( quatset )
	TCCExportS( quatiszero )
	TCCExportS( quatsetnone )
	TCCExportS( quatcopy )
	TCCExportS( quatfromeuler )
	TCCExportS( quattoeuler )
	TCCExportS( quatfromaxisangle )
	TCCExportS( quatfromaxisanglemag )
	TCCExportS( quattoaxisanglemag )
	TCCExportS( quatmagnitude )
	TCCExportS( quatinvsqmagnitude )
	TCCExportS( quatnormalize )
	TCCExportS( quattomatrix )
	TCCExportS( quatfrommatrix )
	TCCExportS( quatgetconjugate )
	TCCExportS( findnearestaxisanglemag )
	TCCExportS( quatconjugateby )
	TCCExportS( quatgetreciprocal )
	TCCExportS( quatfind )
	TCCExportS( quatrotateabout )
	TCCExportS( quatmultiplyrotation )
	TCCExportS( quatscale )
	TCCExportS( quatdivs )
	TCCExportS( quatsub )
	TCCExportS( quatadd )
	TCCExportS( quatinnerproduct )
	TCCExportS( quatouterproduct )
	TCCExportS( quatevenproduct )
	TCCExportS( quatoddproduct )
	TCCExportS( quatslerp )
	TCCExportS( quatrotatevector )
	TCCExportS( eulerrotatevector )
	TCCExportS( quatfrom2vectors )
	TCCExportS( eulerfrom2vectors )
	TCCExportS( apply_pose_to_point )
	TCCExportS( apply_pose_to_pose )
	TCCExportS( unapply_pose_from_pose )
	TCCExportS( pose_invert )
	TCCExportS( pose_to_matrix44 )
	TCCExportS( matrix44_to_pose )
	TCCExportS( matrix44copy )
	TCCExportS( matrix44transposeself )
	TCCExportS( matrix44transposeunsafe )
	TCCExportS( matrix44identity )
	TCCExportS( matrix44zero )
	TCCExportS( matrix44translate )
	TCCExportS( matrix44scale )
	TCCExportS( matrix44rotateaa )
	TCCExportS( matrix44rotatequat )
	TCCExportS( matrix44rotateea )
	TCCExportS( matrix44multiply )
	TCCExportS( matrix44print )
	TCCExportS( matrix44perspective )
	TCCExportS( matrix44lookat )
	TCCExportS( matrix44ptransform )
	TCCExportS( matrix44vtransform )
	TCCExportS( matrix444transform )
	TCCExportS( matrix34multiply )

	TCCExportS( CNOVRPoseFromHMDMatrix )
	TCCExportS( CNOVRVBOTackv )
	TCCExportS( CNOVRModelSetNumVBOsWithStrides )

	TCCExportS( glGenBuffers )
	TCCExportS( glBindBuffer )
	TCCExportS( glMapBufferRange )
	TCCExportS( glUnmapBuffer )
	TCCExportS( glBufferData )
	TCCExportS( glMapBuffer )
	TCCExportS( glUniform4fv )
	TCCExportS( glUniform4f )
	TCCExportS( glUniform4fvCHEW )
	TCCExportS( glBindTexture )
	TCCExportS( glTexImage2D )
	TCCExportS( glGenerateMipmapCHEW )
	TCCExportS( glEnable )
	TCCExportS( glBlendFunc )
	TCCExportS( glPointSize )
	TCCExportS( glDepthFunc )
	TCCExportS( glLineWidth )
	TCCExportS( glTexParameteri )
	TCCExportS( glGetError )
	TCCExportS( glTexSubImage2D )
	TCCExportS( glDepthMask )
	TCCExportS( glHint )
	TCCExportS( glViewport )
	TCCExportS( glClearColor )
	TCCExportS( glClear )
	
	TCCExportS( glDisable )

	TCCExportS( CNFGSwapBuffers )

#if defined(WINDOWS) || defined( WIN32 ) || defined ( WIN64 )
	
	TCCExportS( _vsnprintf )
	TCCExportS( _vsnwprintf )

	TCCExportS( _stricmp )
	TCCExportS( _strnicmp )
	
	TCCExportS( memmove )
	TCCExportS( strstr )	
	TCCExportS( strcmp )
	TCCExportS( strncmp )
	TCCExportS( strlen )
	TCCExportS( strcpy )
	TCCExportS( snprintf )
	TCCExportS( sprintf )
	TCCExportS( atoi )

	TCCExportS( strncpy )
	TCCExportS( EnumWindows )
	TCCExportS( EnumDesktopWindows )
	TCCExportS( GetWindowThreadProcessId )
	TCCExportS( GetWindowTextA )
	TCCExportS( OpenProcess )
	TCCExportS( GetModuleFileNameA )

	TCCExportS( CloseHandle )
	TCCExportS( CreateCompatibleDC )
	TCCExportS( GetDC )
	TCCExportS( GetSystemMetrics )
	TCCExportS( CreateCompatibleBitmap )
	TCCExportS( SelectObject )
	TCCExportS( ReleaseDC )
	TCCExportS( DeleteObject )
	TCCExportS( DeleteDC )
	TCCExportS( GetWindowRect )
	TCCExportS( GetDesktopWindow )
	TCCExportS( BitBlt )
	TCCExportS( GetDIBits )
	TCCExportS( GetClassNameA )
	TCCExportS( GetCursorPos )

	TCCExport( _InterlockedExchangeAdd )
	TCCExport( _InterlockedExchangeAdd64 )
	TCCExport( _mul128 )
	TCCExport( __shiftright128 )
	TCCExport( _umul128 )
	TCCExport( __stosb )
	
	TCCExportS( _exit )
	TCCExportS( _atoi64 )
	TCCExportS( _ui64toa )
	TCCExportS( _i64toa )
	TCCExportS( _wtoi64 )
	TCCExportS( _i64tow )
	TCCExportS( _ui64tow )

	TCCExportS( _chgsign )
	TCCExportS( _copysign )

	TCCExportS( frexp )
	TCCExportS( ldexp )
	TCCExportS( hypot )
	TCCExportS( rand )
	TCCExportS( srand )

	TCCExportS( sin )
	TCCExportS( sinf )
	TCCExportS( cos )
	TCCExportS( cosf )
	TCCExportS( acos )
	TCCExportS( atan )
	TCCExport( ffloor )
	TCCExportS( floor )
	TCCExportS( floorf )
	TCCExportS( tan )
	TCCExportS( atan2 )
	TCCExportS( fmodf )
	{ "_findfirst64", FindFirstFileA },
	{ "_findnext64", FindNextFile },
#elif defined(ANDROID)
	//No OS-dependant exports here yet.
	
#else
	TCCExportS( CNFGSetVSync )

	TCCExportS( stat )
	TCCExportS( XShmCreateImage )
	TCCExportS( XShmAttach )
	TCCExportS( XShmGetImage )
	TCCExportS( CNFGDisplay )
	TCCExportS( CNFGWindow )
	TCCExportS( CNFGPixmap )
	TCCExportS( CNFGGC )
	TCCExportS( CNFGWindowGC )
	TCCExportS( CNFGVisual )
#endif
	{ 0, 0 }
};

void InternalPopulateTCC( TCCInstance * tce )
{
	MARKOGLockMutex( tccinterfacemutex );

#if defined(WINDOWS) || defined( WIN32 ) || defined ( WIN64 )
	//Not sure why this symbol doesn't actually link, so we have to do something wild toget the symbol.
	tcc_add_symbol( tce->state, "QueryFullProcessImageNameA", GetProcAddress( LoadLibrary("kernel32.dll"), "QueryFullProcessImageNameA" ) );
#endif

	int i;
	for( i = 0; i < sizeof(ILSYMS)/sizeof(ILSYMS[0]); i++ )
	{
		const struct ImportList * il = ILSYMS + i;
		if( il->SymName && il->SymPlace )
			tcc_add_symbol( tce->state, il->SymName, il->SymPlace );
	}
	 
	MARKOGUnlockMutex( tccinterfacemutex );
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

