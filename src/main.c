#include <stdio.h>
#include <CNFGFunctions.h>
#include <cnovr.h>

int main()
{
	if( CNOVRInit( "test", 640, 480, 1 ) )
	{
		fprintf( stderr, "Error: Could not init CNOVR.\n" );
		return -1;
	}

	cnovr_simple_node * root = cnovrstate->pRootNode;
	cnovr_model * model = CNOVRModelCreate( 0, 3, GL_TRIANGLES );
	CNOVRModelAppendCube( model, 1, 1, 1 );
	cnovr_shader * shader = CNOVRShaderCreate( "assets/basic" );
	CNOVRNodeAddObject( root, (cnovr_header*)shader );
	CNOVRNodeAddObject( root, (cnovr_header*)model );

	while(1)
	{
		CNOVRUpdate();
	}
}

