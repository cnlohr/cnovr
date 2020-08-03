#version AUTOVER
#include "cnovr.glsl"

out vec3 RayDirection;
out vec3 InitialCamera;
out vec2 PosInTex;

in vec4 position; //#MAPATTRIB position 0
in vec4 normal;   //#MAPATTRIB normal 2

out mat4 toss;
out vec4 BasePosition;

void main()
{
	gl_Position = umModel * vec4(position.xyz,1.0);
	gl_Position.z = .1; //Just to test  (not needed (I think))
	//localpos = position.xyz;
	InitialCamera = ( inverse( umView )* vec4( 0, 0, 0, 1 ) ).xyz;
	vec4 rayG = ( inverse( umView ) * inverse( umPerspective )  ) * vec4( gl_Position.xy, 1, 1 );
	RayDirection = rayG.xyz;
	PosInTex = position.xy*.5+.5;
//	PosInTex.y = 1.-PosInTex.y;

//	BasePosition = gl_MultiTexCoord0.xy;
	toss = umModel * umView;
//	toss = gl_ModelViewMatrix;

}
