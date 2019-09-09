#include <cnovrtcc.h>

void init( const char * identifier )
{
	printf( "Example init %s\n", identifier );
}

void start( const char * identifier )
{
	printf( "Example start %s\n", identifier );
	printf( "WOOT WOOT\n" );
	char * c = 0;
	*c = 5;
}

void stop( const char * identifier )
{
	printf( "Example stop %s\n", identifier );
}

