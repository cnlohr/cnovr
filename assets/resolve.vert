#version AUTOVER
#include "cnovr.glsl"

in vec4 position;    //#MAPATTRIB position 0
in vec2 texcoordsin; //#MAPATTRIB texcoordsin 1

out vec3 localpos;
out vec2 texcoords;

void main()
{
	gl_Position = vec4(position.xyz,1.0);
	localpos = position.xyz;
	texcoords = texcoordsin.xy;
}
