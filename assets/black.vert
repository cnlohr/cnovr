#version 430
#include "cnovr.glsl"

layout(location = 0) in vec4 position;
layout(location = 2) in vec4 normal;

varying vec3 localpos;

void main()
{
	gl_Position = umPerspective * umView * umModel * vec4(position.xyz,1.0);
	localpos = vec3(0.);
}
