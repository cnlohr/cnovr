#include <cnovrtccinterface.h>
#include <cnovrutil.h>
#include <libtcc.h>
#include <stdarg.h>
#include <cnovr.h>
#include <stdio.h>

og_tls_t tcctlstag;

#define TCCTag( v ) OGSetTLS( tcctlstag, v )

void InternalShutdownTCC( TCCState * tcc )
{
	CNOVRJobCancelAllTag( tcc, 1 );
	CNOVRListDeleteTag( tcc );
	//Do whatever else needs to be done to clean up here.
}

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
	if( !tcctlstag ) tcctlstag = OGCreateTLS();
	printf( "Attaching\n" );
	tcc_add_symbol( tcc, "printf", tccprintf );
}

