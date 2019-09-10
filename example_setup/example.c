#include <cnovrtcc.h>


void * my_thread( void * v )
{
//	while(1)
//	{
//		printf( "THREADS!\n" );
//		OGUSleep(100000);
//	}
	return 0;
}

void init( const char * identifier )
{
	printf( "Example init %s\n", identifier );
}

void caller()
{
	//char * c = 0;
	//*c = 5;
//	OGCreateThread( my_thread, 0 );
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

