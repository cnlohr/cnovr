
#if defined( __x86_64__ ) || defined( _M_AMD64 )
	#define TCC_TARGET_X86_64
#elif defined( __i386__ ) || defined( _M_IX86 )
	#define TCC_TARGET_I386
#endif

#define ONE_SOURCE 1

#include "tinycc/libtcc.c"

