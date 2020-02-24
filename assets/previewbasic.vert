#version 330
#include "cnovr.glsl"

in vec3 positionin;  //#MAPATTRIB positionin 0
in vec2 texcoordsin; //#MAPATTRIB texcoordsin 1

out vec2 texcoords;

void main()
{
	gl_Position = vec4(positionin.xyz,1.0);
	texcoords = texcoordsin.xy;
}
