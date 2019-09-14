
#if defined( __x86_64__ ) || defined( _M_AMD64 )
	#define TCC_TARGET_X86_64
#elif defined( __i386__ ) || defined( _M_IX86 )
	#define TCC_TARGET_I386
#endif

#define TCC_IS_NATIVE

#if defined( WIN32 ) || defined( WIN64 ) || defined( WINDOWS )
#define TCC_TARGET_PE
#endif

#define ONE_SOURCE 1

#include "tinycc/libtcc.c"

