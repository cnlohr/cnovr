#include <cnovrtcc.h>

void init( const char * identifier )
{
	printf( "Example init %s\n", identifier );
}

void caller()
{
	//char * c = 0;
	//*c = 5;
}

void start( const char * identifier )
{
	printf( "Example start %s\n", identifier );
	caller();
}

void stop( const char * identifier )
{
	printf( "Example stop %s\n", identifier );
}

