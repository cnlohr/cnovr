#version 430
#include "cnovr.glsl"

layout(location = 0) in vec4 position;
layout(location = 1) in vec2 texcoordsin;

out vec3 localpos;
out vec2 texcoords;

void main()
{
	gl_Position = vec4(position.xyz,1.0);
	localpos = position.xyz;
	texcoords = texcoordsin.xy;
}
