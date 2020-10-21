#include <stdio.h>
#include <CNFG.h>
#include <cnovrtcc.h>
#include <cnovr.h>
#include <cnovrutil.h>

int main( int argc, char ** argv )
{
	if( CNOVRInit( "test", 0, 0, CNOVR_INIT_IS_OVERLAY ) )
	{
		fprintf( stderr, "Error: Could not init CNOVR.\n" );
		return -1;
	}

	CNOVRStartTCCSystem( (argc==2)?argv[1]:"example/example.json" );

	while(1)
	{
		CNOVRUpdate();
	}
}

