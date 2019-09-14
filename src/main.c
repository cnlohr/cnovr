#include <stdio.h>
#include <CNFGFunctions.h>
#include <cnovrtcc.h>
#include <cnovr.h>

int main()
{
	if( CNOVRInit( "test", 640, 480, 2 ) )
	{
		fprintf( stderr, "Error: Could not init CNOVR.\n" );
		return -1;
	}

	CNOVRStartTCCSystem( "example_setup/example.json" );

	cnovr_simple_node * root = cnovrstate->pRootNode;
	cnovr_model * model = CNOVRModelCreate( 0, 3, GL_TRIANGLES );
	CNOVRModelAppendCube( model, 1, 1, 1 );
	cnovr_shader * shader = CNOVRShaderCreate( "assets/basic" );
	CNOVRNodeAddObject( root, shader );
	CNOVRNodeAddObject( root, model );

	while(1)
	{
		CNOVRUpdate();
	}
}

