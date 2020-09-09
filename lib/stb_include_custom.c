#define STB_INCLUDE_IMPLEMENTATION
//If you're on an AMD card, this may be needed.
#define STB_INCLUDE_LINE_GLSL

#include <stdio.h>
#include <stddef.h>
#include "cnovrutil.h"

#define OVERRIDE_STB_INCLUDE_LOAD_FILE  stb_include_load_file_custom


static char * stb_include_load_file_custom(char *filename, size_t *plen)
{
	int len = 0;
	char * ret = CNOVRFileToString( filename, &len );
	*plen = len;
	return ret;
}

#include "stb_include_custom.h"
