#version AUTOVER
#include "cnovr.glsl"


in vec4 position; //#MAPATTRIB position 0
in vec4 normal;   //#MAPATTRIB normal 2

out vec3 coord;

void main()
{
	gl_Position = umModel * vec4(position.xyz,1.0);
	gl_Position.z = .1; //Just to test  (not needed (I think))
	coord = position.xyz;
}
