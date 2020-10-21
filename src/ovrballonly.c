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
	//cnovrstate->iMultisample = 0;
	OGSetTLS( tcctlstag, 0 );

	CNOVRFileSearchAddPath( "ovrball" );

	//openvrobjectsstart( "wireframe,nodetail" );
	ovrballstart( "ovrball" );
	while(1)
	{
		CNOVRUpdate();
	}
	//unreachable.
	//openvrobjectsstop( 0 );
	//ovrballstop( 0 );
}

