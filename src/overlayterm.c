#include <stdio.h>
#include <CNFG.h>
#include <cnovrtcc.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <cnovrtccinterface.h>

#include "../modules/overlayterm/overlayterm.c"

int main( int argc, char ** argv )
{
	if( CNOVRInit( "test", 0, 0, CNOVR_INIT_IS_OVERLAY ) )
	{
		fprintf( stderr, "Error: Could not init CNOVR.\n" );
		return -1;
	}
	OGSetTLS( tcctlstag, 0 );
	start( "myterm" );
	while(1)
	{
		CNOVRUpdate();
		OGUSleep(20000);
	}
}

