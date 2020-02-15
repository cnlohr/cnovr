#ifndef _CNOVR_TERMINAL_H
#define _CNOVR_TERMINAL_H

#include <vlinterm.h>
///////////////////////////////////////////////////////////////////////////////

typedef struct cnovr_terminal_t
{
	cnovr_header header;
	cnovr_model * model;
	struct TermStructure ts;
} cnovr_terminal;

//Terminal features XXX TODO Think about relationship between models and things like terminals.

	cnovr_header * assoc_dev;	//XXX TODO: Think about relationship between models and things like terminals.
cnovr_model * CNOVRTerminalCreate();

#endif

