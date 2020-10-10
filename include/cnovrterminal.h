#ifndef _CNOVRTERMINAL_H
#define _CNOVRTERMINAL_H

#include "cnovrcanvas.h"
#include "vlinterm.h"

typedef struct cnovr_terminal_t
{
	cnovr_base base;
	cnovr_canvas * canvas;
	struct TermStructure ts;
	char * title;
	int last_scrollback;
	int last_curx, last_cury;
	og_thread_t apprxthread;
	uint8_t tainted;
	uint8_t quit;
	int iUpdateNumber;
} cnovr_terminal;

cnovr_terminal * CNOVRTerminalCreate( const char * name, int cols, int rows );

void CNOVRTerminalRunCommand( cnovr_terminal * term, int argc, char ** argv );

//For writing directly to terminal output.
void CNOVRTerminalEmitStr( cnovr_terminal * term, const uint8_t * strs, int len );

//For writing to the app you are connected to.
void CNOVRTerminalFeedback( cnovr_terminal * term, const uint8_t * strs, int len );

void CNOVRTerminalUpdate( cnovr_terminal * term );
void CNOVRTerminalFlipTex( cnovr_terminal * term );

#endif

