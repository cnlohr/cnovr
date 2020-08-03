#version AUTOVER
#include "cnovr.glsl"


in vec4 position; //#MAPATTRIB position 0
in vec4 normal;   //#MAPATTRIB normal 2


out vec3 RayDirection;
out vec3 InitialCamera;
out float maxdist;
out float doPhysics;
out vec3 AuxRotation;

void main()
{
	gl_Position = umModel * vec4(position.xyz,1.0);
	gl_Position.z = .1; //Just to test  (not needed (I think))
	//localpos = position.xyz;
	InitialCamera = ( inverse( umView )* vec4( 0, 0, 0, 1 ) ).xyz;
	vec4 rayG = ( inverse( umView ) * inverse( umPerspective )  ) * vec4( gl_Position.xy, 1, 1 );
	RayDirection = rayG.xyz;
	doPhysics = 0.;
	maxdist = 100.;
	AuxRotation = vec3( 0. );

#if 0
	AuxRotation = gl_MultiTexCoord1.xyz;

	if( gl_Color.a < 0. )
	{
		//We're in an override mode (used for physics, not regular stuff)
		InitialCamera = gl_Color.rgb;
		RayDirection = gl_Normal;
		doPhysics = 1.;
	}
	else
	{
		InitialCamera = -(( gl_ModelViewMatrixInverse * vec4(0.0,0.0,0.0,-1.0) )).xyz;
		RayDirection = vec3( ( gl_Vertex  ).x, ( gl_Vertex  ).y, -2.0 );
		RayDirection = -( ( gl_ModelViewMatrixInverse * vec4(-RayDirection,0.0) ) ).xyz;
		doPhysics = 0.0;
	}

	//Careful -- don't do any transforms or anything, we want this triangle to take p the whole view.
	gl_Position = (gl_MultiTexCoord0*2.0-1.0) * vec4( 1., -1., 1., 1. );
#endif

}
