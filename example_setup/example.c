#include <cnovrtcc.h>

og_thread_t thdmax;

void * my_thread( void * v )
{
	while(1)
	{
		printf( "THREADS 8 %s %p\n", v, thdmax );
		OGUSleep(1000000);
	}
	return 0;
}

void init( const char * identifier )
{
	printf( "Example init %s\n", identifier );
}

void start( const char * identifier )
{
	thdmax = OGCreateThread( my_thread, (void*)identifier );
	printf( "Example start %s                   ++++++++++++++++++++%p %p\n", identifier, thdmax, &thdmax );
}

void stop( const char * identifier )
{
	//OGCancelThread( thdmax );
	printf( "Example stop %s                   ---------------------%p %p\n", identifier, thdmax, &thdmax );
}

