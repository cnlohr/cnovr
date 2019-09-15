#version 430
#include "cnovr.glsl"

layout(location = 0) in vec4 position;

varying vec3 localpos;

void main()
{
	gl_Position = umPerspective * umView * umModel * vec4(position.xyz,1.0);
	localpos = position.xyz;
}
