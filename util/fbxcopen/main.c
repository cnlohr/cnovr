#include "fbxcopen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void fbxcerror( fbxcopen * fbxc, fbxcopen_context * ctx, const char * error )
{
	printf( "Error in %p(%p) %p(%p): %s\n", fbxc, fbxc?fbxc->opaque:0, ctx, ctx?ctx->opaque:0, error );
}

int main()
{
	fbxcopen * fbx = FBXCOpenFile(
		"monkey-and-box.fbx",
		fbxcerror,
		(void*)0x77 /*Sentinal to make sure opaque is preserved*/ );

	printf( "fbxcopen ptr: %p\n", fbx );

	if( !fbx ) return 0;


	fbxcopen_context myctx;
	FBXCOpenMakeContext( fbx, &myctx, 
		(void*)0x88 /*Sentinal to make sure opaque is preserved*/  );

	FBXCOpenContextPrintNodeInfo( &myctx, 0 );

#if 0
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

