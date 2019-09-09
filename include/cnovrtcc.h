#ifndef _CNOVRTCC_H
#define _CNOVRTCC_H

#ifdef TCCINSTANCE

//You should implement these
void init( const char * id );	//called only once
void start( const char * id );	//called on start and after reload
void stop( const char * id );	//called on reload and stop

int printf( const char * format, ... );



#else

void CNOVRTCCLog( void * data, const char * tolog );
void CNOVRStartTCCSystem( const char * systemname );

#endif

#endif

