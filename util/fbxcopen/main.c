#include "fbxcopen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void fbxcerror( fbxcopen * fbxc, fbxcopen_context * ctx, const char * error )
{
	printf( "Error in %p(%p) %p(%p): %s\n", fbxc, fbxc?fbxc->opaqueO:0, ctx, ctx?ctx->opaqueC:0, error );
}

int MyFBXCOpenLoadFn( fbxcopen_context * ctx, int cbtype, void * data, void * opaqueL )
{
	printf( "Got val: ctx %d %p\n", cbtype, opaqueL );
}


int main()
{
	fbxcopen * fbx = FBXCOpenFile(
		"testscene.fbx",
		fbxcerror,
		(void*)0x77 /*Sentinal to make sure opaque is preserved*/ );

	printf( "fbxcopen ptr: %p\n", fbx );

	if( !fbx ) return 0;

#if 0
	//Load a file.
	FBXCOpenLoadFromContext( fbx, MyFBXCOpenLoadFn, (void*)0x88 );
#endif

	//A normal enumeration, for dumping a section or the entire file.
#if 1
	fbxcopen_context myctx;
	FBXCOpenMakeContext( fbx, &myctx, 
		(void*)0x88 /*Sentinal to make sure opaque is preserved*/  );

	FBXCOpenContextPrintNodeInfo( &myctx, 0 );
#endif

#if 0
	//Manual Enumeration, this is completely superceded. Do not do things this way.
	//This is just a raw test.

	char ** enumeratePtrs;
	int * enumerateOffsets;
	int sections = FBXCOpenEnumerate( &myctx, &enumeratePtrs, &enumerateOffsets );
	printf( "Sections: %d\n", sections );
	int i;
	for( i = 0; i < sections; i++ )
	{
		char * name = enumeratePtrs[i];
		int offset = enumerateOffsets[i];

		printf( "%4d: %s @ %d\n", i, name, offset );
		//if( strcmp( name, "Objects") == 0 )
		{
			FBXCOpenResetContext( &myctx, offset );
			if( FBXCOpenContextGoToNestedList( &myctx ) == 0 )
			{
				char ** enumerateSubPtrs;
				int * enumerateSubOffsets;
				int subsections = FBXCOpenEnumerate( &myctx, &enumerateSubPtrs, &enumerateSubOffsets );
				int j;
				for( j = 0; j < subsections; j++ )
				{
					char * name = enumerateSubPtrs[j];
					int offset = enumerateSubOffsets[j];
					printf( "%7d: %s @ %d\n", j, name, offset );
					if( strcmp( name, "Model" ) == 0  || strcmp( name, "Geometry" ) == 0)
					{
						FBXCOpenResetContext( &myctx, offset );
						FBXCOpenContextPrintNodeInfo( &myctx );
						//Somehow read model??!?
					}
				}
				free( enumerateSubPtrs );
			}
		}
	}
	free( enumeratePtrs );
#endif

	return 0;
}

