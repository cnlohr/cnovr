#version 430
#include "cnovr.glsl"

layout(location = 0) in vec3 positionin;
layout(location = 1) in vec2 texcoordsin;
layout(location = 2) in vec3 normalin;

out vec2 texcoords;
out vec3 normal;
out vec3 position;

void main()
{
	gl_Position = vec4(positionin.xyz,1.0);
	texcoords = texcoordsin.xy;
	normal = normalin.xyz;
	position = positionin.xyz;
}
