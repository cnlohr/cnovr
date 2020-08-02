#version AUTOVER
#include "cnovr.glsl"

in vec4 position; //#MAPATTRIB position 0
in vec4 normal;   //#MAPATTRIB normal 2

out vec3 localpos;
out vec3 start;
out vec3 rayin;

void main()
{
	gl_Position = /* umPerspective * umView  * */ umModel * vec4(position.xyz,1.0);
	gl_Position.z = .1;
	localpos = position.xyz;
	start = ( inverse( umView )* vec4( 0, 0, 0, 1 ) ).xyz;
	vec4 rayG = ( inverse( umView ) * inverse( umPerspective )  ) * vec4( gl_Position.xy, 1, 1 );
	rayin = rayG.xyz;
}
