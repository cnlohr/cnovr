#version 330
#include "cnovr.glsl"

layout(location = 0) in vec3 positionin;
layout(location = 1) in vec2 texcoordsin;

out vec2 texcoords;

void main()
{
	gl_Position = vec4(positionin.xyz,1.0);
	texcoords = texcoordsin.xy;
}
