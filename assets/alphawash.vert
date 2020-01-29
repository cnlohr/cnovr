#version 430
#include "cnovr.glsl"

layout(location = 0) in vec3 positionin;

void main()
{
	gl_Position = vec4(positionin.xyz,1.0);
}
