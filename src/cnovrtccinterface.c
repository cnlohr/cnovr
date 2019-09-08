#include <cnovrtccinterface.h>
#include <cnovrutil.h>
#include <libtcc.h>

void InternalShutdownTCC( TCCState * tcc )
{
	CNOVRJobCancelAllTag( tcc, 1 );
	CNOVRListDeleteTag( tcc );

	//Do whatever else needs to be done to clean up here.
}

void InternalPopulateTCC( TCCState * tcc )
{
}

