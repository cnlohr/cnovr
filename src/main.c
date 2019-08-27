#include <stdio.h>
#include <CNFGFunctions.h>
#include <cnovr.h>

int main()
{
	if( CNOVRInit( "test", 640, 480, 1 ) )
	{
		fprintf( stderr, "Error: Could not init CNOVR.\n" );
		return -1;
	}

	while(1)
	{
		CNOVRUpdate();
	}
}

