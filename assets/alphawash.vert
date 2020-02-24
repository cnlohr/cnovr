#version 430
#include "cnovr.glsl"

in vec3 positionin; //#MAPATTRIB position 0

void main()
{
	gl_Position = vec4(positionin.xyz,1.0);
}
