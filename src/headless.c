#include <stdio.h>
#include <CNFG.h>
#include <cnovrtcc.h>
#include <cnovr.h>
#include <cnovrutil.h>

int main( int argc, char ** argv )
{
	if( CNOVRInit( "test", 0, 0, CNOVR_INIT_NEED_OPENVR ) )
	{
		fprintf( stderr, "Error: Could not init CNOVR.\n" );
		return -1;
	}

	CNOVRStartTCCSystem( (argc==2)?argv[1]:"example_setup/example.json" );
/*
	cnovr_simple_node * root = cnovrstate->pRootNode;
	cnovr_model * model = CNOVRModelCreate( 0, 3, GL_TRIANGLES );
	CNOVRModelAppendCube( model, 1, 1, 1 );
	cnovr_shader * shader = CNOVRShaderCreate( "assets/basic" );
	CNOVRNodeAddObject( root, shader );
	CNOVRNodeAddObject( root, model );
*/
	while(1)
	{
		CNOVRUpdate();
	}
}

