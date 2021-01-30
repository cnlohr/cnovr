#define CNFA_IMPLEMENTATION
#include "cnfa/cnfa.h"
#include "cnovrcnfa.h"
#include "cnovrutil.h"


int did_init_cnovrcnfa;
struct CNFADriver * CNOVRCNFA;

void CNOVRCNFACB( struct CNFADriver * sd, short * out, short * in, int framesp, int framesr )
{
	cnovr_audiodataset cs;
	if( out )
	{
		cs.numframes = framesp;
		cs.channels = sd->channelsPlay;
		cs.frames = out;
		cs.cb_no = 0;
		memset( out, 0, sd->channelsPlay * 2 * framesp );
		CNOVRListCall( cnovrLAudioPlay, &cs );
	}
	if( in )
	{
		cs.numframes = framesr;
		cs.channels = sd->channelsRec;
		cs.frames = in;
		cs.cb_no = 0;
		CNOVRListCall( cnovrLAudioRec, &cs );
	}
}

void CNOVRCNFAInit()
{
	if( did_init_cnovrcnfa ) return;
#ifdef __TINYC__
#if defined( WIN32 ) || defined( WINDOWS )
	REGISTERWinCNFA();
//	REGISTERcnfa_wasapi();
#endif
#endif	

	did_init_cnovrcnfa = 1;
	CNOVRCNFA = CNFAInit( 
#if defined( WIN32 ) || defined( WINDOWS )
		"WIN" //We currently need WinMM on Windows.
#else
		0
#endif
	, "cnovr", CNOVRCNFACB, 48000, 48000, 2, 2, 768, 0, 0, 0 );
}

