#include <stdio.h>
#include <CNFG.h>
#include <cnovrtcc.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <cnovrtccinterface.h>

#include "../projects/ovrball/ovrball.c"

int main( int argc, char ** argv )
{
	if( CNOVRInit( "test", 0, 0, CNOVR_INIT_DISABLE_MULTISAMPLE | CNOVR_INIT_NEED_OPENVR ) )
	{
		fprintf( stderr, "Error: Could not init CNOVR.\n" );
		return -1;
	}
	OGSetTLS( tcctlstag, 0 );
	start( "ovrball" );
	while(1)
	{
		CNOVRUpdate();
	}
}

